#include <hex/plugin.hpp>

#include <hex/api/content_registry.hpp>
#include <hex/helpers/logger.hpp>

#include <romfs/romfs.hpp>

#include "content/views/view_diff.hpp"


namespace hex::plugin::diffing {

    void registerDiffingAlgorithms();

}

using namespace hex;
using namespace hex::plugin::diffing;

IMHEX_PLUGIN_SETUP("Diffing", "WerWolv", "Support for diffing data") {
    hex::log::debug("Using romfs: '{}'", romfs::name());

    for (auto &path : romfs::list("lang"))
        hex::ContentRegistry::Language::addLocalization(nlohmann::json::parse(romfs::get(path).string()));

    registerDiffingAlgorithms();

    ContentRegistry::Views::add<ViewDiff>();

}