#include <hex/plugin.hpp>

#include <hex/api/events/events_gui.hpp>
#include <hex/helpers/logger.hpp>

#include <romfs/romfs.hpp>
#include <font_settings.hpp>
#include <fonts/fonts.hpp>
#include <hex/api/task_manager.hpp>
#include <hex/api/events/events_lifecycle.hpp>

namespace hex::fonts {

    void registerUIFonts();
    void registerMergeFonts();

    namespace loader {
        bool loadFonts();
    }

}


IMHEX_LIBRARY_SETUP("Fonts") {
    hex::log::debug("Using romfs: '{}'", romfs::name());
    hex::LocalizationManager::addLanguages(romfs::get("lang/languages.json").string(), [](const std::filesystem::path &path) {
        return romfs::get(path).string();
    });

    hex::fonts::registerUIFonts();
    hex::fonts::registerMergeFonts();

    hex::EventImHexStartupFinished::subscribe([] {
        hex::fonts::loader::loadFonts();
        hex::ImHexApi::Fonts::setDefaultFont(hex::fonts::Default());
    });

    hex::EventDPIChanged::subscribe([](float, float newScale) {
        ImGui::GetIO().ConfigDpiScaleFonts = newScale;
    });
}