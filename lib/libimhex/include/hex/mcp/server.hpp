#pragma once

#include <functional>
#include <nlohmann/json.hpp>
#include <wolv/net/socket_server.hpp>

namespace hex::mcp {

    class Server {
    public:
        constexpr static auto McpInternalPort = 19743;

        Server();
        ~Server();

        void listen();
        void shutdown();
        void disconnect();
        bool isConnected();

        void addPrimitive(std::string type, std::string_view capabilities, std::function<nlohmann::json(const nlohmann::json &params)> function);

    private:
        nlohmann::json handleInitialize();
        void handleNotifications(const std::string &method, const nlohmann::json &params);

        struct Primitive {
            nlohmann::json capabilities;
            std::function<nlohmann::json(const nlohmann::json &params)> function;
        };

        std::map<std::string, std::map<std::string, Primitive>> m_primitives;

        wolv::net::SocketServer m_server;
        bool m_connected = false;
    };

}
