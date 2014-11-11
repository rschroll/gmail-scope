#include <scope/preview.h>
#include <api/client.h>

#include <unity/scopes/ColumnLayout.h>
#include <unity/scopes/PreviewWidget.h>
#include <unity/scopes/PreviewReply.h>
#include <unity/scopes/Result.h>
#include <unity/scopes/VariantBuilder.h>

#include <iostream>

namespace sc = unity::scopes;

using namespace std;
using namespace scope;

Preview::Preview(const sc::Result &result, const sc::ActionMetadata &metadata,
                 api::Config::Ptr config) :
    sc::PreviewQueryBase(result, metadata), client_(config) {
}

void Preview::cancelled() {
}

void Preview::run(sc::PreviewReplyProxy const& reply) {
    sc::ColumnLayout layout1col(1);
    layout1col.add_column( { "header", "recipients", "body" });
    reply->register_layout( { layout1col });

    // Define the header section
    sc::PreviewWidget header("header", "header");
    // It has title and a subtitle properties
    header.add_attribute_mapping("title", "subject");
    header.add_attribute_mapping("subtitle", "date");
    header.add_attribute_mapping("mascot", "gravatar");

    // Define the summary section
    sc::PreviewWidget recipients("recipients", "text");
    // It has a text property, mapped to the result's description property
    recipients.add_attribute_mapping("text", "recipients");

    // Push each of the sections
    reply->push( { header, recipients });

    // Get the body
    api::Client::Email message = client_.messages_get(result()["id"].get_string(), true);
    sc::PreviewWidget body("body", "text");
    body.add_attribute_value("text", sc::Variant(message.body));

    reply->push( { body });

}

