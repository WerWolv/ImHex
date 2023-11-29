#include <imgui_internal.h>
#include <hex/api/imhex_api.hpp>

#include <romfs/romfs.hpp>

#include <hex/helpers/utils.hpp>

#include <fonts/fontawesome_font.h>
#include <fonts/codicons_font.h>

#include <imgui_freetype.h>

namespace hex::plugin::builtin {

    void loadFonts() {
        using namespace ImHexApi::Fonts;

        /**
         *  !!IMPORTANT!!
         *  Always load the font files in decreasing size of their glyphs. This will make the rasterize be more
         *  efficient when packing the glyphs into the font atlas and therefor make the atlas much smaller.
         */

        ImHexApi::Fonts::loadFont("Font Awesome 5", romfs::get("fonts/fontawesome.otf").span<u8>(),
            {
                { glyph(ICON_FA_BACKSPACE), glyph(ICON_FA_INFINITY), glyph(ICON_FA_TACHOMETER_ALT), glyph(ICON_FA_MICROCHIP), glyph(ICON_FA_CODE_BRANCH) }
            },
            { 0, 0 });

        ImHexApi::Fonts::loadFont("VS Codicons", romfs::get("fonts/codicons.ttf").span<u8>(),
            {
                { ICON_MIN_VS, ICON_MAX_VS }
            },
            { 0, -1_scaled });

        ImHexApi::Fonts::loadFont("Unifont", romfs::get("fonts/unifont.otf").span<u8>());
    }

}