#include <hex/api/imhex_api/fonts.hpp>

#include <fonts/fonts.hpp>
#include <romfs/romfs.hpp>

namespace hex::fonts {

    const ImHexApi::Fonts::Font& Default() {
        static auto font = ImHexApi::Fonts::Font("hex.fonts.font.default"_unlocalized);
        return font;
    }
    const ImHexApi::Fonts::Font& HexEditor()  {
        static auto font = ImHexApi::Fonts::Font("hex.fonts.font.hex_editor"_unlocalized);
        return font;
    }
    const ImHexApi::Fonts::Font& CodeEditor() {
        static auto font = ImHexApi::Fonts::Font("hex.fonts.font.code_editor"_unlocalized);
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

        // Registered ahead of Unifont so they take priority for the codepoints they
        // cover; a merge font earlier in this list wins ties. Some emoji fall in the
        // BMP, where Unifont would otherwise win and show a monochrome glyph instead.
        ImHexApi::Fonts::registerMergeFont("Noto Color Emoji",             romfs::get("fonts/NotoColorEmoji.ttf").span<u8>(),               { .x=+0.0F, .y=+0.0F }, 1.0F, true);
        ImHexApi::Fonts::registerMergeFont("Noto Sans Egyptian Hieroglyphs", romfs::get("fonts/NotoSansEgyptianHieroglyphs.ttf").span<u8>(), { .x=+0.0F, .y=+0.0F }, 1.0F);
        ImHexApi::Fonts::registerMergeFont("Noto Sans Cuneiform",          romfs::get("fonts/NotoSansCuneiform.ttf").span<u8>(),            { .x=+0.0F, .y=+0.0F }, 1.0F);
        ImHexApi::Fonts::registerMergeFont("Noto Sans Anatolian Hieroglyphs", romfs::get("fonts/NotoSansAnatolianHieroglyphs.ttf").span<u8>(), { .x=+0.0F, .y=+0.0F }, 1.0F);

        ImHexApi::Fonts::registerMergeFont("Unifont",        romfs::get("fonts/unifont.otf").span<u8>(),      { .x=+0.0F, .y=+0.0F }, 0.75F);

        // Long-tail fallback for every other historic script and symbol block Unifont
        // covers but the fonts above do not.
        ImHexApi::Fonts::registerMergeFont("Unifont Upper",  romfs::get("fonts/unifont_upper.otf").span<u8>(), { .x=+0.0F, .y=+0.0F }, 0.75F);
    }

}
