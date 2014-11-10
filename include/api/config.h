#ifndef API_CONFIG_H_
#define API_CONFIG_H_

#include <memory>
#include <string>

namespace api {

struct Config {
    typedef std::shared_ptr<Config> Ptr;

    /*
     * The root of all API request URLs
     */
    std::string apiroot { "https://www.googleapis.com/gmail/v1" };

    /*
     * The custom HTTP user agent string for this library
     */
    std::string user_agent { "GMail Scope for Ubuntu" };

    // From the YouTube scope
    std::string access_token { };
    std::string client_id { };
    std::string client_secret { };
    std::string api_key { }; // For unauthenticated access; hard coded
    bool authenticated = false;
};

}

#endif /* API_CONFIG_H_ */

