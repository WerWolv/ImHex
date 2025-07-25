#include <hex/api/imhex_api.hpp>

#include <fonts/fonts.hpp>
#include <romfs/romfs.hpp>

namespace hex::fonts {

    static auto s_defaultFont = ImHexApi::Fonts::Font("hex.fonts.font.default");
    const ImHexApi::Fonts::Font& Default()    { return s_defaultFont; }
    static auto s_hexEditorFont = ImHexApi::Fonts::Font("hex.fonts.font.hex_editor");
    const ImHexApi::Fonts::Font& HexEditor()  { return s_hexEditorFont; }
    static auto s_codeEditorFont = ImHexApi::Fonts::Font("hex.fonts.font.code_editor");
    const ImHexApi::Fonts::Font& CodeEditor() { return s_codeEditorFont; }

    void registerFonts() {
        ImHexApi::Fonts::loadFont("Blender Icons",  romfs::get("fonts/blendericons.ttf").span<u8>(), { -1, -1 });
        ImHexApi::Fonts::loadFont("VS Codicons",    romfs::get("fonts/codicons.ttf").span<u8>(),     {  0, -2 });
        ImHexApi::Fonts::loadFont("Unifont",        romfs::get("fonts/unifont.otf").span<u8>(),      {  0,  0 }, 10);
    }

}