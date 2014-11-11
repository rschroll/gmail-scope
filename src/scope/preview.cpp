#include <scope/preview.h>

#include <unity/scopes/ColumnLayout.h>
#include <unity/scopes/PreviewWidget.h>
#include <unity/scopes/PreviewReply.h>
#include <unity/scopes/Result.h>
#include <unity/scopes/VariantBuilder.h>

#include <iostream>

namespace sc = unity::scopes;

using namespace std;
using namespace scope;

Preview::Preview(const sc::Result &result, const sc::ActionMetadata &metadata) :
    sc::PreviewQueryBase(result, metadata) {
}

void Preview::cancelled() {
}

void Preview::run(sc::PreviewReplyProxy const& reply) {
    sc::ColumnLayout layout1col(1);
    layout1col.add_column( { "header", "recipients" });
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
}

