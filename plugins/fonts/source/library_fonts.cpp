#include <hex/plugin.hpp>

#include <hex/api/content_registry.hpp>
#include <hex/helpers/logger.hpp>

#include <romfs/romfs.hpp>

namespace hex::fonts {
    void loadFonts();
}

IMHEX_LIBRARY_SETUP("Fonts") {
    hex::log::debug("Using romfs: '{}'", romfs::name());

    hex::fonts::loadFonts();
}