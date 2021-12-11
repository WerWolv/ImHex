#include <hex/api/content_registry.hpp>
#include <hex/api/event.hpp>

#include "content/providers/gdb_provider.hpp"
#include "content/providers/file_provider.hpp"
#include "content/providers/disk_provider.hpp"

namespace hex::plugin::builtin {

    void registerProviders() {
        ContentRegistry::Provider::add("hex.builtin.provider.gdb");
        ContentRegistry::Provider::add("hex.builtin.provider.disk");

        (void) EventManager::subscribe<RequestCreateProvider>([](const std::string &unlocalizedName, hex::prv::Provider **provider){
            if (unlocalizedName != "hex.builtin.provider.file") return;

            auto newProvider = new prv::FileProvider();

            hex::ImHexApi::Provider::add(newProvider);

            if (provider != nullptr)
                *provider = newProvider;
        });

        (void) EventManager::subscribe<RequestCreateProvider>([](const std::string &unlocalizedName, hex::prv::Provider **provider){
            if (unlocalizedName != "hex.builtin.provider.gdb") return;

            auto newProvider = new prv::GDBProvider();

            hex::ImHexApi::Provider::add(newProvider);

            if (provider != nullptr)
                *provider = newProvider;
        });

        (void) EventManager::subscribe<RequestCreateProvider>([](const std::string &unlocalizedName, hex::prv::Provider **provider){
            if (unlocalizedName != "hex.builtin.provider.disk") return;

            auto newProvider = new prv::DiskProvider();

            hex::ImHexApi::Provider::add(newProvider);

            if (provider != nullptr)
                *provider = newProvider;
        });
    }

}