#include <hex/plugin.hpp>

#include <hex/api/content_registry.hpp>
#include <hex/helpers/logger.hpp>

#include <romfs/romfs.hpp>

namespace hex::fonts {
    void registerFonts();

    bool buildFontAtlas();
}

IMHEX_LIBRARY_SETUP("Fonts") {
    hex::log::debug("Using romfs: '{}'", romfs::name());

    hex::ImHexApi::System::addStartupTask("Loading fonts", true, hex::fonts::buildFontAtlas);
    hex::fonts::registerFonts();
}