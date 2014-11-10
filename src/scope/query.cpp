#include <boost/algorithm/string/trim.hpp>

#include <scope/localization.h>
#include <scope/query.h>

#include <unity/scopes/Annotation.h>
#include <unity/scopes/CategorisedResult.h>
#include <unity/scopes/CategoryRenderer.h>
#include <unity/scopes/QueryBase.h>
#include <unity/scopes/SearchReply.h>
#include <unity/scopes/OnlineAccountClient.h>

#include <QRegularExpression>

#include <iomanip>
#include <sstream>

namespace sc = unity::scopes;
namespace alg = boost::algorithm;

using namespace std;
using namespace api;
using namespace scope;


/**
 * Define the larger "current weather" layout.
 *
 * The icons are larger.
 */
const static string MESSAGE_TEMPLATE =
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
        "attributes": [{
        "value": "date"
        }]
        }
        }
        )";

const static string THREADED_TEMPLATE =
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
        "attributes": [{
        "value": "date"
        }]
        }
        }
        )";

namespace {

static std::string trim_subject(const std::string line) {
    QRegularExpression regex("^ *(?:(?:fwd|re):? *)*(.*?) *$",
                             QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch match = regex.match(line.c_str());
    if (match.hasMatch())
        return match.captured(1).toStdString();
    return line;
}
}

Query::Query(const sc::CannedQuery &query, const sc::SearchMetadata &metadata,
             Config::Ptr config) :
    sc::SearchQueryBase(query, metadata), client_(config) {
}

void Query::cancelled() {
    client_.cancel();
}

bool Query::init_scope(sc::SearchReplyProxy const& reply) {
    sc::VariantMap config = settings();
    if (config.empty())
        cerr << "No config!" << endl;
    thread_messages = config["threading"].get_bool();
    show_snippets = config["snippets"].get_bool();

    sc::OnlineAccountClient oa_client(SCOPE_NAME, "email", "google");
    if (!client_.authenticated(oa_client)) {
        auto cat = reply->register_category("gmail_login", "", "");
        sc::CategorisedResult res(cat);
        res.set_title("Log in");

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
        // Start by getting information about the query
        const sc::CannedQuery &query(sc::SearchQueryBase::query());

        // Trim the query string of whitespace
        string query_string = alg::trim_copy(query.query_string());

        Client::EmailList messages;
        if (query_string.empty()) {
            // If the string is empty, get the current weather for London
            messages = client_.messages_list("");
        } else {
            // otherwise, get the current weather for the search string
            messages = client_.messages_list(query_string);
        }

        auto single_cat = reply->register_category("messages", "", "",
                                                   sc::CategoryRenderer(MESSAGE_TEMPLATE));
        std::map<std::string,sc::Category::SCPtr> categories;

        for (const Client::Email message : messages){
            // Find out more
            Client::Email message_full = client_.messages_get(message.id, false);
            sc::Category::SCPtr cat = single_cat;
            if (thread_messages) {
                if (categories[message_full.threadId] == NULL)
                    categories[message_full.threadId] =
                            reply->register_category(message_full.threadId,
                                                     trim_subject(message_full.header.subject), "",
                                                     sc::CategoryRenderer(THREADED_TEMPLATE));
                cat = categories[message_full.threadId];
            }
            sc::CategorisedResult res(cat);

            // We must have a URI
            res.set_uri("gmail://" + message_full.id);
            res.set_title(message_full.header.from.name + "   (" + message_full.header.date + ")");
            res["subject"] = message_full.header.subject;
            res["date"] = message_full.header.date;
            if (show_snippets)
                res["snippet"] = message_full.snippet;
            res["gravatar"] = message_full.header.from.gravatar;

            // Push the result
            if (!reply->push(res)) {
                // If we fail to push, it means the query has been cancelled.
                // So don't continue;
                return;
            }
        }

    } catch (domain_error &e) {
        // Handle exceptions being thrown by the client API
        cerr << e.what() << endl;
        reply->error(current_exception());
    }
    //reply->finished();
}

