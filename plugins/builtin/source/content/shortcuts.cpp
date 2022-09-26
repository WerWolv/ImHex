#include <hex/api/keybinding.hpp>
#include <hex/api/event.hpp>

#include <hex/providers/provider.hpp>

namespace hex::plugin::builtin {

    void registerShortcuts() {
        // New file
        ShortcutManager::addGlobalShortcut(CTRL + Keys::N, [] {
            EventManager::post<RequestOpenWindow>("Create File");
        });

        // Open file
        ShortcutManager::addGlobalShortcut(CTRL + Keys::O, [] {
            EventManager::post<RequestOpenWindow>("Open File");
        });

        // Close file
        ShortcutManager::addGlobalShortcut(CTRL + Keys::W, [] {
            if (ImHexApi::Provider::isValid())
                ImHexApi::Provider::remove(ImHexApi::Provider::get());
        });

        // Reload file
        ShortcutManager::addGlobalShortcut(CTRL + Keys::R, [] {
            if (ImHexApi::Provider::isValid()) {
                auto provider = ImHexApi::Provider::get();

                provider->close();
                if (!provider->open())
                    ImHexApi::Provider::remove(provider, true);
            }
        });
    }

}