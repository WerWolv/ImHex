#include <imgui.h>
#include <imgui_internal.h>
#include <list>

#include <hex/api/imhex_api.hpp>
#include <hex/api/content_registry.hpp>

#include <hex/helpers/logger.hpp>
#include <hex/helpers/fs.hpp>
#include <hex/helpers/utils.hpp>

#include <wolv/utils/string.hpp>

#include <font_atlas.hpp>

namespace hex::fonts {

    bool buildFontAtlas(FontAtlas *fontAtlas, std::fs::path fontPath, bool pixelPerfectFont, float fontSize, bool loadUnicodeCharacters, bool bold, bool italic, bool antialias) {
        if (fontAtlas == nullptr) {
            return false;
        }

        fontAtlas->reset();

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
            if (!fontAtlas->build()) {
                log::error("Failed to load custom font '{}'! Falling back to default font", wolv::util::toUTF8String(fontPath));
                defaultFont.reset();
            }
        }

        // If there's no custom font set, or it failed to load, fall back to the default font
        if (!defaultFont.has_value()) {
            if (pixelPerfectFont) {
                fontSize = std::max(1.0F, std::floor(ImHexApi::System::getGlobalScale() * ImHexApi::System::getBackingScaleFactor() * 13.0F));
                defaultFont = fontAtlas->addDefaultFont();
            } else
                defaultFont = fontAtlas->addFontFromRomFs("fonts/JetBrainsMono.ttf", fontSize, true, ImVec2());

            if (!fontAtlas->build()) {
                log::fatal("Failed to load default font!");
                return false;
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
                const ImVec2 offset = { font.offset.x, font.offset.y - (defaultFont->getDescent() - fontAtlas->calculateFontDescend(font, fontSize)) };

                // Load the font
                float size = fontSize;
                if (font.defaultSize.has_value())
                    size = font.defaultSize.value() * ImHexApi::System::getBackingScaleFactor();
                fontAtlas->addFontFromMemory(font.fontData, size, !font.defaultSize.has_value(), offset, glyphRanges.back());
            }
        }

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

}