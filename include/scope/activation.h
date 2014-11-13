#ifndef SCOPE_ACTIVATION_H_
#define SCOPE_ACTIVATION_H_

#include <api/client.h>

#include <unity/scopes/ActivationQueryBase.h>

namespace scope {

class Activation: public unity::scopes::ActivationQueryBase {
public:
    Activation(const unity::scopes::Result &result,
               const unity::scopes::ActionMetadata &metadata,
               const std::string &widget_id,
               const std::string &action_id,
               api::Config::Ptr config);

    ~Activation() = default;

    unity::scopes::ActivationResponse activate() override;

private:
    api::Client client_;
};

}

#endif // SCOPE_ACTIVATION_H_
