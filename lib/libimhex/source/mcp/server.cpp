#include <hex/mcp/server.hpp>

#include <string>

#include <fmt/format.h>
#include <hex/helpers/logger.hpp>
#include <nlohmann/json.hpp>
#include <utility>
#include <wolv/net/socket_client.hpp>
#include <hex/api/imhex_api/system.hpp>


namespace hex::mcp {

    std::optional<std::string> JsonRpc::execute(const Callback &callback) {
        try {
            auto requestJson = nlohmann::json::parse(m_request);

            if (requestJson.is_array()) {
                return handleBatchedMessages(requestJson, callback).transform([](const auto &response) { return response.dump(); });
            } else {
                return handleMessage(requestJson, callback).transform([](const auto &response) { return response.dump(); });
            }
        } catch (const nlohmann::json::exception &) {
            return createErrorMessage(ErrorCode::ParseError, "Parse error").dump();
        }
    }

    void JsonRpc::setError(ErrorCode code, std::string message) {
        m_error = Error{ code, std::move(message) };
    }

    std::optional<nlohmann::json> JsonRpc::handleMessage(const nlohmann::json &request, const Callback &callback) {
        try {
            // Validate JSON-RPC request
            if (!request.contains("jsonrpc") || request["jsonrpc"] != "2.0" ||
                !request.contains("method") || !request["method"].is_string()) {
                m_id = request.contains("id") ? std::optional(request["id"].get<int>()) : std::nullopt;

                return createErrorMessage(ErrorCode::InvalidRequest, "Invalid Request");
            }

            m_id = request.contains("id") ? std::optional(request["id"].get<int>()) : std::nullopt;

            // Return a user-specified error if set
            if (m_error.has_value()) {
                return createErrorMessage(m_error->code, m_error->message);
            }

            // Execute the method
            auto result = callback(request["method"].get<std::string>(), request.value("params", nlohmann::json::object()));

            if (!m_id.has_value())
                return std::nullopt;

            return createResponseMessage(result.is_null() ? nlohmann::json::object() : result);
        } catch (const MethodNotFoundException &) {
            return createErrorMessage(ErrorCode::MethodNotFound, "Method not found");
        } catch (const InvalidParametersException &) {
            return createErrorMessage(ErrorCode::InvalidParams, "Invalid params");
        } catch (const std::exception &e) {
            return createErrorMessage(ErrorCode::InternalError, e.what());
        }
    }

    std::optional<nlohmann::json> JsonRpc::handleBatchedMessages(const nlohmann::json &request, const Callback &callback) {
        if (!request.is_array()) {
            return createErrorMessage(ErrorCode::InvalidRequest, "Invalid Request");
        }

        nlohmann::json responses = nlohmann::json::array();
        for (const auto &message : request) {
            auto response = handleMessage(message, callback);
            if (response.has_value())
                responses.push_back(*response);
        }

        if (responses.empty())
            return std::nullopt;

        return responses;
    }

    nlohmann::json JsonRpc::createDefaultMessage() {
        nlohmann::json message;
        message["jsonrpc"] = "2.0";
        if (m_id.has_value())
            message["id"] = m_id.value();
        else
            message["id"] = nullptr;

        return message;
    }

    nlohmann::json JsonRpc::createErrorMessage(ErrorCode code, const std::string &message) {
        auto json = createDefaultMessage();
        json["error"] = {
            { "code",    int(code) },
            { "message", message   }
        };
        return json;
    }

    nlohmann::json JsonRpc::createResponseMessage(const nlohmann::json &result) {
        auto json = createDefaultMessage();
        json["result"] = result;
        return json;
    }

    Server::Server() : m_server(McpInternalPort, 1024, 1, true) {

    }

    Server::~Server() {
        this->shutdown();
    }

    void Server::listen() {
        m_server.accept([this](auto, const std::vector<u8> &data) -> std::vector<u8> {
            std::string request(data.begin(), data.end());

            log::debug("MCP ----> {}", request);

            JsonRpc rpc(request);
            auto response = rpc.execute([this](const std::string &method, const nlohmann::json &params) -> nlohmann::json {
                if (method == "initialize") {
                    return handleInitialize();
                } else if (method.starts_with("notifications/")) {
                    handleNotifications(method.substr(14), params);
                    return {};
                } else if (method.ends_with("/list")) {
                    auto primitiveName = method.substr(0, method.size() - 5);
                    if (m_primitives.contains(primitiveName)) {
                        nlohmann::json capabilitiesList = nlohmann::json::array();
                        for (const auto &[name, primitive] : m_primitives[primitiveName]) {
                            capabilitiesList.push_back(primitive.capabilities);
                        }

                        nlohmann::json result;
                        result[primitiveName] = capabilitiesList;
                        return result;
                    }
                } else if (method.ends_with("/call")) {
                    auto primitive = method.substr(0, method.size() - 5);
                    if (auto primitiveIt = m_primitives.find(primitive); primitiveIt != m_primitives.end()) {
                        auto name = params.value("name", "");
                        if (auto functionIt = primitiveIt->second.find(name); functionIt != primitiveIt->second.end()) {
                            auto result = functionIt->second.function(params.value("arguments", nlohmann::json::object()));

                            return result.is_null() ? nlohmann::json::object() : result;
                        }
                    }
                }

                throw JsonRpc::MethodNotFoundException();
            });

            log::debug("MCP <---- {}", response.value_or("<Nothing>"));

            if (response.has_value())
                return { response->begin(), response->end() };
            else
                return std::vector<u8>{ 0x00 };
        }, [this](auto) {
            log::info("MCP client disconnected");
            m_connected = false;
        }, true);
    }

    void Server::shutdown() {
        m_server.shutdown();
    }

    void Server::disconnect() {
        m_server.disconnectClients();
    }

    void Server::addPrimitive(std::string type, std::string_view capabilities, std::function<nlohmann::json(const nlohmann::json &params)> function) {
        auto json = nlohmann::json::parse(capabilities);
        auto name = json["name"].get<std::string>();

        m_primitives[type][name] = {
            .capabilities=json,
            .function=function
        };
    }


    nlohmann::json Server::handleInitialize() {
        constexpr static auto ServerName = "ImHex";
        constexpr static auto ProtocolVersion = "2025-06-18";

        return {
            { "protocolVersion", ProtocolVersion },
            {
                "capabilities",
                {
                    { "tools",     nlohmann::json::object() },
                },
            },
            {
                "serverInfo", {
                   { "name",    ServerName    },
                   { "version", ImHexApi::System::getImHexVersion().get() }
                }
            }
        };
    }

    void Server::handleNotifications(const std::string &method, [[maybe_unused]] const nlohmann::json &params) {
        if (method == "initialized") {
            m_connected = true;
        }
    }

    bool Server::isConnected() {
        return m_connected;
    }
}
