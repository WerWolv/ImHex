#include <hex/api/keybinding.hpp>
#include <hex/api/event.hpp>

#include <hex/providers/provider.hpp>
#include "content/global_actions.hpp"

namespace hex::plugin::builtin {

    void registerShortcuts() {
        // New file
        ShortcutManager::addGlobalShortcut(CTRLCMD + Keys::N, [] {
            EventManager::post<RequestOpenWindow>("Create File");
        });

        // Open file
        ShortcutManager::addGlobalShortcut(CTRLCMD + Keys::O, [] {
            EventManager::post<RequestOpenWindow>("Open File");
        });

        // Close file
        ShortcutManager::addGlobalShortcut(CTRLCMD + Keys::W, [] {
            if (ImHexApi::Provider::isValid())
                ImHexApi::Provider::remove(ImHexApi::Provider::get());
        });

        // Reload file
        ShortcutManager::addGlobalShortcut(CTRLCMD + Keys::R, [] {
            if (ImHexApi::Provider::isValid()) {
                auto provider = ImHexApi::Provider::get();

                provider->close();
                if (!provider->open())
                    ImHexApi::Provider::remove(provider, true);
            }
        });

        // Save project
        ShortcutManager::addGlobalShortcut(ALT + Keys::S, [] {
            saveProject();
        });

        // Save project as...
        ShortcutManager::addGlobalShortcut(ALT + SHIFT + Keys::S, [] {
            saveProjectAs();
        });
    }

}