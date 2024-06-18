#include <hex/api/content_registry.hpp>

#include "content/providers/gdb_provider.hpp"
#include "content/providers/file_provider.hpp"
#include "content/providers/null_provider.hpp"
#include "content/providers/disk_provider.hpp"
#include "content/providers/intel_hex_provider.hpp"
#include "content/providers/motorola_srec_provider.hpp"
#include "content/providers/memory_file_provider.hpp"
#include "content/providers/view_provider.hpp"
#include <content/providers/process_memory_provider.hpp>
#include <content/providers/base64_provider.hpp>
#include <popups/popup_notification.hpp>

#include <hex/api/project_file_manager.hpp>
#include <hex/api/task_manager.hpp>
#include <hex/helpers/fmt.hpp>

#include <nlohmann/json.hpp>
#include <toasts/toast_notification.hpp>

#include <wolv/utils/guards.hpp>

namespace hex::plugin::builtin {

    void registerProviders() {

        ContentRegistry::Provider::add<FileProvider>(false);
        ContentRegistry::Provider::add<NullProvider>(false);
        #if !defined(OS_WEB)
            ContentRegistry::Provider::add<DiskProvider>();
        #endif
        ContentRegistry::Provider::add<GDBProvider>();
        ContentRegistry::Provider::add<IntelHexProvider>();
        ContentRegistry::Provider::add<MotorolaSRECProvider>();
        ContentRegistry::Provider::add<Base64Provider>();
        ContentRegistry::Provider::add<MemoryFileProvider>(false);
        ContentRegistry::Provider::add<ViewProvider>(false);

        #if defined(OS_WINDOWS) || defined(OS_MACOS) || (defined(OS_LINUX) && !defined(OS_FREEBSD))
            ContentRegistry::Provider::add<ProcessMemoryProvider>();
        #endif

        ProjectFile::registerHandler({
            .basePath = "providers",
            .required = true,
            .load = [](const std::fs::path &basePath, const Tar &tar) {
                auto json = nlohmann::json::parse(tar.readString(basePath / "providers.json"));
                auto providerIds = json.at("providers").get<std::vector<int>>();

                bool success = true;
                std::map<hex::prv::Provider*, std::string> providerWarnings;
                for (const auto &id : providerIds) {
                    auto providerSettings = nlohmann::json::parse(tar.readString(basePath / hex::format("{}.json", id)));

                    auto providerType = providerSettings.at("type").get<std::string>();
                    auto newProvider = ImHexApi::Provider::createProvider(providerType, true, false);
                    ON_SCOPE_EXIT {
                        if (!success) {
                            for (auto &task : TaskManager::getRunningTasks())
                                task->interrupt();

                            TaskManager::runWhenTasksFinished([]{
                                for (const auto &provider : ImHexApi::Provider::getProviders())
                                    ImHexApi::Provider::remove(provider, true);
                            });
                        }
                    };

                    if (newProvider == nullptr) {
                        // If a provider is not created, it will be overwritten when saving the project,
                        // so we should prevent the project from loading at all
                        ui::ToastError::open(
                            hex::format("hex.builtin.popup.error.project.load"_lang,
                                hex::format("hex.builtin.popup.error.project.load.create_provider"_lang, providerType)
                            )
                        );
                        success = false;
                        break;
                    }

                    newProvider->setID(id);
                    bool loaded = false;
                    try {
                        newProvider->loadSettings(providerSettings.at("settings"));
                        loaded = true;
                    } catch (const std::exception &e){
                            providerWarnings[newProvider] = e.what();
                    }
                    if (loaded) {
                        if (!newProvider->open() || !newProvider->isAvailable() || !newProvider->isReadable()) {
                            providerWarnings[newProvider] = newProvider->getErrorMessage();
                        } else {
                            EventProviderOpened::post(newProvider);
                        }
                    }
                }

                std::string warningMessage;
                for (const auto &warning : providerWarnings){
                    ImHexApi::Provider::remove(warning.first);
                    warningMessage.append(
                        hex::format("\n - {} : {}", warning.first->getName(), warning.second));
                }

                // If no providers were opened, display an error with
                // the warnings that happened when opening them 
                if (ImHexApi::Provider::getProviders().empty()) {
                    ui::ToastError::open(hex::format("{}{}", "hex.builtin.popup.error.project.load"_lang, "hex.builtin.popup.error.project.load.no_providers"_lang, warningMessage));

                    return false;
                } else {
                    // Else, if there are warnings, still display them
                    if (warningMessage.empty()) {
                        return true;
                    } else {
                        ui::ToastWarning::open(hex::format("hex.builtin.popup.error.project.load.some_providers_failed"_lang, warningMessage));
                    }

                    return success;
                }
            },
            .store = [](const std::fs::path &basePath, const Tar &tar) {
                std::vector<int> providerIds;
                for (const auto &provider : ImHexApi::Provider::getProviders()) {
                    auto id = provider->getID();
                    providerIds.push_back(id);

                    nlohmann::json json;
                    json["type"] = provider->getTypeName();
                    json["settings"] = provider->storeSettings({});

                    tar.writeString(basePath / hex::format("{}.json", id), json.dump(4));
                }

                tar.writeString(basePath / "providers.json",
                    nlohmann::json({ { "providers", providerIds } }).dump(4)
                );

                return true;
            }
        });
    }

}
