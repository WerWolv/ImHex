#include <hex/api/keybinding.hpp>
#include <hex/api/event.hpp>

namespace hex::plugin::builtin {

    void registerShortcuts() {
        // Open file
        ShortcutManager::addGlobalShortcut(CTRL + Keys::O, [] {
            fs::openFileBrowser(fs::DialogMode::Open, {}, [](const auto &path) {
                EventManager::post<RequestOpenFile>(path);
            });
        });

        // Close file
        ShortcutManager::addGlobalShortcut(CTRL + Keys::W, [] {
            ImHexApi::Provider::remove(ImHexApi::Provider::get());
        });
    }

}