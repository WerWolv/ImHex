#include <hex/plugin.hpp>

#include <hex/api/content_registry.hpp>
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
    for (auto &path : romfs::list("lang"))
        hex::ContentRegistry::Language::addLocalization(nlohmann::json::parse(romfs::get(path).string()));

    registerPatternLanguageVisualizers();
    registerPatternLanguageInlineVisualizers();
}
