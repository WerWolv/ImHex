#include <hex/api/imhex_api.hpp>

#include <fonts/fonts.hpp>

#include <romfs/romfs.hpp>

#include <hex/helpers/utils.hpp>

#include <fonts/vscode_icons.hpp>
#include <fonts/blender_icons.hpp>

namespace hex::fonts {

    static auto s_defaultFont = ImHexApi::Fonts::Font("hex.fonts.font.default");
    const ImHexApi::Fonts::Font& Default()    { return s_defaultFont; }
    static auto s_hexEditorFont = ImHexApi::Fonts::Font("hex.fonts.font.hex_editor");
    const ImHexApi::Fonts::Font& HexEditor()  { return s_hexEditorFont; }
    static auto s_codeEditorFont = ImHexApi::Fonts::Font("hex.fonts.font.code_editor");
    const ImHexApi::Fonts::Font& CodeEditor() { return s_codeEditorFont; }

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
            { 0, -2 });

        ImHexApi::Fonts::loadFont("Unifont", romfs::get("fonts/unifont.otf").span<u8>(), { }, {}, 0, false, 16);
    }

}