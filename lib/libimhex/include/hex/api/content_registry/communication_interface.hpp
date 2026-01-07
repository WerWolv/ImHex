#pragma once

#include <hex.hpp>

#include <nlohmann/json_fwd.hpp>

#include <map>
#include <string>

#include <hex/mcp/server.hpp>

EXPORT_MODULE namespace hex {

    /* Network Communication Interface Registry. Allows adding new communication interface endpoints */
    namespace ContentRegistry::CommunicationInterface {

        namespace impl {
            using NetworkCallback = std::function<nlohmann::json(const nlohmann::json &)>;

            const std::map<std::string, NetworkCallback>& getNetworkEndpoints();
        }

        void registerNetworkEndpoint(const std::string &endpoint, const impl::NetworkCallback &callback);

    }

    namespace ContentRegistry::MCP {

        namespace impl {
            std::unique_ptr<mcp::Server>& getMcpServerInstance();

            void setEnabled(bool enabled);
        }

        bool isEnabled();
        bool isConnected();

        void registerTool(std::string_view capabilities, std::function<nlohmann::json(const nlohmann::json &params)> function);

    }

}