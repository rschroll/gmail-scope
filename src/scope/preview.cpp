/* Copyright 2014 Robert Schroll
 *
 * This file is part of Gmail Scope and is distributed under the terms of
 * the GPL. See the file LICENSE for full details.
 */

#include <scope/preview.h>
#include <api/client.h>

#include <unity/scopes/ColumnLayout.h>
#include <unity/scopes/PreviewWidget.h>
#include <unity/scopes/PreviewReply.h>
#include <unity/scopes/Result.h>
#include <unity/scopes/VariantBuilder.h>

#include <iostream>

namespace sc = unity::scopes;

using namespace scope;


Preview::Preview(const sc::Result &result, const sc::ActionMetadata &metadata,
                 api::Config::Ptr config) :
    sc::PreviewQueryBase(result, metadata), client_(config) {
}

void Preview::cancelled() {
}

void Preview::run(sc::PreviewReplyProxy const& reply) {
    sc::Result res = result();
    sc::ColumnLayout layout1col(1), layout2col(2);
    layout1col.add_column( { "header", "recipients", "body", "modifiers", "search header", "searches",
                             "reply", "openers" });
    layout2col.add_column( { "header", "recipients", "body" });
    layout2col.add_column( { "modifiers", "search header", "searches", "reply", "openers" });
    reply->register_layout( { layout1col, layout2col });

    sc::PreviewWidget header("header", "header");
    header.add_attribute_mapping("title", "subject");
    header.add_attribute_mapping("subtitle", "date");
    header.add_attribute_mapping("mascot", "gravatar");

    sc::PreviewWidget recipients("recipients", "text");
    recipients.add_attribute_mapping("text", "recipients");

    sc::PreviewWidget search_header("search header", "header");
    search_header.add_attribute_value("title", sc::Variant("Find messages"));

    sc::PreviewWidget searches("searches", "actions");
    {
        sc::VariantBuilder builder;
        sc::CannedQuery from_query(SCOPE_NAME, "from:" + res["from address"].get_string(), "");
        builder.add_tuple({ { "id", sc::Variant("search from") },
                            { "label", sc::Variant("From " + res["from name"].get_string()) },
                            { "uri", sc::Variant(from_query.to_uri()) } });
        sc::CannedQuery thread_query(SCOPE_NAME, "threadid:" + res["threadid"].get_string(), "");
        builder.add_tuple({ { "id", sc::Variant("search thread") },
                            { "label", sc::Variant("In thread") },
                            { "uri", sc::Variant(thread_query.to_uri()) } });
        searches.add_attribute_value("actions", builder.end());
    }

    sc::PreviewWidget reply_widget("reply", "rating-input");
    std::string reply_name = res["replyto name"].get_string();
    if (reply_name == "")
        reply_name = res["from name"].get_string();
    reply_widget.add_attribute_value("visible", sc::Variant("review"));
    reply_widget.add_attribute_value("required", sc::Variant("review"));
    reply_widget.add_attribute_value("review-label", sc::Variant("Reply to " + reply_name));

    sc::PreviewWidget openers("openers", "actions");
    {
        sc::VariantBuilder builder;
        builder.add_tuple({ { "id", sc::Variant("open gmail") },
                            { "label", sc::Variant("View in Gmail") },
                            { "uri", sc::Variant("https://mail.google.com/mail/mu/mp/206/#cv/All Mail/" +
                              res["threadid"].get_string()) }});
        openers.add_attribute_value("actions", builder.end());
    }

    reply->push( { header, recipients, search_header, searches, reply_widget, openers });

    // The body we get through another HTTP request, so do it last and push it separately
    api::Client::Email message = client_.messages_get(result()["id"].get_string(), true);
    sc::PreviewWidget body("body", "text");
    body.add_attribute_value("text", sc::Variant(message.body));

    bool unread = false;
    bool trash = false;
    for (const std::string &label : message.labels) {
        if (label == "UNREAD")
            unread = true;
        else if (label == "TRASH")
            trash = true;
    }
    sc::PreviewWidget modifiers("modifiers", "actions");
    {
        std::string status = (unread ? "read" : "unread");
        sc::VariantBuilder builder;
        builder.add_tuple({ { "id", sc::Variant("mark " + status) },
                            { "label", sc::Variant("Mark " + status) } });
        builder.add_tuple({ { "id", sc::Variant(trash ? "untrash" : "trash") },
                            { "label", sc::Variant(trash ? "Remove from trash" : "Move to trash") } });
        modifiers.add_attribute_value("actions", builder.end());
    }

    reply->push( { body, modifiers });
}
