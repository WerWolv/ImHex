#include <hex/api/content_registry.hpp>
#include <hex/api/localization.hpp>
#include <hex/api/event.hpp>

#include <wolv/net/socket_server.hpp>

#include <hex/helpers/logger.hpp>

#include <nlohmann/json.hpp>

namespace hex::plugin::builtin {

    static bool networkInterfaceServiceEnabled = false;

    namespace {

        void handleNetworkInterfaceService() {
            if (!networkInterfaceServiceEnabled) {
                std::this_thread::yield();
                return;
            }
            static wolv::net::SocketServer networkInterfaceServer(51337);
            networkInterfaceServer.accept([](auto, const std::vector<u8> &data) -> std::vector<u8> {
                nlohmann::json result;

                try {
                   auto json = nlohmann::json::parse(data.begin(), data.end());

                   const auto &endpoints = ContentRegistry::CommunicationInterface::impl::getNetworkEndpoints();
                   if (auto callback = endpoints.find(json["endpoint"].get<std::string>()); callback != endpoints.end()) {
                       auto responseData = callback->second(json["data"]);

                       result["status"] = "success";
                       result["data"] = responseData;
                   }
                } catch (const std::exception &e) {
                    log::error("Network interface service error: {}", e.what());

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
            networkInterfaceServiceEnabled = bool(ContentRegistry::Settings::read("hex.builtin.setting.general", "hex.builtin.setting.general.network_interface", 0));
        });

        ContentRegistry::BackgroundServices::registerService("hex.builtin.background_service.network_interface"_lang, handleNetworkInterfaceService);
    }

}