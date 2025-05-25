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

    bool BuildSubPixelAtlas(FontAtlas *fontAtlas) {
        FT_Library ft = nullptr;
        if (FT_Init_FreeType(&ft) != 0) {
            log::fatal("Failed to initialize FreeType");
            return false;
        }

        FT_Face face;
        auto io = ImGui::GetIO();
        io.Fonts = fontAtlas->getAtlas();

        if (io.Fonts->Sources.Size <= 0) {
            log::fatal("No font data found");
            return false;
        } else {
            ImVector<ImS32> customRectIds;
            std::map<ImS32, ft::Bitmap> customRectBitmaps;
            for (const auto &config : io.Fonts->Sources) {
                const auto &fontName = config.Name;

                if (hex::equalsIgnoreCase(fontName, "nonscalable")) {
                    continue;
                }

                if (FT_New_Memory_Face(ft, static_cast<const FT_Byte *>(config.FontData), config.FontDataSize, 0, &face) != 0) {
                    log::fatal("Failed to load face");
                    return false;
                }

                FT_Size_RequestRec request;
                request.type = FT_SIZE_REQUEST_TYPE_REAL_DIM;
                request.width = 0;
                request.height = (uint32_t)(IM_ROUND(config.SizePixels) * 64.0F);
                request.horiResolution = 0;
                request.vertResolution = 0;
                FT_Request_Size(face, &request);

                FT_UInt glyphIndex;
                FT_ULong charCode = FT_Get_First_Char(face, &glyphIndex);

                while (glyphIndex != 0) {
                    if (FT_Load_Glyph(face, glyphIndex, FT_LOAD_TARGET_LCD | FT_LOAD_TARGET_LIGHT | FT_LOAD_RENDER) != 0) {
                        IM_ASSERT(true && "Failed to load glyph");
                        return false;
                    }

                    const ft::Bitmap bitmap = ft::Bitmap(face->glyph->bitmap.width, face->glyph->bitmap.rows, face->glyph->bitmap.pitch, face->glyph->bitmap.buffer);
                    if (face->glyph->bitmap.width * face->glyph->bitmap.rows == 0) {
                        charCode = FT_Get_Next_Char(face, charCode, &glyphIndex);
                        continue;
                    }

                    const auto width = bitmap.getWidth() / 3.0F;
                    const auto height = bitmap.getHeight();
                    const auto slot = face->glyph;
                    const auto size = face->size;

                    auto offset = ImVec2(face->glyph->bitmap_left, -face->glyph->bitmap_top);
                    offset.x += config.GlyphOffset.x;
                    offset.y += size->metrics.ascender / 64.0F;

                    const ImS32 advance = slot->advance.x / 64.0F;

                    ImS32 rectId = io.Fonts->AddCustomRectFontGlyph(io.Fonts->Fonts[0], charCode, width, height, advance, offset);
                    customRectIds.push_back(rectId);
                    customRectBitmaps.insert(std::make_pair(rectId, bitmap));

                    charCode = FT_Get_Next_Char(face, charCode, &glyphIndex);
                }
                FT_Done_Face(face);
            }

            fontAtlas->getAtlas()->FontBuilderFlags |= ImGuiFreeTypeBuilderFlags_SubPixel;
            if (!fontAtlas->build())
                return false;

            ImU8 *textureColors = nullptr;
            ImS32 textureWidth, textureHeight;

            fontAtlas->getAtlas()->GetTexDataAsRGBA32(&textureColors, &textureWidth, &textureHeight);
            auto *texturePixels = reinterpret_cast<ImU32 *>(textureColors);
            for (auto rect_id: customRectIds) {
                if (const ImFontAtlasCustomRect *rect = io.Fonts->GetCustomRectByIndex(rect_id)) {
                    if (rect->X == 0xFFFF || rect->Y == 0xFFFF || !customRectBitmaps.contains(rect_id))
                        continue;

                    const auto &bitmap = customRectBitmaps.at(rect_id);
                    ImU32 imageWidth = bitmap.getWidth() / 3;
                    ImU32 imageHeight = bitmap.getHeight();
                    const auto &bitmapBuffer = bitmap.getData();

                    for (ImU32 y = 0; y < imageHeight; y++) {
                        ImU32 *pixel = texturePixels + (rect->Y + y) * textureWidth + (rect->X);
                        for (ImU32 x = 0; x < imageWidth; x++) {
                            const ImU8 *bitmapPixel = &bitmapBuffer[y * bitmap.getPitch() + 3 * x];
                             *pixel++ = ft::RGBA::addAlpha(*bitmapPixel, *(bitmapPixel + 1), *(bitmapPixel + 2));
                        }
                    }
                }
            }

            for (i64 i = 0; i < textureWidth * textureHeight; i++) {
                if (texturePixels[i] == 0x00FFFFFF) {
                    texturePixels[i] = 0x00000000;
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
            defaultFont = fontAtlas->addFontFromFile(fontPath, fontSize, true, ImVec2());
            std::string defaultFontName = defaultFont.has_value() ? fontPath.filename().string() : "Custom Font";
            memcpy(fontAtlas->getAtlas()->Sources[fontIndex].Name, defaultFontName.c_str(), defaultFontName.size());
            fontIndex += 1;
            if ((antialias || monochrome) && !fontAtlas->build()) {
                log::error("Failed to load custom font '{}'! Falling back to default font", wolv::util::toUTF8String(fontPath));
                defaultFont.reset();
            }
        }

        // If there's no custom font set, or it failed to load, fall back to the default font
        if (!defaultFont.has_value()) {
            if (pixelPerfectFont) {
                defaultFont = fontAtlas->addDefaultFont();
                std::string defaultFontName = "Proggy Clean";
                memcpy(fontAtlas->getAtlas()->Sources[fontIndex].Name, defaultFontName.c_str(), defaultFontName.size());
                fontIndex += 1;
            } else {
                defaultFont = fontAtlas->addFontFromRomFs("fonts/JetBrainsMono.ttf", fontSize, true, ImVec2());
                std::string defaultFontName = "JetBrains Mono";
                memcpy(fontAtlas->getAtlas()->Sources[fontIndex].Name, defaultFontName.c_str(), defaultFontName.size());
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

                // Load the font
                if (font.defaultSize.has_value())
                    fontSize = font.defaultSize.value() * ImHexApi::System::getBackingScaleFactor();

                ImVec2 offset = { font.offset.x, font.offset.y };

                bool scalable = font.scalable.value_or(true);
                if (scalable) {
                    offset.y += ImCeil(3_scaled);
                }

                fontAtlas->addFontFromMemory(font.fontData, fontSize, !font.defaultSize.has_value(), offset, glyphRanges.back());

                if (!scalable) {
                    std::string fontName = "NonScalable";
                    auto nameSize = fontName.size();
                    memcpy(fontAtlas->getAtlas()->Sources[fontIndex].Name, fontName.c_str(), nameSize);
                } else {
                    auto nameSize = font.name.size();
                    memcpy(fontAtlas->getAtlas()->Sources[fontIndex].Name, font.name.c_str(), nameSize);
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
           return BuildSubPixelAtlas(fontAtlas);
    }
}