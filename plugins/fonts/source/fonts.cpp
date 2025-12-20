#include <hex/api/imhex_api/fonts.hpp>

#include <fonts/fonts.hpp>
#include <romfs/romfs.hpp>

namespace hex::fonts {

    const ImHexApi::Fonts::Font& Default() {
        static auto font = ImHexApi::Fonts::Font("hex.fonts.font.default");
        return font;
    }
    const ImHexApi::Fonts::Font& HexEditor()  {
        static auto font = ImHexApi::Fonts::Font("hex.fonts.font.hex_editor");
        return font;
    }
    const ImHexApi::Fonts::Font& CodeEditor() {
        static auto font = ImHexApi::Fonts::Font("hex.fonts.font.code_editor");
        return font;
    }

    void registerUIFonts() {
        ImHexApi::Fonts::registerFont(Default());
        ImHexApi::Fonts::registerFont(HexEditor());
        ImHexApi::Fonts::registerFont(CodeEditor());
    }

    void registerMergeFonts() {
        ImHexApi::Fonts::registerMergeFont("Blender Icons",  romfs::get("fonts/blendericons.ttf").span<u8>(), { .x=-1.0F, .y=-1.0F }, 0.95F);
        ImHexApi::Fonts::registerMergeFont("VS Codicons",    romfs::get("fonts/codicons.ttf").span<u8>(),     { .x=+0.0F, .y=-2.5F }, 0.95F);
        ImHexApi::Fonts::registerMergeFont("Tabler Icons",   romfs::get("fonts/tablericons.ttf").span<u8>(), { .x=+2.0F, .y=-1.5F }, 1.10F);
        ImHexApi::Fonts::registerMergeFont("Unifont",        romfs::get("fonts/unifont.otf").span<u8>(),      { .x=+0.0F, .y=+0.0F }, 0.75F);
    }

}