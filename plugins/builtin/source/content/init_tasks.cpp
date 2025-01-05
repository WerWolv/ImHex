#include <hex/api/imhex_api.hpp>
#include <hex/api/content_registry.hpp>
#include <hex/api/event_manager.hpp>
#include <hex/api_urls.hpp>

#include <hex/api/task_manager.hpp>
#include <hex/helpers/http_requests.hpp>
#include <hex/helpers/logger.hpp>

#include <wolv/hash/uuid.hpp>

namespace hex::plugin::builtin {

    namespace {

        using namespace std::literals::string_literals;

        bool checkForUpdatesSync() {
            int checkForUpdates = ContentRegistry::Settings::read<int>("hex.builtin.setting.general", "hex.builtin.setting.general.server_contact", 2);

            // Check if we should check for updates
            if (checkForUpdates == 1) {
                HttpRequest request("GET", GitHubApiURL + "/releases/latest"s);

                // Query the GitHub API for the latest release version
                auto response = request.execute().get();
                if (response.getStatusCode() != 200)
                    return false;

                nlohmann::json releases;
                try {
                    releases = nlohmann::json::parse(response.getData());
                } catch (const std::exception &) {
                    return false;
                }

                // Check if the response is valid
                if (!releases.contains("tag_name") || !releases["tag_name"].is_string())
                    return false;

                // Convert the current version string to a format that can be compared to the latest release
                auto currVersion   = "v" + ImHexApi::System::getImHexVersion().get(false);

                // Get the latest release version string
                auto latestVersion = releases["tag_name"].get<std::string_view>();

                // Check if the latest release is different from the current version
                if (latestVersion != currVersion)
                    ImHexApi::System::impl::addInitArgument("update-available", latestVersion.data());

                // Check if there is a telemetry uuid
                auto uuid = ContentRegistry::Settings::read<std::string>("hex.builtin.setting.general", "hex.builtin.setting.general.uuid", "");
                if (uuid.empty()) {
                    // Generate a new uuid
                    uuid = wolv::hash::generateUUID();
                    // Save
                    ContentRegistry::Settings::write<std::string>("hex.builtin.setting.general", "hex.builtin.setting.general.uuid", uuid);
                }

                TaskManager::createBackgroundTask("hex.builtin.task.sending_statistics"_lang, [uuid](auto&) {
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
            }
            return true;
        }

        bool checkForUpdates() {
            TaskManager::createBackgroundTask("hex.builtin.task.check_updates"_lang, [](auto&) { checkForUpdatesSync(); });
            return true;
        }

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

                properties.maximized = ContentRegistry::Settings::read<bool>("hex.builtin.setting.interface", "hex.builtin.setting.interface.window.maximized", 0);
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
        ImHexApi::System::addStartupTask("Checking for updates", true, checkForUpdates);
    }
}