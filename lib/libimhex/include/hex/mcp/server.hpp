#pragma once

#include <hex.hpp>

#include <functional>
#include <nlohmann/json.hpp>
#include <wolv/net/socket_server.hpp>

namespace hex::mcp {

    class JsonRpc {
    public:
        explicit JsonRpc(std::string request) : m_request(std::move(request)){ }

        struct MethodNotFoundException : std::exception {};
        struct InvalidParametersException : std::exception {};

        enum class ErrorCode: i16 {
            ParseError     = -32700,
            InvalidRequest = -32600,
            MethodNotFound = -32601,
            InvalidParams  = -32602,
            InternalError  = -32603,
        };

        using Callback = std::function<nlohmann::json(const std::string &method, const nlohmann::json &params)>;
        std::optional<std::string> execute(const Callback &callback);
        void setError(ErrorCode code, std::string message);

    private:
        std::optional<nlohmann::json> handleMessage(const nlohmann::json &request, const Callback &callback);
        std::optional<nlohmann::json> handleBatchedMessages(const nlohmann::json &request, const Callback &callback);

        nlohmann::json createDefaultMessage();
        nlohmann::json createErrorMessage(ErrorCode code, const std::string &message);
        nlohmann::json createResponseMessage(const nlohmann::json &result);

    private:
        std::string m_request;
        std::optional<int> m_id;

        struct Error {
            ErrorCode code;
            std::string message;
        };
        std::optional<Error> m_error;
    };

    struct TextContent {
        std::string text;

        operator nlohmann::json() const {
            nlohmann::json result;
            result["content"] = nlohmann::json::array({
                nlohmann::json::object({
                    { "type", "text" },
                    { "text", text }
                })
            });

            return result;
        }
    };

    struct StructuredContent {
        std::string text;
        nlohmann::json data;

        operator nlohmann::json() const {
            nlohmann::json result;
            result["content"] = nlohmann::json::array({
                nlohmann::json::object({
                    { "type", "text" },
                    { "text", text }
                })
            });
            result["structuredContent"] = data;

            return result;
        }
    };

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

        struct ClientInfo {
            std::string name;
            std::string version;
            std::string protocolVersion;
        };

        const ClientInfo& getClientInfo() const {
            return m_clientInfo;
        }

    private:
        nlohmann::json handleInitialize(const nlohmann::json &params);
        void handleNotifications(const std::string &method, const nlohmann::json &params);

        struct Primitive {
            nlohmann::json capabilities;
            std::function<nlohmann::json(const nlohmann::json &params)> function;
        };

        std::map<std::string, std::map<std::string, Primitive>> m_primitives;

        wolv::net::SocketServer m_server;
        bool m_connected = false;
        ClientInfo m_clientInfo;
    };

}
