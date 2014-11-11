#ifndef API_CLIENT_H_
#define API_CLIENT_H_

#include <api/config.h>

#include <atomic>
#include <deque>
#include <map>
#include <string>
#include <core/net/http/request.h>
#include <core/net/uri.h>

#include <QJsonDocument>

#include <unity/scopes/OnlineAccountClient.h>

namespace api {

const std::string TIME_FMT = "MMMM d, yyyy HH:mm";

/**
 * Provide a nice way to access the HTTP API.
 *
 * We don't want our scope's code to be mixed together with HTTP and JSON handling.
 */
class Client {
public:

    struct Contact {
        std::string name;
        std::string address;
        std::string gravatar;
    };

    typedef std::deque<Contact> ContactList;

    struct Header {
        std::string date;
        Contact from;
        ContactList to;
        ContactList cc;
        std::string subject;
    };

    /**
     * Information about an email
     */
    struct Email {
        std::string id;
        std::string threadId;
        std::string snippet;
        Header header;
    };

    /**
     * A list of weather information
     */
    typedef std::deque<Email> EmailList;

    Client(Config::Ptr config);

    virtual ~Client() = default;

    virtual bool authenticated(unity::scopes::OnlineAccountClient &oa_client);

    /**
     * Get a list of email messages
     */
    virtual EmailList messages_list(const std::string &query);

    virtual Email messages_get(const std::string &id, bool full);

    /**
     * Cancel any pending queries (this method can be called from a different thread)
     */
    virtual void cancel();

    virtual Config::Ptr config();

protected:
    void get(const core::net::Uri::Path &path,
             const core::net::Uri::QueryParameters &parameters,
             QJsonDocument &root);
    /**
     * Progress callback that allows the query to cancel pending HTTP requests.
     */
    core::net::http::Request::Progress::Next progress_report(
            const core::net::http::Request::Progress& progress);

    /**
     * Hang onto the configuration information
     */
    Config::Ptr config_;

    /**
     * Thread-safe cancelled flag
     */
    std::atomic<bool> cancelled_;
};

}

#endif // API_CLIENT_H_

