#include <hex/api/imhex_api.hpp>

#include <romfs/romfs.hpp>

#include <hex/helpers/utils.hpp>

#include <fonts/vscode_icons.hpp>
#include <fonts/blender_icons.hpp>

namespace hex::fonts {

    void registerFonts() {
        using namespace ImHexApi::Fonts;

        /**
         *  !!IMPORTANT!!
         *  Always load the font files in decreasing size of their glyphs. This will make the rasterize be more
         *  efficient when packing the glyphs into the font atlas and therefor make the atlas much smaller.
         */

        ImHexApi::Fonts::loadFont("Blender Icons",  romfs::get("fonts/blendericons.ttf").span<u8>(),{ { ICON_MIN_BI, ICON_MAX_BI } }, { -1_scaled, -1_scaled });

        ImHexApi::Fonts::loadFont("VS Codicons", romfs::get("fonts/codicons.ttf").span<u8>(),
            {
                { ICON_MIN_VS, ICON_MAX_VS }
            },
            { -1_scaled, -1_scaled });

        ImHexApi::Fonts::loadFont("Unifont", romfs::get("fonts/unifont.otf").span<u8>(), { }, {}, 0, 16);
    }

}