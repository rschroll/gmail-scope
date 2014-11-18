/* Copyright 2014 Robert Schroll
 *
 * This file is part of Gmail Scope and is distributed under the terms of
 * the GPL. See the file LICENSE for full details.
 */

#include <boost/algorithm/string/trim.hpp>

#include <scope/localization.h>
#include <scope/query.h>
#include <scope/svg.h>

#include <unity/scopes/Annotation.h>
#include <unity/scopes/CategorisedResult.h>
#include <unity/scopes/CategoryRenderer.h>
#include <unity/scopes/QueryBase.h>
#include <unity/scopes/SearchReply.h>
#include <unity/scopes/OnlineAccountClient.h>
#include <unity/scopes/Department.h>

#include <QDateTime>
#include <QRegularExpression>

#include <iomanip>
#include <sstream>

namespace sc = unity::scopes;
namespace alg = boost::algorithm;

using namespace scope;


/**
 * Templates for displaying results
 */
const static std::string MESSAGE_TEMPLATE =
        R"(
{
        "schema-version": 1,
        "template": {
        "category-layout": "vertical-journal",
        "card-layout": "horizontal",
        "card-size": "38"
        },
        "components": {
        "title": "title",
        "subtitle": "subject",
        "summary": "snippet",
        "mascot": "gravatar",
        "emblem": "emblem"
        }
        }
        )";

const static std::string THREADED_TEMPLATE =
        R"(
{
        "schema-version": 1,
        "template": {
        "category-layout": "vertical-journal",
        "card-layout": "horizontal",
        "card-size": "38"
        },
        "components": {
        "title": "title",
        "subtitle": "snippet",
        "mascot": "gravatar",
        "emblem": "emblem"
        }
        }
        )";

const static std::string LOGIN_TEMPLATE =
        R"(
{
        "schema-version": 1,
        "template": {
        "category-layout": "grid",
        "card-layout": "vertical",
        "card-size": "large",
        "card-background": "color:///white"
        },
        "components": {
        "title": "title",
        "mascot": "art"
        }
        }
        )";

/**
 * Utility functions for displaying information about messages
 */
namespace {

static std::string trim_subject(const std::string line) {
    QRegularExpression regex("^ *(?:(?:fwd?|re):? *)*(.*?) *$",
                             QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch match = regex.match(line.c_str());
    if (match.hasMatch())
        return match.captured(1).toStdString();
    return line;
}

static std::string short_date(std::string input) {
    QDateTime email = QDateTime::fromString(input.c_str(), api::TIME_FMT.c_str());
    if (!email.isValid())
        return input;
    QDateTime now = QDateTime::currentDateTime();
    if (now.date() == email.date())
        return email.toString("HH:mm").toStdString();
    if (email.daysTo(now) < 7)
        return email.toString("dddd").toStdString();
    if (email.daysTo(now) < 365)
        return email.toString("MMM d").toStdString();
    return email.toString("MMM d, yyyy").toStdString();
}

static std::string contacts_line(api::Client::ContactList contacts) {
    std::stringstream ss;
    bool multiple = false;
    for (const api::Client::Contact c : contacts) {
        if (multiple)
            ss << ", ";
        ss << c.name;
        multiple = true;
    }
    return ss.str();
}

static std::string create_emblem(std::string date, std::string color) {
    return "data:image/svg+xml;utf8," SVG_FRAGMENT_1 + color + SVG_FRAGMENT_2 +
            short_date(date) + SVG_FRAGMENT_3;
}
}

/**
 * Query class
 */
Query::Query(const sc::CannedQuery &query, const sc::SearchMetadata &metadata,
             api::Config::Ptr config) :
    sc::SearchQueryBase(query, metadata), client_(config) {
}

void Query::cancelled() {
    client_.cancel();
}

bool Query::init_scope(sc::SearchReplyProxy const& reply) {
    sc::VariantMap config = settings();
    if (config.empty())
        std::cerr << "No config!" << std::endl;
    thread_messages = config["threading"].get_bool();
    show_snippets = config["snippets"].get_bool();

    sc::OnlineAccountClient oa_client(SCOPE_NAME, "email", "google");
    if (!client_.authenticated(oa_client)) {
        auto cat = reply->register_category("gmail_login", "", "",
                                            sc::CategoryRenderer(LOGIN_TEMPLATE));
        sc::CategorisedResult res(cat);
        res.set_title("Log in with Google");
        res.set_art("file:///usr/share/icons/suru/apps/scalable/googleplus-symbolic.svg");

        oa_client.register_account_login_item(res, query(),
                                              sc::OnlineAccountClient::InvalidateResults,
                                              sc::OnlineAccountClient::DoNothing);
        reply->push(res);
        return false;
    }
    return true;
}

void Query::run(sc::SearchReplyProxy const& reply) {
    if (!init_scope(reply))
        return;

    try {
        const sc::CannedQuery &query(sc::SearchQueryBase::query());
        std::string query_string = alg::trim_copy(query.query_string());
        std::string prefix = "";
        size_t sep = query_string.find(':');
        if (sep != std::string::npos) {
            prefix = query_string.substr(0, sep);
        }

        // The empty string here is important; it denotes the department to use when none has been
        // selected by the user.
        sc::Department::SPtr inbox = sc::Department::create("", query, "Inbox");
        api::Client::LabelList labels = client_.get_labels();
        for (const auto label_pair : labels) {
            sc::Department::SPtr dept = sc::Department::create(label_pair.first, query,
                                                               label_pair.second);
            inbox->add_subdepartment(dept);
        }
        sc::Department::SPtr all_mail = sc::Department::create("ALL_MAIL", query, "All mail");
        inbox->add_subdepartment(all_mail);
        reply->register_departments(inbox);

        api::Client::EmailList messages;
        if (prefix == "threadid") {
            messages = client_.threads_get(query_string.substr(sep+1, std::string::npos));
            thread_messages = true;
        } else {
            std::string label_id = "";
            if (query_string.empty()) {
                // We only get department info for empty query strings
                label_id = query.department_id();
                if (label_id == "")
                    label_id = "INBOX";
                else if (label_id == "ALL_MAIL")
                    label_id = "";
            }
            messages = client_.messages_list(query_string, label_id);
        }

        if (!messages.empty()) {
            // Threads already include full messages, searches don't
            if (messages[0].snippet == "")
                messages = client_.messages_get_batch(messages);
        }

        auto single_cat = reply->register_category("messages", "", "",
                                                   sc::CategoryRenderer(MESSAGE_TEMPLATE));
        std::map<std::string,sc::Category::SCPtr> categories;

        for (const api::Client::Email message : messages){
            bool unread = false;
            bool draft = false;
            for (const std::string &label : message.labels) {
                if (label == "UNREAD")
                    unread = true;
                if (label == "DRAFT")
                    draft = true;
            }
            // Don't display drafts
            if (draft)
                continue;

            sc::Category::SCPtr cat = single_cat;
            if (thread_messages) {
                if (categories[message.threadId] == NULL)
                    categories[message.threadId] =
                            reply->register_category(message.threadId,
                                                     trim_subject(message.header.subject), "",
                                                     sc::CategoryRenderer(THREADED_TEMPLATE));
                cat = categories[message.threadId];
            }
            sc::CategorisedResult res(cat);

            // We must have a URI
            res.set_uri("gmail://" + message.id);
            res["id"] = message.id;

            std::stringstream title;
            if (unread)
                title << "<font color='black'>";
            title << message.header.from.name;
            if (unread)
                title << "</font>";
            res.set_title(title.str());

            res["subject"] = message.header.subject;
            res["date"] = message.header.date;
            if (show_snippets)
                res["snippet"] = message.snippet;
            res["gravatar"] = message.header.from.gravatar;
            res["emblem"] = create_emblem(message.header.date, unread ? "black" : "#7a7a7a");

            res["from name"] = message.header.from.name;
            res["from address"] = message.header.from.address;
            res["replyto name"] = message.header.replyto.name;
            res["replyto address"] = message.header.replyto.address;
            res["messageId"] = message.header.messageId;
            res["threadid"] = message.threadId;

            std::stringstream ss;
            ss << "<strong>From:</strong> " << message.header.from.name;
            std::string to_line = contacts_line(message.header.to);
            if (to_line.length())
                ss << "<br><strong>To:</strong> " << to_line;
            std::string cc_line = contacts_line(message.header.cc);
            if (cc_line.length())
                ss << "<br><strong>Cc:</strong> " << cc_line;
            res["recipients"] = ss.str();

            // Push the result
            if (!reply->push(res)) {
                // If the push fails, it means the query has been cancelled.  So quit.
                return;
            }
        }

    } catch (std::domain_error &e) {
        // Handle exceptions being thrown by the client API
        std::cerr << e.what() << std::endl;
        reply->error(std::current_exception());
    }
}
