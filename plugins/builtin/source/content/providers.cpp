#include <hex/api/content_registry.hpp>

#include "content/providers/gdb_provider.hpp"
#include "content/providers/file_provider.hpp"
#include "content/providers/null_provider.hpp"
#include "content/providers/disk_provider.hpp"
#include "content/providers/intel_hex_provider.hpp"
#include "content/providers/motorola_srec_provider.hpp"

#include <hex/api/project_file_manager.hpp>
#include <nlohmann/json.hpp>
#include <hex/helpers/fmt.hpp>

namespace hex::plugin::builtin {

    void registerProviders() {

        ContentRegistry::Provider::add<prv::FileProvider>(false);
        ContentRegistry::Provider::add<prv::NullProvider>(false);
        ContentRegistry::Provider::add<prv::DiskProvider>();
        ContentRegistry::Provider::add<prv::GDBProvider>();
        ContentRegistry::Provider::add<prv::IntelHexProvider>();
        ContentRegistry::Provider::add<prv::MotorolaSRECProvider>();

        ProjectFile::registerHandler({
             .basePath = "providers",
             .required = true,
             .load = [](const std::fs::path &basePath, Tar &tar) {
                 auto json = nlohmann::json::parse(tar.readString(basePath / "providers.json"));
                 auto providerIds = json["providers"].get<std::vector<int>>();

                 bool success = true;
                 for (const auto &id : providerIds) {
                     auto providerSettings = nlohmann::json::parse(tar.readString(basePath / hex::format("{}.json", id)));

                     auto provider = ImHexApi::Provider::createProvider(providerSettings["type"].get<std::string>(), true);
                     ON_SCOPE_EXIT { if (!success) ImHexApi::Provider::remove(provider, true); };
                     if (provider == nullptr) {
                         success = false;
                         continue;
                     }

                     provider->loadSettings(providerSettings["settings"]);
                     if (!provider->open() || !provider->isAvailable() || !provider->isReadable())
                         success = false;
                     else
                         EventManager::post<EventProviderOpened>(provider);
                 }

                 return success;
             },
             .store = [](const std::fs::path &basePath, Tar &tar) {
                 std::vector<int> providerIds;
                 for (const auto &provider : ImHexApi::Provider::getProviders()) {
                     auto id = provider->getID();
                     providerIds.push_back(id);

                     nlohmann::json json;
                     json["type"] = provider->getTypeName();
                     json["settings"] = provider->storeSettings();

                     tar.write(basePath / hex::format("{}.json", id), json.dump(4));
                 }

                 tar.write(basePath / "providers.json",
                    nlohmann::json({ {"providers", providerIds } }).dump(4)
                 );

                 return true;
             }
        });
    }

}