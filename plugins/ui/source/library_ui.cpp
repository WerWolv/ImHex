#include <hex/plugin.hpp>

#include <hex/api/content_registry.hpp>
#include <hex/helpers/logger.hpp>

#include <romfs/romfs.hpp>

#include <ui/hex_editor.hpp>
#include <ui/pattern_drawer.hpp>
#include <ui/widgets.hpp>

IMHEX_LIBRARY_SETUP("UI") {
    hex::log::debug("Using romfs: '{}'", romfs::name());
    for (auto &path : romfs::list("lang"))
        hex::ContentRegistry::Language::addLocalization(nlohmann::json::parse(romfs::get(path).string()));
}