/* Copyright 2014 Robert Schroll
 *
 * This file is part of Gmail Scope and is distributed under the terms of
 * the GPL. See the file LICENSE for full details.
 */

#include <api/client.h>

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
            .replace("&lt;", "<");
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
        else if (name == "Subject")
            header.subject = value.toStdString();
    }
    return header;
}

static std::string decode(const QVariant &encoded) {
    // There is an alternate encoding of "62" and "63".
    QByteArray decoded = QByteArray::fromBase64(encoded.toByteArray().replace("-", "+")
                                                .replace("_", "/"));
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
    return ss.str();
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
    for (std::string header : { "Date", "From", "To", "Cc", "Subject" })
        params.emplace_back("metadataHeaders", header);
    return params;
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

Client::EmailList Client::messages_list(const std::string& query) {
    QJsonDocument root;
    get( { "users", "me", "messages" }, { { "q", query }, { "maxResults", "50" } }, root);

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
        params = { { "format", "full" }, { "fields", "payload" } };
    } else {
        params = metadata_params();
    }
    get({ "users", "me", "messages", id}, params, root);

    QVariant message = root.toVariant();
    return parse_email(message);
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
