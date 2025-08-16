#pragma once

#include <hex.hpp>

#include <nlohmann/json_fwd.hpp>

#include <map>
#include <string>

EXPORT_MODULE namespace hex {

    /* Network Communication Interface Registry. Allows adding new communication interface endpoints */
    namespace ContentRegistry::CommunicationInterface {

        namespace impl {
            using NetworkCallback = std::function<nlohmann::json(const nlohmann::json &)>;

            const std::map<std::string, NetworkCallback>& getNetworkEndpoints();
        }

        void registerNetworkEndpoint(const std::string &endpoint, const impl::NetworkCallback &callback);

    }

}