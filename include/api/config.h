#ifndef API_CONFIG_H_
#define API_CONFIG_H_

#include <memory>
#include <string>
#include <deque>

namespace api {

struct Config {
    typedef std::shared_ptr<Config> Ptr;

    /*
     * The root of all API request URLs
     */
    std::string apidomain { "https://www.googleapis.com" };
    std::string apiroot { "/gmail/v1" };

    /*
     * The custom HTTP user agent string for this library
     */
    std::string user_agent { "Gmail Scope (Ubuntu) " VERSION };

    /*
     * Cached values
     */
    std::string users_address { };
    std::deque<std::pair<std::string, std::string>> labels { };
};

}

#endif /* API_CONFIG_H_ */
