#include <hex/plugin.hpp>

#include <hex/api/content_registry.hpp>
#include <hex/api/theme_manager.hpp>
#include <hex/helpers/logger.hpp>

#include <romfs/romfs.hpp>
#include <nlohmann/json.hpp>

#include "views/view_tty_console.hpp"

#include <windows.h>

using namespace hex;

namespace hex::plugin::windows {

    void addFooterItems();
    void addTitleBarButtons();
    void registerSettings();
}

static void detectSystemTheme() {
    // Setup system theme change detector
    EventOSThemeChanged::subscribe([] {
        bool themeFollowSystem = ContentRegistry::Settings::read<std::string>("hex.builtin.setting.interface", "hex.builtin.setting.interface.color", ThemeManager::NativeTheme) == ThemeManager::NativeTheme;
        if (!themeFollowSystem)
            return;

        HKEY hkey;
        if (RegOpenKeyW(HKEY_CURRENT_USER, LR"(Software\Microsoft\Windows\CurrentVersion\Themes\Personalize)", &hkey) == ERROR_SUCCESS) {
            DWORD value = 0;
            DWORD size  = sizeof(DWORD);

            auto error = RegQueryValueExW(hkey, L"AppsUseLightTheme", nullptr, nullptr, reinterpret_cast<LPBYTE>(&value), &size);
            if (error == ERROR_SUCCESS) {
                RequestChangeTheme::post(value == 0 ? "Dark" : "Light");
            }
        }
    });

    EventWindowInitialized::subscribe([=] {
        bool themeFollowSystem = ContentRegistry::Settings::read<std::string>("hex.builtin.setting.interface", "hex.builtin.setting.interface.color", ThemeManager::NativeTheme) == ThemeManager::NativeTheme;

        if (themeFollowSystem)
            EventOSThemeChanged::post();
    });
}

IMHEX_PLUGIN_SETUP("Windows", "WerWolv", "Windows-only features") {
    using namespace hex::plugin::windows;

    hex::log::debug("Using romfs: '{}'", romfs::name());
    for (auto &path : romfs::list("lang"))
        hex::ContentRegistry::Language::addLocalization(nlohmann::json::parse(romfs::get(path).string()));

    hex::ContentRegistry::Views::add<ViewTTYConsole>();

    addFooterItems();
    registerSettings();

    detectSystemTheme();
}
