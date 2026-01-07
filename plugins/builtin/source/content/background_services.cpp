#include <hex/api/imhex_api/provider.hpp>
#include <hex/api/content_registry/communication_interface.hpp>
#include <hex/api/content_registry/settings.hpp>
#include <hex/api/content_registry/background_services.hpp>
#include <hex/api/events/events_provider.hpp>
#include <hex/api/events/events_lifecycle.hpp>
#include <hex/api/project_file_manager.hpp>

#include <hex/helpers/fmt.hpp>
#include <hex/helpers/default_paths.hpp>
#include <hex/helpers/logger.hpp>
#include <hex/providers/provider.hpp>

#include <wolv/utils/guards.hpp>
#include <wolv/utils/string.hpp>
#include <wolv/net/socket_server.hpp>

#include <fmt/chrono.h>
#include <nlohmann/json.hpp>
#include <romfs/romfs.hpp>
#include <toasts/toast_notification.hpp>

namespace hex::plugin::builtin {

    static ContentRegistry::Settings::SettingsVariable<bool, "hex.builtin.setting.general", "hex.builtin.setting.general.network_interface"> s_networkInterfaceServiceEnabled = false;
    static ContentRegistry::Settings::SettingsVariable<int, "hex.builtin.setting.general", "hex.builtin.setting.general.backups.auto_backup_time"> s_autoBackupTime = 0;

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

        bool s_dataDirty = false;
        void handleAutoBackup() {
            auto now = std::chrono::steady_clock::now();
            static std::chrono::time_point<std::chrono::steady_clock> lastBackupTime = now;

            if (s_autoBackupTime > 0 && (now - lastBackupTime) > std::chrono::seconds(s_autoBackupTime * 30)) {
                lastBackupTime = now;

                if (ImHexApi::Provider::isValid() && s_dataDirty) {
                    s_dataDirty = false;

                    std::vector<prv::Provider *> dirtyProviders;
                    for (const auto &provider : ImHexApi::Provider::getProviders()) {
                        if (provider->isDirty()) {
                            dirtyProviders.push_back(provider);
                        }
                    }

                    for (const auto &path : paths::Backups.write()) {
                        const auto backupPath = path / fmt::format("auto_backup.{:%y%m%d_%H%M%S}.hexproj", fmt::gmtime(std::chrono::system_clock::now()));
                        if (ProjectFile::store(backupPath, false)) {
                            log::info("Created auto-backup file '{}'", wolv::util::toUTF8String(backupPath));
                            break;
                        }
                    }

                    for (const auto &provider : dirtyProviders) {
                        provider->markDirty();
                    }
                }
            }

            std::this_thread::sleep_for(std::chrono::seconds(1));
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
        ContentRegistry::Settings::onChange("hex.builtin.setting.general", "hex.builtin.setting.general.mcp_server", [](const ContentRegistry::Settings::SettingsValue &value) {
            ContentRegistry::MCP::impl::setEnabled(value.get<bool>(false));
        });

        ContentRegistry::BackgroundServices::registerService("hex.builtin.background_service.network_interface", handleNetworkInterfaceService);
        ContentRegistry::BackgroundServices::registerService("hex.builtin.background_service.auto_backup", handleAutoBackup);
        ContentRegistry::BackgroundServices::registerService("hex.builtin.background_service.mcp", handleMCPServer);

        EventImHexClosing::subscribe([] {
            ContentRegistry::MCP::impl::getMcpServerInstance().reset();
        });

        EventProviderDirtied::subscribe([](prv::Provider *) {
            s_dataDirty = true;
        });
    }

}
