#include <hex/plugin.hpp>

#include <loaders/dotnet/dotnet_loader.hpp>

#include <hex/helpers/logger.hpp>
#include <romfs/romfs.hpp>

using namespace hex;

using PluginLoaders = std::tuple<
    plugin::loader::DotNetLoader
>;

namespace {

    void callPluginEntryPoints(auto &loader) {
        for (const auto &entryPoint : loader.getPluginEntryPoints()) {
            entryPoint();
        }
    }

    void loadPlugin(auto &loader) {
        loader.loadAll();
        callPluginEntryPoints(loader);
    }

    void loadAllPlugins() {
        std::apply([](auto&&... args) {
            try {
                (loadPlugin(args), ...);
            } catch (const std::exception &exception) {
                log::error("Failed to load plugin: {}", exception.what());
            }
        }, PluginLoaders{});
    }

}

IMHEX_PLUGIN_SETUP("Managed Plugin Loader", "WerWolv", "Plugin loader for C# plugins") {
    hex::log::debug("Using romfs: '{}'", romfs::name());

    loadAllPlugins();
}
