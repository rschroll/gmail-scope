/* Copyright 2014 Robert Schroll
 *
 * This file is part of Gmail Scope and is distributed under the terms of
 * the GPL. See the file LICENSE for full details.
 */

#include <api/client.h>
#include <trojita/Encoders.h>

#include <core/net/error.h>
#include <core/net/http/client.h>
#include <core/net/http/content_type.h>
#include <core/net/http/response.h>
#include <QVariantMap>
#include <QRegularExpression>
#include <QCryptographicHash>
#include <QDateTime>

#include <iostream>

namespace http = core::net::http;
namespace net = core::net;

using namespace api;

/**
 * Utility functions for parsing the JSON response
 */
namespace {

static QString unescape(QString input) {
    return input.replace("&quot;", "\"").replace("&#39;", "'").replace("&gt;", ">")
            .replace("&lt;", "<").replace("&amp;", "&");
}

static QString parse_time(QString input) {
    QDateTime email = QDateTime::fromString(input, Qt::RFC2822Date).toLocalTime();
    if (!email.isValid())
        return input;
    return email.toString(TIME_FMT.c_str());
}

static Client::Contact parse_contact(const QString &contact_string) {
    Client::Contact contact;
    QString address;
    QRegularExpression regex("\"?(.*?)\"? <(.*)>");
    QRegularExpressionMatch match = regex.match(contact_string);
    if (match.hasMatch()) {
        contact.name = match.captured(1).toStdString();
        address = match.captured(2);
    } else {
        contact.name = contact_string.toStdString();
        address = contact_string;
    }
    contact.address = address.toStdString();
    std::string hash = QCryptographicHash::hash(address.trimmed().toLower().toUtf8(),
                                                QCryptographicHash::Algorithm::Md5).toHex().constData();
    contact.gravatar = "https://secure.gravatar.com/avatar/" + hash + "?d=identicon";
    return contact;
}

static Client::ContactList parse_contact_list(const QString &contact_string) {
    Client::ContactList contacts;
    for (const QString &contact : contact_string.split(", "))
        contacts.emplace_back(parse_contact(contact));
    return contacts;
}

static Client::Header parse_header(const QVariant &headers) {
    QVariantList header_list = headers.toList();
    Client::Header header;
    for (const QVariant &i : header_list) {
        QVariantMap item = i.toMap();
        std::string name = item["name"].toString().toStdString();
        QString value = item["value"].toString();

        if (name == "Date")
            header.date = parse_time(value).toStdString();
        else if (name == "From")
            header.from = parse_contact(value);
        else if (name == "To")
            header.to = parse_contact_list(value);
        else if (name == "Cc")
            header.cc = parse_contact_list(value);
        else if (name == "Reply-To")
            header.replyto = parse_contact(value);
        else if (name == "Subject")
            header.subject = value.toStdString();
        else if (name == "Message-ID" || name == "Message-Id")
            header.messageId = value.toStdString();
    }
    return header;
}

static std::string decode(const QVariant &encoded) {
    QByteArray decoded = QByteArray::fromBase64(encoded.toByteArray(), QByteArray::Base64UrlEncoding);
    QList<QByteArray> lines = decoded.replace("\r\n", "\n").split('\n');
    std::stringstream ss;
    bool continued = false;
    int quote_level = 0;
    const std::string colors[] = { "#9a5d9a", "#7474a7", "#3f8c8c", "#4c914c", "#818115", "#9f6666" };
    for (QByteArray &line : lines) {
        int i = 0;
        while (i < line.length() && line.at(i) == '>')
            i += 1;
        if (continued && quote_level != i)
            ss << "<br>";
        while (quote_level < i) {
            ss << "<font color='" << colors[quote_level % 6] << "'>";
            quote_level += 1;
        }
        while (quote_level > i) {
            ss << "</font>";
            quote_level -= 1;
        }
        if (i < line.length() && line.at(i) == ' ')
            i += 1;
        ss << line.remove(0, i).constData();
        continued = (line.endsWith(" ") && line != "-- ");
        if (!continued)
            ss << "<br>";
    }
    while (quote_level > 0) {
        ss << "</font>";
        quote_level -= 1;
    }

    // Remove extra blank lines from end
    std::string value = ss.str();
    size_t n = value.length();
    while (n > 4 && value.substr(n - 4, 4) == "<br>")
        n -= 4;
    return value.substr(0, n);
}

static std::string parse_payload(const QVariant &p) {
    QVariantMap payload = p.toMap();
    if (payload["mimeType"].toString().startsWith("multipart")) {
        QVariantList parts = payload["parts"].toList();
        for (const QVariant &part : parts) {
            std::string body = parse_payload(part);
            if (body != "")
                return body;
        }
    } else if (payload["mimeType"] == "text/plain") {
        return decode(payload["body"].toMap()["data"]);
    }
    return "";
}

static Client::Labels parse_labels(const QVariant &l) {
    QVariantList labelids = l.toList();
    Client::Labels labels;
    for (const QVariant &i : labelids)
        labels.emplace_back(i.toString().toStdString());
    return labels;
}

static Client::Email parse_email(const QVariant &i) {
    QVariantMap item = i.toMap();
    Client::Email message;
    message.id = item["id"].toString().toStdString();
    message.threadId = item["threadId"].toString().toStdString();
    message.snippet = unescape(item["snippet"].toString()).toStdString();
    message.header = parse_header(item["payload"].toMap()["headers"]);
    message.body = parse_payload(item["payload"]);
    message.labels = parse_labels(item["labelIds"]);
    return message;
}

static net::Uri::QueryParameters metadata_params() {
    net::Uri::QueryParameters params = { { "format", "metadata" } };
    for (std::string header : { "Date", "From", "To", "Cc", "Reply-To", "Subject", "Message-ID", "Message-Id" })
        params.emplace_back("metadataHeaders", header);
    return params;
}

/**
 * Utilities for constructing an RFC822 message
 */
static std::string encode_rfc2074(const std::string& input) {
    return Imap::encodeRFC2047StringWithAsciiPrefix(QString(input.c_str())).constData();
}

static std::string rfc822_address(const Client::Contact& contact) {
    if (contact.name == contact.address || contact.name == "")
        return contact.address;
    std::string name = contact.name;
    if (name.find(',') != std::string::npos)
        name = "\"" + name + "\"";
    return encode_rfc2074(name) + " <" + contact.address + ">";
}

void add_rfc822_header(QByteArray& message, const std::string& line) {
    // Gmail will wrap long header lines for us.
    message.append(line.c_str());
    message.append("\r\n");
}

void end_rfc822_header(QByteArray& message, const std::string& user_agent) {
    add_rfc822_header(message, "X-Mailer: " + user_agent);
    add_rfc822_header(message, "Content-Type: text/plain; charset=utf-8; format=flowed");
    add_rfc822_header(message, "Content-Transfer-Encoding: quoted-printable");
    message.append("\r\n");
}

void add_rfc822_body(QByteArray& message, const std::string body){
    message.append(Imap::quotedPrintableEncode(Imap::wrapFormatFlowed(body.c_str()).toUtf8()));
}
}


/**
 * Client class
 */
Client::Client(Config::Ptr config) :
    config_(config), cancelled_(false) {
}

void Client::get(const net::Uri::Path &path,
                 const net::Uri::QueryParameters &parameters, QJsonDocument &root) {
    // Create a new HTTP client
    auto client = http::make_client();

    // Start building the request configuration
    http::Request::Configuration configuration;

    // Build the URI from its components
    net::Uri uri = net::make_uri(config_->apiroot, path, parameters);
    configuration.uri = client->uri_to_string(uri);

    if (!config_->authenticated) {
        std::cerr << "Not authenticated!" << std::endl;
        return;
    }

    configuration.header.add("Authorization", "Bearer " + config_->access_token);
    configuration.header.add("User-Agent", config_->user_agent);

    // Build a HTTP request object from our configuration
    auto request = client->head(configuration);

    try {
        // Synchronously make the HTTP request
        // We bind the cancellable callback to #progress_report
        auto response = request->execute(
                    std::bind(&Client::progress_report, this, std::placeholders::_1));

        std::cerr << configuration.uri << std::endl;

        // Check that we got a sensible HTTP status code
        if (response.status != http::Status::ok) {
            throw std::domain_error(response.body);
        }
        // Parse the JSON from the response
        root = QJsonDocument::fromJson(response.body.c_str());

    } catch (net::Error &) {
    }
}

void Client::post(const net::Uri::Path& path, const net::Uri::QueryParameters& parameters,
                  const std::string& payload, QJsonDocument& root) {
    // Create a new HTTP client
    auto client = http::make_client();

    // Start building the request configuration
    http::Request::Configuration configuration;

    // Build the URI from its components
    net::Uri uri = net::make_uri(config_->apiroot, path, parameters);
    configuration.uri = client->uri_to_string(uri);

    if (!config_->authenticated) {
        std::cerr << "Not authenticated!" << std::endl;
        return;
    }

    configuration.header.add("Authorization", "Bearer " + config_->access_token);
    configuration.header.add("User-Agent", config_->user_agent);
    configuration.header.add("Content-Type", "application/json");

    // Build a HTTP request object from our configuration
    auto request = client->post(configuration, payload, "application/json");

    try {
        // Synchronously make the HTTP request
        // We bind the cancellable callback to #progress_report
        auto response = request->execute(
                    std::bind(&Client::progress_report, this, std::placeholders::_1));

        std::cerr << configuration.uri << std::endl;

        // Check that we got a sensible HTTP status code
        if (response.status != http::Status::ok) {
            throw std::domain_error(response.body);
        }
        // Parse the JSON from the response
        root = QJsonDocument::fromJson(response.body.c_str());

    } catch (net::Error &) {
    }
}

Client::EmailList Client::messages_list(const std::string& query, const std::string& label_id) {
    QJsonDocument root;
    net::Uri::QueryParameters params = { { "q", query }, { "maxResults", "50" } };
    if (label_id != "")
        params.emplace_back("labelIds", label_id);
    get( { "users", "me", "messages" }, params, root);

    EmailList result;
    QVariantMap variant = root.toVariant().toMap();
    QVariantList messages = variant["messages"].toList();

    for (const QVariant &i : messages) {
        result.emplace_back(parse_email(i));
    }
    return result;
}

Client::Email Client::messages_get(const std::string& id, bool body = false) {
    QJsonDocument root;
    net::Uri::QueryParameters params;
    if (body) {
        params = { { "format", "full" }, { "fields", "payload,labelIds" } };
    } else {
        params = metadata_params();
    }
    get({ "users", "me", "messages", id}, params, root);

    QVariant message = root.toVariant();
    return parse_email(message);
}

Client::EmailList Client::messages_get_batch(const EmailList& messages) {
    Client::EmailList result;

    // Create a new HTTP client
    auto client = http::make_client();

    // Start building the request configuration
    http::Request::Configuration configuration;
    configuration.uri = "https://www.googleapis.com/batch";

    if (!config_->authenticated) {
        std::cerr << "Not authenticated!" << std::endl;
        return result;
    }

    std::string boundary = "batch_boundary_fnord";
    configuration.header.add("Authorization", "Bearer " + config_->access_token);
    configuration.header.add("User-Agent", config_->user_agent);
    configuration.header.add("Content-Type", "multipart/mixed; boundary=" + boundary);

    std::stringstream ss;
    int i = 0;
    for (const Client::Email& message : messages) {
        std::string id = message.id;
        ss << "--" << boundary << "\n";
        ss << "Content-Type: application/http\n";
        ss << "Content-ID: <" << i << ":" << id << "@rschroll.developer.ubuntu.com>\n\n";
        net::Uri uri = net::make_uri("/gmail/v1", { "users", "me", "messages", id }, metadata_params());
        ss << "GET " << client->uri_to_string(uri) << "\n\n";
        i += 1;
    }
    ss << "--" << boundary << "--\n";

    // Build a HTTP request object from our configuration
    auto request = client->post(configuration, ss.str(), "multipart/mixed");

    try {
        // Synchronously make the HTTP request
        // We bind the cancellable callback to #progress_report
        auto response = request->execute(
                    std::bind(&Client::progress_report, this, std::placeholders::_1));

        std::cerr << configuration.uri << std::endl;

        // Check that we got a sensible HTTP status code
        if (response.status != http::Status::ok) {
            throw std::domain_error(response.body);
        }

        QString response_body(response.body.c_str());
        // Is there no way to inspect the header?  We assume that the first line is a boundary marker.
        QString boundary = response_body.section("\r\n", 0, 0);
        QStringList response_parts = response_body.split(boundary);
        // We assume the parts come back in the same order.  This isn't guaranteed.  We could look
        // at the Content-ID of each part, in which is embedded the index of the original message.
        // But for right now, this seems to work.
        for (const QString part : response_parts) {
            QString payload = part.section("\r\n\r\n", 2, -1);
            if (payload != "") {
                QJsonDocument root = QJsonDocument::fromJson(payload.toUtf8());
                result.emplace_back(parse_email(root.toVariant()));
            }
        }

    } catch (net::Error &) {
    }
    return result;
}

Client::Email Client::messages_set_unread(const std::string& id, bool unread) {
    std::string command = unread ? "addLabelIds" : "removeLabelIds";
    std::string payload = "{ \"" + command + "\": [\"UNREAD\"] }";
    QJsonDocument root;
    post({ "users", "me", "messages", id, "modify" }, {}, payload, root);
    return parse_email(root.toVariant());
}

Client::Email Client::messages_trash(const std::string& id) {
    QJsonDocument root;
    post({ "users", "me", "messages", id, "trash" }, {}, "", root);
    return parse_email(root.toVariant());
}

Client::Email Client::messages_untrash(const std::string& id) {
    QJsonDocument root;
    post({ "users", "me", "messages", id, "untrash" }, {}, "", root);
    return parse_email(root.toVariant());
}

Client::EmailList Client::threads_get(const std::string& id) {
    QJsonDocument root;
    get({ "users", "me", "threads", id }, metadata_params(), root);

    EmailList result;
    QVariantMap variant = root.toVariant().toMap();
    QVariantList messages = variant["messages"].toList();

    for (const QVariant &i : messages) {
        result.emplace_back(parse_email(i));
    }
    return result;
}

Client::Email Client::send_message(const Client::Contact& to, const std::string& subject,
                                   const std::string& body, const std::string& from_name,
                                   const std::string& ref_id, const std::string& thread_id) {
    QByteArray message;
    Contact from;
    from.address = users_address();
    from.name = from_name;

    add_rfc822_header(message, "In-Reply-To: " + ref_id);
    add_rfc822_header(message, "References: " + ref_id);
    if (from.address != "")
        add_rfc822_header(message, "From: " + rfc822_address(from));
    // Otherwise, Google will add a From header for us.
    add_rfc822_header(message, "To: " + rfc822_address(to));
    add_rfc822_header(message, "Subject: " + encode_rfc2074(subject));
    end_rfc822_header(message, config_->user_agent);
    add_rfc822_body(message, body);

    std::string request_body = "{ \"raw\": \"" +
            std::string(message.toBase64(QByteArray::Base64UrlEncoding).constData()) +
            "\", \"threadId\": \"" + thread_id + "\" }";
    std::cerr << request_body << std::endl;
    QJsonDocument root;
    post({ "users", "me", "messages", "send" }, {}, request_body, root);
    return parse_email(root.toVariant());
}

std::string Client::users_address() {
    if (config_->users_address.empty()) {
        QJsonDocument root;
        get({ "users", "me", "profile" }, {}, root);
        config_->users_address = root.toVariant().toMap()["emailAddress"].toString().toStdString();
    }
    return config_->users_address;
}

Client::LabelList Client::get_labels() {
    if (config_->labels.empty()) {
        QJsonDocument root;
        get({ "users", "me", "labels" }, {}, root);

        LabelList::iterator iter;
        QVariantMap variant = root.toVariant().toMap();
        QVariantList labels = variant["labels"].toList();

        for (const QVariant &i : labels) {
            QVariantMap label_map = i.toMap();
            if (label_map["messageListVisibility"] == "show") {
                iter = config_->labels.begin();
                std::string name = label_map["name"].toString().toStdString();
                // Sort by names, case insensitively
                while (iter != config_->labels.end() &&
                       strcasecmp(iter->second.c_str(), name.c_str()) <= 0)
                    iter += 1;
                config_->labels.insert(iter,
                                       std::make_pair(label_map["id"].toString().toStdString(),
                                       name));
            }
        }
    }
    return config_->labels;
}

bool Client::authenticated(unity::scopes::OnlineAccountClient& oa_client) {
    if (config_->authenticated)
        return true;

    for (auto const& status : oa_client.get_service_statuses()) {
        if (status.service_authenticated) {
            config_->authenticated = true;
            config_->access_token = status.access_token;
            config_->client_id = status.client_id;
            config_->client_secret = status.client_secret;
            break;
        }
    }
    return config_->authenticated;
}

http::Request::Progress::Next Client::progress_report(
        const http::Request::Progress&) {

    return cancelled_ ?
                http::Request::Progress::Next::abort_operation :
                http::Request::Progress::Next::continue_operation;
}

void Client::cancel() {
    cancelled_ = true;
}

Config::Ptr Client::config() {
    return config_;
}
