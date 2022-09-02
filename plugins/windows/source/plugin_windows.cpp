#include <hex/plugin.hpp>

#include <hex/api/content_registry.hpp>

#include <nlohmann/json.hpp>

#include "views/view_tty_console.hpp"

using namespace hex;

namespace hex::plugin::windows {

    void registerLanguageEnUS();
    void registerLanguageDeDE();
    void registerLanguageZhCN();
    void registerLanguagePtBR();
    void registerLanguageZhTW();
    void registerLanguageKoKR();

    void addFooterItems();
    void addTitleBarButtons();
    void registerSettings();
}

static void detectSystemTheme() {
    // Setup system theme change detector
    EventManager::subscribe<EventOSThemeChanged>([] {
        bool themeFollowSystem = ContentRegistry::Settings::getSetting("hex.builtin.setting.interface", "hex.builtin.setting.interface.color") == 0;
        if (!themeFollowSystem)
            return;

        HKEY hkey;
        if (RegOpenKey(HKEY_CURRENT_USER, R"(Software\Microsoft\Windows\CurrentVersion\Themes\Personalize)", &hkey) == ERROR_SUCCESS) {
            DWORD value = 0;
            DWORD size  = sizeof(DWORD);

            auto error = RegQueryValueEx(hkey, "AppsUseLightTheme", nullptr, nullptr, reinterpret_cast<LPBYTE>(&value), &size);
            if (error == ERROR_SUCCESS) {
                EventManager::post<RequestChangeTheme>(value == 0 ? 1 : 2);
            } else {
                ImHexApi::System::impl::setBorderlessWindowMode(false);
            }
        } else {
            ImHexApi::System::impl::setBorderlessWindowMode(false);
        }
    });

    EventManager::subscribe<EventWindowInitialized>([=] {
        bool themeFollowSystem = ContentRegistry::Settings::getSetting("hex.builtin.setting.interface", "hex.builtin.setting.interface.color") == 0;

        if (themeFollowSystem)
            EventManager::post<EventOSThemeChanged>();
    });
}

static void checkBorderlessWindowOverride() {
    bool borderlessWindowForced = ContentRegistry::Settings::read("hex.builtin.setting.interface", "hex.builtin.setting.interface.force_borderless_window_mode", 0) != 0;

    if (borderlessWindowForced)
        ImHexApi::System::impl::setBorderlessWindowMode(true);
}

IMHEX_PLUGIN_SETUP("Windows", "WerWolv", "Windows-only features") {
    using namespace hex::plugin::windows;

    registerLanguageEnUS();
    registerLanguageDeDE();
    registerLanguageZhCN();
    registerLanguagePtBR();
    registerLanguageZhTW();
    registerLanguageKoKR();
    
    hex::ContentRegistry::Views::add<ViewTTYConsole>();

    addFooterItems();
    addTitleBarButtons();
    registerSettings();

    detectSystemTheme();
    checkBorderlessWindowOverride();
}
