#include <hex/api/content_registry.hpp>
#include <hex/api/localization_manager.hpp>
#include <hex/api/event_manager.hpp>

#include <wolv/utils/guards.hpp>
#include <wolv/net/socket_server.hpp>

#include <hex/helpers/logger.hpp>

#include <nlohmann/json.hpp>

namespace hex::plugin::builtin {

    static bool networkInterfaceServiceEnabled = false;

    namespace {

        void handleNetworkInterfaceService() {
            if (!networkInterfaceServiceEnabled) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                return;
            }

            static wolv::net::SocketServer networkInterfaceServer(31337);

            AT_FIRST_TIME {
                EventManager::subscribe<EventImHexClosing>([]{
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

    }

    void registerBackgroundServices() {
        EventManager::subscribe<EventSettingsChanged>([]{
            networkInterfaceServiceEnabled = bool(ContentRegistry::Settings::read("hex.builtin.setting.general", "hex.builtin.setting.general.network_interface", false));
        });

        ContentRegistry::BackgroundServices::registerService("hex.builtin.background_service.network_interface"_lang, handleNetworkInterfaceService);
    }

}