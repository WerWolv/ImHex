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
        if (RegOpenKey(HKEY_CURRENT_USER, R"(Software\Microsoft\Windows\CurrentVersion\Themes\Personalize)", &hkey) == ERROR_SUCCESS) {
            DWORD value = 0;
            DWORD size  = sizeof(DWORD);

            auto error = RegQueryValueEx(hkey, "AppsUseLightTheme", nullptr, nullptr, reinterpret_cast<LPBYTE>(&value), &size);
            if (error == ERROR_SUCCESS) {
                RequestChangeTheme::post(value == 0 ? "Dark" : "Light");
            } else {
                ImHexApi::System::impl::setBorderlessWindowMode(false);
            }
        } else {
            ImHexApi::System::impl::setBorderlessWindowMode(false);
        }
    });

    EventWindowInitialized::subscribe([=] {
        bool themeFollowSystem = ContentRegistry::Settings::read<std::string>("hex.builtin.setting.interface", "hex.builtin.setting.interface.color", ThemeManager::NativeTheme) == ThemeManager::NativeTheme;

        if (themeFollowSystem)
            EventOSThemeChanged::post();
    });
}

static void checkBorderlessWindowOverride() {
    bool borderlessWindowForced = ContentRegistry::Settings::read<bool>("hex.builtin.setting.interface", "hex.builtin.setting.interface.force_borderless_window_mode", false);

    if (borderlessWindowForced)
        ImHexApi::System::impl::setBorderlessWindowMode(true);
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
    checkBorderlessWindowOverride();
}
