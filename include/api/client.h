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

    /**
     * Data structures
     */

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
        Contact replyto;
        std::string subject;
        std::string messageId;
    };

    typedef std::deque<std::string> Labels;

    struct Email {
        std::string id;
        std::string threadId;
        std::string snippet;
        Header header;
        std::string body;
        Labels labels;
    };

    typedef std::deque<Email> EmailList;

    typedef std::deque<std::pair<std::string, std::string>> LabelList;

    /**
     * Constructor / destructor
     */

    Client(Config::Ptr config);

    virtual ~Client() = default;

    /**
     * Public methods
     */

    virtual bool authenticated(unity::scopes::OnlineAccountClient &oa_client);

    virtual EmailList messages_list(const std::string &query, const std::string &label_id);

    virtual Email messages_get(const std::string &id, bool body);

    virtual Email messages_set_unread(const std::string& id, bool unread);

    virtual Email messages_trash(const std::string& id);

    virtual Email messages_untrash(const std::string& id);

    virtual EmailList threads_get(const std::string& id);

    virtual Email send_message(const Contact& to, const std::string& subject,
                               const std::string& body, const std::string &from_name,
                               const std::string& ref_id, const std::string &thread_id);

    virtual std::string users_address();

    virtual LabelList get_labels();

    /**
     * Cancel any pending queries (this method can be called from a different thread)
     */
    virtual void cancel();

    virtual Config::Ptr config();

protected:
    void get(const core::net::Uri::Path &path,
             const core::net::Uri::QueryParameters &parameters,
             QJsonDocument &root);

    void post(const core::net::Uri::Path &path,
              const core::net::Uri::QueryParameters &parameters,
              const std::string& payload,
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
