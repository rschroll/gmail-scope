/* Copyright 2014 Robert Schroll
 *
 * This file is part of Gmail Scope and is distributed under the terms of
 * the GPL. See the file LICENSE for full details.
 */

#include <scope/activation.h>
#include <api/config.h>

#include <unity/scopes/Result.h>
#include <unity/scopes/ActionMetadata.h>

#include <iostream>

namespace sc = unity::scopes;

using namespace scope;


Activation::Activation(const sc::Result &result, const sc::ActionMetadata &metadata,
                       const std::string &widget_id, const std::string &action_id,
                       api::Config::Ptr config) :
    sc::ActivationQueryBase(result, metadata, widget_id, action_id), client_(config) {
}

sc::ActivationResponse Activation::activate() {
    std::string widgetId = widget_id();
    if (widgetId == "reply") {
        sc::Result res = result();
        std::string message = action_metadata().scope_data().get_dict()["review"].get_string();
        std::string to_source = (res["replyto address"].get_string() != "" ? "replyto" : "from");
        std::string threadid = res["threadid"].get_string();
        api::Client::Contact replyto;
        replyto.name = res[to_source + " name"].get_string();
        replyto.address = res[to_source + " address"].get_string();
        client_.send_message(replyto, "Re: " + res["subject"].get_string(), message,
                settings()["from"].get_string(), res["messageId"].get_string(), threadid);

        sc::CannedQuery query(SCOPE_NAME, "threadid:" + threadid, "");
        return sc::ActivationResponse(query);
    } else if (widgetId == "modifiers") {
        std::string id = result()["id"].get_string();
        std::string actionId = action_id();
        if (actionId == "mark unread")
            client_.messages_set_unread(id, true);
        else if (actionId == "mark read")
            client_.messages_set_unread(id, false);
        else if (actionId == "trash")
            client_.messages_trash(id);
        else if (actionId == "untrash")
            client_.messages_untrash(id);
        return sc::ActivationResponse::ShowPreview;
    }
    return sc::ActivationResponse::NotHandled;
}
