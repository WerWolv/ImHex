#include <hex/plugin.hpp>

#include <hex/api/localization_manager.hpp>
#include <hex/helpers/logger.hpp>

#include <romfs/romfs.hpp>

namespace hex::plugin::visualizers {

    void registerPatternLanguageVisualizers();
    void registerPatternLanguageInlineVisualizers();

}

using namespace hex;
using namespace hex::plugin::visualizers;

IMHEX_PLUGIN_SETUP("Visualizers", "WerWolv", "Visualizers for the Pattern Language") {
    hex::log::debug("Using romfs: '{}'", romfs::name());
    hex::LocalizationManager::addLanguages(romfs::get("lang/languages.json").string(), [](const std::filesystem::path &path) {
        return romfs::get(path).string();
    });

    registerPatternLanguageVisualizers();
    registerPatternLanguageInlineVisualizers();
}
