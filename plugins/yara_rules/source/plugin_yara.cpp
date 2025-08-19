#include <hex/plugin.hpp>

#include <hex/api/content_registry/views.hpp>
#include <hex/helpers/logger.hpp>

#include <romfs/romfs.hpp>

#include "content/views/view_yara.hpp"

using namespace hex;
using namespace hex::plugin::yara;

namespace hex::plugin::yara {

    void registerDataInformationSections();
    void registerViews() {
        ContentRegistry::Views::add<ViewYara>();
    }

}

IMHEX_PLUGIN_SETUP("Yara Rules", "WerWolv", "Support for matching Yara rules") {
    hex::log::debug("Using romfs: '{}'", romfs::name());
    hex::LocalizationManager::addLanguages(romfs::get("lang/languages.json").string(), [](const std::filesystem::path &path) {
        return romfs::get(path).string();
    });

    registerViews();
    registerDataInformationSections();
}
