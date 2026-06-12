#include <hex/api/imhex_api/system.hpp>
#include <hex/api/content_registry/settings.hpp>
#include <hex/api/content_registry/user_interface.hpp>
#include <hex/api/events/events_gui.hpp>
#include <hex/api_urls.hpp>

#include <hex/api/task_manager.hpp>
#include <hex/helpers/http_requests.hpp>

#include <wolv/hash/uuid.hpp>
#include <hex/ui/imgui_imhex_extensions.h>
#include <toasts/toast_notification.hpp>

#include <nlohmann/json.hpp>

#include <string>
#include <fonts/tabler_icons.hpp>
#include <hex/api/events/events_lifecycle.hpp>

namespace hex::fonts { bool setupFonts(); }

namespace hex::plugin::builtin {

    namespace {

        using namespace std::literals::string_literals;

        #if defined(IMHEX_ENABLE_UPDATER)
            bool checkForUpdatesSync() {
                int checkForUpdates = ContentRegistry::Settings::read<int>("hex.builtin.setting.general", "hex.builtin.setting.general.server_contact", 2);
                if (checkForUpdates != 1)
                    return true;

                // Check if we should check for updates
                TaskManager::createBackgroundTask("Update Check", [] {
                    const auto updateString = ImHexApi::System::checkForUpdate();

                    if (!updateString.has_value())
                        return;

                    TaskManager::doLater([updateString] {
                        ContentRegistry::UserInterface::addTitleBarButton(ICON_TA_DOWNLOAD, ImGuiCustomCol_ToolbarGreen, "hex.builtin.welcome.update.title", [] {
                            ImHexApi::System::updateImHex(ImHexApi::System::isNightlyBuild() ? ImHexApi::System::UpdateType::Nightly : ImHexApi::System::UpdateType::Stable);
                        });

                        ui::ToastInfo::open(fmt::format("hex.builtin.welcome.update.desc"_lang, *updateString));
                    });
                });

                // Check if there is a telemetry uuid
                auto uuid = ContentRegistry::Settings::read<std::string>("hex.builtin.setting.general", "hex.builtin.setting.general.uuid", "");
                if (uuid.empty()) {
                    // Generate a new uuid
                    uuid = wolv::hash::generateUUID();
                    // Save
                    ContentRegistry::Settings::write<std::string>("hex.builtin.setting.general", "hex.builtin.setting.general.uuid", uuid);
                }

                TaskManager::createBackgroundTask("hex.builtin.task.sending_statistics", [uuid](auto&) {
                    // To avoid potentially flooding our database with lots of dead users
                    // from people just visiting the website, don't send telemetry data from
                    // the web version
                    #if defined(OS_WEB)
                        return;
                    #endif

                    // Make telemetry request
                    nlohmann::json telemetry = {
                            { "uuid", uuid },
                            { "format_version", "1" },
                            { "imhex_version", ImHexApi::System::getImHexVersion().get(false) },
                            { "imhex_commit", fmt::format("{}@{}", ImHexApi::System::getCommitHash(true), ImHexApi::System::getCommitBranch()) },
                            { "install_type", ImHexApi::System::isPortableVersion() ? "Portable" : "Installed" },
                            { "os", ImHexApi::System::getOSName() },
                            { "os_version", ImHexApi::System::getOSVersion() },
                            { "arch", ImHexApi::System::getArchitecture() },
                            { "gpu_vendor", ImHexApi::System::getGPUVendor() },
                            { "corporate_env", ImHexApi::System::isCorporateEnvironment() }
                    };

                    HttpRequest telemetryRequest("POST", ImHexApiURL + "/telemetry"s);
                    telemetryRequest.setTimeout(500);

                    telemetryRequest.setBody(telemetry.dump());
                    telemetryRequest.addHeader("Content-Type", "application/json");

                    // Execute request
                    telemetryRequest.execute();
                });

                return true;
            }

            bool checkForUpdates() {
                TaskManager::createBackgroundTask("hex.builtin.task.check_updates", [](auto&) { checkForUpdatesSync(); });
                return true;
            }
        #endif

        bool configureUIScale() {
            EventDPIChanged::subscribe([](float, float newScaling) {
                int interfaceScaleSetting = int(ContentRegistry::Settings::read<float>("hex.builtin.setting.interface", "hex.builtin.setting.interface.scaling_factor", 0.0F) * 10.0F);

                float interfaceScaling;
                if (interfaceScaleSetting == 0)
                    interfaceScaling = newScaling;
                else
                    interfaceScaling = interfaceScaleSetting / 10.0F;

                ImHexApi::System::impl::setGlobalScale(interfaceScaling);
            });

            const auto nativeScale = ImHexApi::System::getNativeScale();
            EventDPIChanged::post(nativeScale, nativeScale);

            return true;
        }

        bool loadWindowSettings() {
            bool multiWindowEnabled = ContentRegistry::Settings::read<bool>("hex.builtin.setting.interface", "hex.builtin.setting.interface.multi_windows", false);
            ImHexApi::System::impl::setMultiWindowMode(multiWindowEnabled);

            bool restoreWindowPos = ContentRegistry::Settings::read<bool>("hex.builtin.setting.interface", "hex.builtin.setting.interface.restore_window_pos", false);

            if (restoreWindowPos) {
                ImHexApi::System::InitialWindowProperties properties = {};

                properties.maximized = ContentRegistry::Settings::read<bool>("hex.builtin.setting.interface", "hex.builtin.setting.interface.window.maximized", false);
                properties.x       = ContentRegistry::Settings::read<int>("hex.builtin.setting.interface", "hex.builtin.setting.interface.window.x", 0);
                properties.y       = ContentRegistry::Settings::read<int>("hex.builtin.setting.interface", "hex.builtin.setting.interface.window.y", 0);
                properties.width   = ContentRegistry::Settings::read<int>("hex.builtin.setting.interface", "hex.builtin.setting.interface.window.width", 0);
                properties.height  = ContentRegistry::Settings::read<int>("hex.builtin.setting.interface", "hex.builtin.setting.interface.window.height", 0);

                ImHexApi::System::impl::setInitialWindowProperties(properties);
            }

            return true;
        }

    }

    void addInitTasks() {
        ImHexApi::System::addStartupTask("Load Window Settings", false, loadWindowSettings);
        ImHexApi::System::addStartupTask("Configuring UI scale", false, configureUIScale);
        #if defined(IMHEX_ENABLE_UPDATER)
            ImHexApi::System::addStartupTask("Checking for updates", true, checkForUpdates);
        #endif
    }
}