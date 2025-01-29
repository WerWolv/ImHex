#include <imgui.h>
#include <imgui_internal.h>
#include <list>

#include <hex/api/imhex_api.hpp>
#include <hex/api/content_registry.hpp>

#include <hex/helpers/logger.hpp>
#include <hex/helpers/fs.hpp>
#include <hex/helpers/utils.hpp>

#include <wolv/utils/string.hpp>
#include <freetype/freetype.h>
#include "imgui_impl_opengl3_loader.h"

#include <font_atlas.hpp>
#include <string.h>

namespace hex::fonts {

    bool buildFontAtlas(FontAtlas *fontAtlas, std::fs::path fontPath, bool pixelPerfectFont, float fontSize, bool loadUnicodeCharacters, bool bold, bool italic, bool antialias) {
        if (fontAtlas == nullptr) {
            return false;
        }
        FT_Library ft = nullptr;
        if (FT_Init_FreeType(&ft) != 0) {
            log::fatal("Failed to initialize FreeType");
                    return false;
        }
        fontAtlas->reset();
        bool result = false;
        u32 fontIndex = 0;
        auto io = ImGui::GetIO();
        io.Fonts = fontAtlas->getAtlas();
        // Check if Unicode support is enabled in the settings and that the user doesn't use the No GPU version on Windows
        // The Mesa3D software renderer on Windows identifies itself as "VMware, Inc."
        bool shouldLoadUnicode =
                ContentRegistry::Settings::read<bool>("hex.fonts.setting.font", "hex.builtin.fonts.font.load_all_unicode_chars", false) &&
                ImHexApi::System::getGPUVendor() != "VMware, Inc.";

        if (!loadUnicodeCharacters)
            shouldLoadUnicode = false;

        fontAtlas->enableUnicodeCharacters(shouldLoadUnicode);

        // If a custom font is set in the settings, load the rest of the settings as well
        if (!pixelPerfectFont) {
            fontAtlas->setBold(bold);
            fontAtlas->setItalic(italic);
            fontAtlas->setAntiAliasing(antialias);
        } else {
            fontPath.clear();
        }

        // Try to load the custom font if one was set
        std::optional<Font> defaultFont;
        if (!fontPath.empty()) {
            defaultFont = fontAtlas->addFontFromFile(fontPath, fontSize, true, ImVec2());
            std::string defaultFontName = defaultFont.has_value() ? fontPath.filename().string() : "Custom Font";
            memcpy(fontAtlas->getAtlas()->ConfigData[fontIndex].Name, defaultFontName.c_str(), defaultFontName.size());
            fontIndex += 1;
            //if (!fontAtlas->build()) {
            //    log::error("Failed to load custom font '{}'! Falling back to default font", wolv::util::toUTF8String(fontPath));
            //    defaultFont.reset();
            // }
        }

        // If there's no custom font set, or it failed to load, fall back to the default font
        if (!defaultFont.has_value()) {
            if (pixelPerfectFont) {
                fontSize = std::max(1.0F, std::floor(ImHexApi::System::getGlobalScale() * ImHexApi::System::getBackingScaleFactor() * 13.0F));
                defaultFont = fontAtlas->addDefaultFont();
                std::string defaultFontName = "Proggy Clean";
                memcpy(fontAtlas->getAtlas()->ConfigData[fontIndex].Name, defaultFontName.c_str(), defaultFontName.size());
                fontIndex += 1;
            } else {
                defaultFont = fontAtlas->addFontFromRomFs("fonts/JetBrainsMono.ttf", fontSize, true, ImVec2());
                std::string defaultFontName = "JetBrains Mono";
                memcpy(fontAtlas->getAtlas()->ConfigData[fontIndex].Name, defaultFontName.c_str(), defaultFontName.size());
                fontIndex += 1;
            }
            //if (!fontAtlas->build()) {
            //   log::fatal("Failed to load default font!");
            //   return false;
            // }
        }


        // Add all the built-in fonts
        {
            static std::list<ImVector<ImWchar>> glyphRanges;
            glyphRanges.clear();

            for (auto &font : ImHexApi::Fonts::impl::getFonts()) {
                // Construct the glyph range for the font
                ImVector<ImWchar> glyphRange;
                if (!font.glyphRanges.empty()) {
                    for (const auto &range : font.glyphRanges) {
                        glyphRange.push_back(range.begin);
                        glyphRange.push_back(range.end);
                    }
                    glyphRange.push_back(0x00);
                }
                glyphRanges.push_back(glyphRange);

                // Calculate the glyph offset for the font
                const ImVec2 offset = { font.offset.x, font.offset.y - (defaultFont->calculateFontDescend(ft, fontSize) - fontAtlas->calculateFontDescend(ft, font, fontSize)) };

                // Load the font
                float size = fontSize;
                if (font.defaultSize.has_value())
                    size = font.defaultSize.value() * ImHexApi::System::getBackingScaleFactor();
                fontAtlas->addFontFromMemory(font.fontData, size, !font.defaultSize.has_value(), offset, glyphRanges.back());
                auto nameSize = font.name.size();
                memcpy(fontAtlas->getAtlas()->ConfigData[fontIndex].Name, font.name.c_str(), nameSize);
                fontIndex += 1;
            }
        }

        std::vector<int> rect_ids;
        std::map<unsigned, FT_ULong> charCodes;

        FT_Int major, minor, patch;
        FT_Library_Version(ft, &major, &minor, &patch);

        FT_Library_SetLcdFilter(ft, FT_LCD_FILTER_DEFAULT);
        FT_Face face;

        std::string fontName = io.Fonts->ConfigData[0].Name;
        if (toLower(fontName).contains("icon") || toLower(fontName).contains("proggy") || toLower(fontName).contains("unifont")) {
            // Build the font atlas
            if (fontAtlas->build()) {
                return true;
            }

            // If the build wasn't successful and Unicode characters are enabled, try again without them
            // If they were disabled already, something went wrong, and we can't recover from it
            if (!shouldLoadUnicode) {
                return false;
            } else {
                return buildFontAtlas(fontAtlas, fontPath, pixelPerfectFont, fontSize, false, bold, italic, antialias);
            }
        }

        if (FT_New_Memory_Face(ft, reinterpret_cast<const FT_Byte *>(io.Fonts->ConfigData[0].FontData), io.Fonts->ConfigData[0].FontDataSize, 0, &face) != 0) {
            log::fatal("Failed to load face");
            return false;
        }
        auto fontSizePt = fontSize * 72.0f / 96.0f;

        if (FT_Set_Pixel_Sizes(face, fontSizePt, fontSizePt) != 0) {
            log::fatal("Failed to set pixel size");
            return false;
        }

        FT_UInt gIndex;
        FT_ULong charCode = FT_Get_First_Char(face, &gIndex);

        while (gIndex != 0) {
            FT_UInt glyph_index = FT_Get_Char_Index(face, charCode);

            if (FT_Load_Glyph(face, glyph_index, FT_LOAD_TARGET_LCD | FT_LOAD_RENDER | FT_LOAD_COMPUTE_METRICS) != 0) {
                log::fatal("Failed to load glyph");
                return false;
            }

            FT_Bitmap bitmap = face->glyph->bitmap;

            auto width = bitmap.width / 3;
            auto height = bitmap.rows;
            if (width * height == 0) {
                charCode = FT_Get_Next_Char(face, charCode, &gIndex);
                continue;
            }
            FT_GlyphSlot slot = face->glyph;
            FT_Size size = face->size;
            slot->bitmap.pixel_mode = FT_PIXEL_MODE_BGRA;
            int advance = (float) std::floor(slot->advance.x / 64.0f);
            ImVec2 offset = ImVec2(slot->metrics.horiBearingX / 64.0f, (size->metrics.ascender - slot->metrics.horiBearingY) / 64.0f);
            int rect_id = fontAtlas->getAtlas()->AddCustomRectFontGlyph(io.Fonts->Fonts[0], charCode, width, height, advance, offset);
            rect_ids.push_back(rect_id);
            charCodes.insert(std::pair<unsigned, FT_ULong>(rect_id, charCode));
            charCode = FT_Get_Next_Char(face, charCode, &gIndex);
        }

        // Set builder to use RGBA texture
        ImFontConfig &cfg = io.Fonts->ConfigData[0];
        cfg.FontBuilderFlags |= ImGuiFreeTypeBuilderFlags_LoadColor;
        // build rgba atlas
        result = io.Fonts->Build();
        // Retrieve texture in RGBA format
        unsigned char *tex_pixels = nullptr;
        int tex_width, tex_height;
        io.Fonts->GetTexDataAsRGBA32(&tex_pixels, &tex_width, &tex_height);
        u32 *u32_tex_pixels = reinterpret_cast<u32 *>(tex_pixels);

        for (auto rect_id: rect_ids) {
            if (const ImFontAtlasCustomRect *rect = io.Fonts->GetCustomRectByIndex(rect_id)) {
                if (rect->X == 0xFFFF || rect->Y == 0xFFFF)
                    continue;

                charCode = charCodes[rect_id];
                FT_UInt glyph_index = FT_Get_Char_Index(face, charCode);
                if (FT_Load_Glyph(face, glyph_index, FT_LOAD_TARGET_LCD | FT_LOAD_RENDER | FT_LOAD_CROP_BITMAP) != 0) {
                    log::fatal("Failed to load glyph");
                    return false;
                }

                FT_Bitmap bitmap = face->glyph->bitmap;
                u32 bmapWidth = bitmap.width / 3;
                u32 imageWidth = bmapWidth, imageHeight = bitmap.rows;

                u32 *image = reinterpret_cast<u32 *>(malloc(imageHeight * imageWidth * sizeof(u32)));

                for (size_t i = 0; i < bitmap.rows; i++) {
                    for (size_t j = 0; j < bmapWidth; j++) {
                        const unsigned char *bitmapBuffer = &bitmap.buffer[i * bitmap.pitch + 3 * j];
                        image[i * imageWidth + j] = Color::AddAlpha(*bitmapBuffer, *(bitmapBuffer + 1), *(bitmapBuffer + 2));
                    }
                }

                // Fill the custom rectangle with bitmap data
                for (u32 y = 0; y < imageHeight; y++) {
                    ImU32 *p = u32_tex_pixels + (rect->Y + y) * tex_width + (rect->X);
                    for (u32 x = 0; x < imageWidth; x++)
                        *p++ = image[y * imageWidth + x];
                }
                free(image);
            }
            result = true;
        }
        if (ft  != nullptr) {
            FT_Done_FreeType(ft);
            ft = nullptr;
        }
        return result;
    }
}