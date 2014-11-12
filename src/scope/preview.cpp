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
    layout1col.add_column( { "header", "recipients", "body", "search header", "searches" });
    layout2col.add_column( { "header", "recipients", "body" });
    layout2col.add_column( { "search header", "searches" });
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
    sc::VariantBuilder builder;
    sc::CannedQuery from_query(SCOPE_NAME, "from:" + res["from email"].get_string(), "");
    builder.add_tuple({ { "id", sc::Variant("search from") },
                        { "label", sc::Variant("From " + res["from name"].get_string()) },
                        { "uri", sc::Variant(from_query.to_uri()) } });
    sc::CannedQuery thread_query(SCOPE_NAME, "threadid:" + res["threadid"].get_string(), "");
    builder.add_tuple({ { "id", sc::Variant("search thread") },
                        { "label", sc::Variant("In thread") },
                        { "uri", sc::Variant(thread_query.to_uri()) } });
    searches.add_attribute_value("actions", builder.end());

    reply->push( { header, recipients, search_header, searches });

    // The body we get through another HTTP request, so do it last and push it separately
    api::Client::Email message = client_.messages_get(result()["id"].get_string(), true);
    sc::PreviewWidget body("body", "text");
    body.add_attribute_value("text", sc::Variant(message.body));

    reply->push( { body });
}
