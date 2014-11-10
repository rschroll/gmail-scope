#include <scope/localization.h>
#include <scope/preview.h>
#include <scope/query.h>
#include <scope/scope.h>

#include <iostream>
#include <sstream>
#include <fstream>

namespace sc = unity::scopes;
using namespace std;
using namespace api;
using namespace scope;

void Scope::service_update(sc::OnlineAccountClient::ServiceStatus const&) {
    std::lock_guard<std::mutex> lock(config_mutex_);

    for (auto const& status : oa_client_->get_service_statuses()) {
        if (status.service_authenticated) {
            config_->authenticated = true;
            config_->access_token = status.access_token;
            config_->client_id = status.client_id;
            config_->client_secret = status.client_secret;
            break;
        }
    }

    if (config_->authenticated)
        cerr << "YES!" << endl;
    else
        cerr << "UH OH!" << endl;

    config_cond_.notify_all();
}

void Scope::start(string const&) {
    config_ = make_shared<Config>();

    setlocale(LC_ALL, "");
    string translation_directory = ScopeBase::scope_directory()
            + "/../share/locale/";
    bindtextdomain(GETTEXT_PACKAGE, translation_directory.c_str());

    // Under test we set a different API root
    char *apiroot = getenv("NETWORK_SCOPE_APIROOT");
    if (apiroot) {
        config_->apiroot = apiroot;
    }

    /*oa_client_.reset(new sc::OnlineAccountClient("com.ubuntu.developer.rschroll.gmail_gmail",
                                                 "email", "google"));
    oa_client_->set_service_update_callback(
                std::bind(&Scope::service_update, this, std::placeholders::_1));

    // Allow 1 second for initialization
    std::unique_lock<std::mutex> lock(config_mutex_);
    config_cond_.wait_for(lock, std::chrono::seconds(1), [this] {
        cerr << "Returning..." << endl;
        return config_ != nullptr;
    });*/
}

void Scope::stop() {
}

sc::SearchQueryBase::UPtr Scope::search(const sc::CannedQuery &query,
                                        const sc::SearchMetadata &metadata) {
    // Boilerplate construction of Query
    return sc::SearchQueryBase::UPtr(new Query(query, metadata, config_));
}

sc::PreviewQueryBase::UPtr Scope::preview(sc::Result const& result,
                                          sc::ActionMetadata const& metadata) {
    // Boilerplate construction of Preview
    return sc::PreviewQueryBase::UPtr(new Preview(result, metadata));
}

#define EXPORT __attribute__ ((visibility ("default")))

// These functions define the entry points for the scope plugin
extern "C" {

EXPORT
unity::scopes::ScopeBase*
// cppcheck-suppress unusedFunction
UNITY_SCOPE_CREATE_FUNCTION() {
    return new Scope();
}

EXPORT
void
// cppcheck-suppress unusedFunction
UNITY_SCOPE_DESTROY_FUNCTION(unity::scopes::ScopeBase* scope_base) {
    delete scope_base;
}

}

