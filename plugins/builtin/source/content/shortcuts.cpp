#include <hex/api/keybinding.hpp>
#include <hex/api/event.hpp>

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
            ImHexApi::Provider::remove(ImHexApi::Provider::get());
        });
    }

}