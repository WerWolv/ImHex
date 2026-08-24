#include <hex/api/content_registry/communication_interface.hpp>
#include <hex/api/content_registry/settings.hpp>
#include <hex/api/content_registry/background_services.hpp>
#include <hex/api/events/events_lifecycle.hpp>

#include <hex/helpers/logger.hpp>

#include <wolv/net/socket_server.hpp>

#include <nlohmann/json.hpp>
#include <romfs/romfs.hpp>
#include <toasts/toast_notification.hpp>

namespace hex::plugin::builtin {

    static ContentRegistry::Settings::SettingsVariable<bool, "hex.builtin.setting.general", "hex.builtin.setting.general.network_interface"> s_networkInterfaceServiceEnabled = false;

    namespace {

        void handleNetworkInterfaceService() {
            if (!s_networkInterfaceServiceEnabled) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                return;
            }

            static wolv::net::SocketServer networkInterfaceServer(31337);

            AT_FIRST_TIME {
                EventImHexClosing::subscribe([]{
                    networkInterfaceServer.shutdown();
                });
            };

            networkInterfaceServer.accept([](auto, const std::vector<u8> &data) -> std::vector<u8> {
                nlohmann::json result;

                try {
                   auto json = nlohmann::json::parse(data.begin(), data.end());

                   const auto &endpoints = ContentRegistry::CommunicationInterface::impl::getNetworkEndpoints();
                   if (auto callback = endpoints.find(json.at("endpoint").get<std::string>()); callback != endpoints.end()) {
                       log::info("Network endpoint {} called with arguments '{}'", json.at("endpoint").get<std::string>(), json.contains("data") ? json.at("data").dump() : "");

                       auto responseData = callback->second(json.contains("data") ? json.at("data") : nlohmann::json::object());

                       result["status"] = "success";
                       result["data"] = responseData;
                   } else {
                       throw std::runtime_error("Endpoint not found");
                   }
                } catch (const std::exception &e) {
                    log::warn("Network interface service error: {}", e.what());

                    result["status"] = "error";
                    result["data"]["error"] = e.what();
                }

                auto resultString = result.dump();
                return { resultString.begin(), resultString.end() };
            });
        }

        void handleMCPServer() {
            if (!ContentRegistry::MCP::isEnabled()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                ContentRegistry::MCP::impl::getMcpServerInstance()->disconnect();
                return;
            }

            ContentRegistry::MCP::impl::getMcpServerInstance()->listen();
        }

    }

    void registerBackgroundServices() {
        ContentRegistry::Settings::onChange("hex.builtin.setting.general"_unlocalized, "hex.builtin.setting.general.mcp_server"_unlocalized, [](const ContentRegistry::Settings::SettingsValue &value) {
            ContentRegistry::MCP::impl::setEnabled(value.get<bool>(false));
        });

        ContentRegistry::BackgroundServices::registerService("hex.builtin.background_service.network_interface"_unlocalized, handleNetworkInterfaceService);
        ContentRegistry::BackgroundServices::registerService("hex.builtin.background_service.mcp"_unlocalized, handleMCPServer);

        EventImHexClosing::subscribe([] {
            ContentRegistry::MCP::impl::getMcpServerInstance().reset();
        });
    }

}
