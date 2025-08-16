#include <hex/plugin.hpp>

#include <hex/helpers/logger.hpp>

#include <romfs/romfs.hpp>

#include <ui/hex_editor.hpp>
#include <ui/pattern_drawer.hpp>
#include <ui/widgets.hpp>

IMHEX_LIBRARY_SETUP("UI") {
    hex::log::debug("Using romfs: '{}'", romfs::name());
    hex::LocalizationManager::addLanguages(romfs::get("lang/languages.json").string(), [](const std::filesystem::path &path) {
        return romfs::get(path).string();
    });
}