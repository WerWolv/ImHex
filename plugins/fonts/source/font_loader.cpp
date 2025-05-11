#if defined(_MSC_VER)
#include <windows.h>
#endif

#include <imgui.h>
#include <imgui_internal.h>
#include <list>

#include <hex/api/imhex_api.hpp>
#include <hex/api/content_registry.hpp>

#include <hex/helpers/logger.hpp>
#include <hex/helpers/fs.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/helpers/freetype.hpp>

#include <wolv/utils/string.hpp>
#include <freetype/freetype.h>
#include "imgui_impl_opengl3_loader.h"

#include <font_atlas.hpp>

namespace hex::fonts {

    bool BuildSubPixelAtlas(FontAtlas *fontAtlas, float fontSize) {
        FT_Library ft = nullptr;
        if (FT_Init_FreeType(&ft) != 0) {
            log::fatal("Failed to initialize FreeType");
            return false;
        }

        FT_Face face;
        auto io = ImGui::GetIO();
        io.Fonts = fontAtlas->getAtlas();

        if (io.Fonts->ConfigData.Size <= 0) {
            log::fatal("No font data found");
            return false;
        } else {
            ImVector<ImS32> rect_ids;
            std::map<ImS32, ft::Bitmap> bitmapLCD;
            ImU32 fontCount = io.Fonts->ConfigData.Size;
            for (ImU32 i = 0; i < fontCount; i++) {
                std::string fontName = io.Fonts->ConfigData[i].Name;

                std::ranges::transform(fontName.begin(), fontName.end(), fontName.begin(), [](unsigned char c) { return std::tolower(c); });
                if (fontName == "nonscalable") {
                    continue;
                }

                if (FT_New_Memory_Face(ft, reinterpret_cast<const FT_Byte *>(io.Fonts->ConfigData[i].FontData), io.Fonts->ConfigData[i].FontDataSize, 0, &face) != 0) {
                    log::fatal("Failed to load face");
                    return false;
                }

                float actualFontSize;
                if (fontName.find("icon") != std::string::npos)
                    actualFontSize = ImHexApi::Fonts::pointsToPixels(fontSize);
                else
                    actualFontSize = fontSize;

                if (FT_Set_Pixel_Sizes(face, actualFontSize, actualFontSize) != 0) {
                    log::fatal("Failed to set pixel size");
                    return false;
                }

                FT_UInt gIndex;
                FT_ULong charCode = FT_Get_First_Char(face, &gIndex);

                while (gIndex != 0) {


                    FT_UInt glyph_index = FT_Get_Char_Index(face, charCode);
                    if (FT_Load_Glyph(face, glyph_index, FT_LOAD_TARGET_LCD | FT_LOAD_TARGET_LIGHT  | FT_LOAD_RENDER) != 0) {
                        IM_ASSERT(true && "Failed to load glyph");
                        return false;
                    }

                    ft::Bitmap bitmap_lcd = ft::Bitmap(face->glyph->bitmap.width, face->glyph->bitmap.rows, face->glyph->bitmap.pitch, face->glyph->bitmap.buffer);
                    if (face->glyph->bitmap.width * face->glyph->bitmap.rows == 0) {
                        charCode = FT_Get_Next_Char(face, charCode, &gIndex);
                        continue;
                    }

                    auto width = bitmap_lcd.getWidth() / 3;
                    auto height = bitmap_lcd.getHeight();
                    FT_GlyphSlot slot = face->glyph;
                    FT_Size size = face->size;

                    ImVec2 offset = ImVec2((slot->metrics.horiBearingX / 64.0f), (size->metrics.ascender - slot->metrics.horiBearingY) / 64.0f);
                    if (fontName.find("codicon") != std::string::npos)
                        offset.x -= 1.0f;
                    ImS32 advance = (float) slot->advance.x / 64.0f;
                    if (offset.x+width > advance && advance >= (int) width)
                        offset.x =  advance - width;

                    ImS32 rect_id = io.Fonts->AddCustomRectFontGlyph(io.Fonts->Fonts[0], charCode, width, height, advance, offset);
                    rect_ids.push_back(rect_id);
                    bitmapLCD.insert(std::make_pair(rect_id, bitmap_lcd));
                    charCode = FT_Get_Next_Char(face, charCode, &gIndex);
                }
                FT_Done_Face(face);
            }
            fontAtlas->getAtlas()->FontBuilderFlags |= ImGuiFreeTypeBuilderFlags_SubPixel;
            fontAtlas->build();
            ImU8 *tex_pixels_ch = nullptr;
            ImS32 tex_width;

            ImS32 tex_height;
            fontAtlas->getAtlas()->GetTexDataAsRGBA32(&tex_pixels_ch, &tex_width, &tex_height);
            ImU32 *tex_pixels = reinterpret_cast<ImU32 *>(tex_pixels_ch);
            for (auto rect_id: rect_ids) {
                if (const ImFontAtlasCustomRect *rect = io.Fonts->GetCustomRectByIndex(rect_id)) {
                    if (rect->X == 0xFFFF || rect->Y == 0xFFFF || !bitmapLCD.contains(rect_id))
                        continue;

                    ft::Bitmap bitmapLCDSaved = bitmapLCD.at(rect_id);
                    ImU32 imageWidth = bitmapLCDSaved.getWidth() / 3;
                    ImU32 imageHeight = bitmapLCDSaved.getHeight();
                    const ImU8 *bitmapBuffer = bitmapLCDSaved.getData();

                    for (ImU32 y = 0; y < imageHeight; y++) {
                        ImU32 *p = tex_pixels + (rect->Y + y) * tex_width + (rect->X);
                        for (ImU32 x = 0; x < imageWidth; x++) {
                            const ImU8 *bitmapPtrLCD = &bitmapBuffer[y * bitmapLCDSaved.getPitch() + 3 * x];
                             *p++ = ft::RGBA::addAlpha(*bitmapPtrLCD, *(bitmapPtrLCD + 1), *(bitmapPtrLCD + 2));
                        }
                    }
                }
            }
            if (ft != nullptr) {
                FT_Done_FreeType(ft);
                ft = nullptr;
            }
        }
        return true;
    }

    bool buildFontAtlas(FontAtlas *fontAtlas, std::fs::path fontPath, bool pixelPerfectFont, float fontSize, bool loadUnicodeCharacters, bool bold, bool italic,const std::string &antiAliasType) {
        if (fontAtlas == nullptr) {
            return false;
        }
        auto realFontSize = ImHexApi::Fonts::pointsToPixels(fontSize);
        bool antialias = antiAliasType == "grayscale";
        bool monochrome = antiAliasType == "none";
        FT_Library ft = nullptr;
        if (FT_Init_FreeType(&ft) != 0) {
            log::fatal("Failed to initialize FreeType");
            return false;
        }
        fontAtlas->reset();
        u32 fontIndex = 0;
        auto io = ImGui::GetIO();
        io.Fonts = fontAtlas->getAtlas();
        // Check if Unicode support is enabled in the settings and that the user doesn't use the No GPU version on Windows
        // The Mesa3D software renderer on Windows identifies itself as "VMware, Inc."
        bool shouldLoadUnicode = ContentRegistry::Settings::read<bool>("hex.fonts.setting.font", "hex.fonts.setting.font.load_all_unicode_chars", false) && ImHexApi::System::getGPUVendor() != "VMware, Inc.";

        if (!loadUnicodeCharacters)
            shouldLoadUnicode = false;

        fontAtlas->enableUnicodeCharacters(shouldLoadUnicode);

        // If a custom font is set in the settings, load the rest of the settings as well
        if (!pixelPerfectFont) {
            fontAtlas->setBold(bold);
            fontAtlas->setItalic(italic);
            if (antialias || monochrome)
               fontAtlas->setAntiAliasing(antialias);
        } else {
            fontPath.clear();
        }

        // Try to load the custom font if one was set
        std::optional<Font> defaultFont;
        if (!fontPath.empty()) {
            defaultFont = fontAtlas->addFontFromFile(fontPath, realFontSize, true, ImVec2());
            std::string defaultFontName = defaultFont.has_value() ? fontPath.filename().string() : "Custom Font";
            memcpy(fontAtlas->getAtlas()->ConfigData[fontIndex].Name, defaultFontName.c_str(), defaultFontName.size());
            fontIndex += 1;
            if ((antialias || monochrome) && !fontAtlas->build()) {
                log::error("Failed to load custom font '{}'! Falling back to default font", wolv::util::toUTF8String(fontPath));
                defaultFont.reset();
            }
        }

        // If there's no custom font set, or it failed to load, fall back to the default font
        if (!defaultFont.has_value()) {
            if (pixelPerfectFont) {
                realFontSize = fontSize = std::max(1.0F, std::floor(ImHexApi::System::getBackingScaleFactor() * 13.0F));
                defaultFont = fontAtlas->addDefaultFont();
                std::string defaultFontName = "Proggy Clean";
                memcpy(fontAtlas->getAtlas()->ConfigData[fontIndex].Name, defaultFontName.c_str(), defaultFontName.size());
                fontIndex += 1;
            } else {
                defaultFont = fontAtlas->addFontFromRomFs("fonts/JetBrainsMono.ttf", realFontSize, true, ImVec2());
                std::string defaultFontName = "JetBrains Mono";
                memcpy(fontAtlas->getAtlas()->ConfigData[fontIndex].Name, defaultFontName.c_str(), defaultFontName.size());
                fontIndex += 1;
            }
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
                const ImVec2 offset = { font.offset.x, font.offset.y - (defaultFont->calculateFontDescend(ft, realFontSize) - fontAtlas->calculateFontDescend(ft, font, realFontSize)) };

                // Load the font
                float size = realFontSize;
                if (font.defaultSize.has_value())
                    size = font.defaultSize.value() * ImHexApi::System::getBackingScaleFactor();
                fontAtlas->addFontFromMemory(font.fontData, size, !font.defaultSize.has_value(), offset, glyphRanges.back());
                if (!font.scalable.value_or(true)) {
                    std::string fontName = "NonScalable";
                    auto nameSize = fontName.size();
                    memcpy(fontAtlas->getAtlas()->ConfigData[fontIndex].Name, fontName.c_str(), nameSize);
                } else {
                    auto nameSize = font.name.size();
                    memcpy(fontAtlas->getAtlas()->ConfigData[fontIndex].Name, font.name.c_str(), nameSize);
                }
                fontIndex += 1;
            }
        }

        if (ft != nullptr) {
            FT_Done_FreeType(ft);
            ft = nullptr;
        }

        if (antialias || monochrome) {
            if (!fontAtlas->build()) {
                log::fatal("Failed to load font!");
                return false;
            }
            return true;
        } else
           return BuildSubPixelAtlas(fontAtlas,fontSize);
    }
}