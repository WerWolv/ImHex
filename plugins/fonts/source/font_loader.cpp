#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_freetype.h>
#include <imgui_impl_opengl3.h>

#include <memory>
#include <list>

#include <hex/api/imhex_api.hpp>
#include <hex/api/content_registry.hpp>
#include <hex/api/event_manager.hpp>
#include <hex/api/task_manager.hpp>
#include <hex/helpers/auto_reset.hpp>

#include <hex/helpers/logger.hpp>
#include <hex/helpers/fs.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/helpers/default_paths.hpp>

#include <romfs/romfs.hpp>

#include <wolv/io/file.hpp>
#include <wolv/utils/string.hpp>

namespace hex::fonts {

    namespace {

        class Font {
        public:
            Font() = default;

            float getDescent() const {
                return m_font->Descent;
            }

        private:
            explicit Font(ImFont *font) : m_font(font) { }

        private:
            friend class FontAtlas;

            ImFont *m_font;
        };

        class FontAtlas {
        public:
            FontAtlas() : m_fontAtlas(IM_NEW(ImFontAtlas)) {
                enableUnicodeCharacters(false);

                // Set the default configuration for the font atlas
                m_config.OversampleH = m_config.OversampleV = 1;
                m_config.PixelSnapH = true;
                m_config.MergeMode = false;

                // Make sure the font atlas doesn't get too large, otherwise weaker GPUs might reject it
                m_fontAtlas->Flags |= ImFontAtlasFlags_NoPowerOfTwoHeight;
                m_fontAtlas->TexDesiredWidth = 4096;
            }

            ~FontAtlas() {
                IM_DELETE(m_fontAtlas);
            }

            Font addDefaultFont() {
                ImFontConfig config = m_config;
                config.FontBuilderFlags |= ImGuiFreeTypeBuilderFlags_Monochrome | ImGuiFreeTypeBuilderFlags_MonoHinting;
                config.SizePixels = std::floor(getAdjustedFontSize(ImHexApi::System::getGlobalScale() * 13.0F));

                auto font = m_fontAtlas->AddFontDefault(&config);
                m_fontSizes.emplace_back(false, config.SizePixels);

                m_config.MergeMode = true;

                return Font(font);
            }

            Font addFontFromMemory(const std::vector<u8> &fontData, float fontSize, bool scalable, ImVec2 offset, const ImVector<ImWchar> &glyphRange = {}) {
                auto &storedFontData = m_fontData.emplace_back(fontData);

                ImFontConfig config = m_config;
                config.FontDataOwnedByAtlas = false;

                config.GlyphOffset = { offset.x, offset.y };
                auto font = m_fontAtlas->AddFontFromMemoryTTF(storedFontData.data(), int(storedFontData.size()), getAdjustedFontSize(fontSize), &config, !glyphRange.empty() ? glyphRange.Data : m_glyphRange.Data);
                m_fontSizes.emplace_back(scalable, fontSize);

                m_config.MergeMode = true;

                return Font(font);
            }

            Font addFontFromRomFs(const std::fs::path &path, float fontSize, bool scalable, ImVec2 offset, const ImVector<ImWchar> &glyphRange = {}) {
                auto data = romfs::get(path).span<u8>();
                return addFontFromMemory({ data.begin(), data.end() }, fontSize, scalable, offset, glyphRange);
            }

            Font addFontFromFile(const std::fs::path &path, float fontSize, bool scalable, ImVec2 offset, const ImVector<ImWchar> &glyphRange = {}) {
                wolv::io::File file(path, wolv::io::File::Mode::Read);

                auto data = file.readVector();
                return addFontFromMemory(data, fontSize, scalable, offset, glyphRange);
            }

            void setBold(bool enabled) {
                if (enabled)
                    m_config.FontBuilderFlags |= ImGuiFreeTypeBuilderFlags_Bold;
                else
                    m_config.FontBuilderFlags &= ~ImGuiFreeTypeBuilderFlags_Bold;
            }

            void setItalic(bool enabled) {
                if (enabled)
                    m_config.FontBuilderFlags |= ImGuiFreeTypeBuilderFlags_Oblique;
                else
                    m_config.FontBuilderFlags &= ~ImGuiFreeTypeBuilderFlags_Oblique;
            }

            void setAntiAliasing(bool enabled) {
                if (enabled)
                    m_config.FontBuilderFlags &= ~ImGuiFreeTypeBuilderFlags_Monochrome | ImGuiFreeTypeBuilderFlags_MonoHinting;
                else
                    m_config.FontBuilderFlags |= ImGuiFreeTypeBuilderFlags_Monochrome | ImGuiFreeTypeBuilderFlags_MonoHinting;
            }

            void enableUnicodeCharacters(bool enabled) {
                ImFontGlyphRangesBuilder glyphRangesBuilder;

                {
                    constexpr static std::array<ImWchar, 3> controlCodeRange    = { 0x0001, 0x001F, 0 };
                    constexpr static std::array<ImWchar, 3> extendedAsciiRange  = { 0x007F, 0x00FF, 0 };
                    constexpr static std::array<ImWchar, 3> latinExtendedARange = { 0x0100, 0x017F, 0 };

                    glyphRangesBuilder.AddRanges(controlCodeRange.data());
                    glyphRangesBuilder.AddRanges(m_fontAtlas->GetGlyphRangesDefault());
                    glyphRangesBuilder.AddRanges(extendedAsciiRange.data());
                    glyphRangesBuilder.AddRanges(latinExtendedARange.data());
                }

                if (enabled) {
                    constexpr static std::array<ImWchar, 3> fullRange = { 0x0180, 0xFFEF, 0 };

                    glyphRangesBuilder.AddRanges(fullRange.data());
                } else {
                    glyphRangesBuilder.AddRanges(m_fontAtlas->GetGlyphRangesJapanese());
                    glyphRangesBuilder.AddRanges(m_fontAtlas->GetGlyphRangesChineseFull());
                    glyphRangesBuilder.AddRanges(m_fontAtlas->GetGlyphRangesCyrillic());
                    glyphRangesBuilder.AddRanges(m_fontAtlas->GetGlyphRangesKorean());
                    glyphRangesBuilder.AddRanges(m_fontAtlas->GetGlyphRangesThai());
                    glyphRangesBuilder.AddRanges(m_fontAtlas->GetGlyphRangesVietnamese());
                    glyphRangesBuilder.AddText("⌘⌥⌃⇧⏎⇥⌫⇪");
                }

                m_glyphRange.clear();
                glyphRangesBuilder.BuildRanges(&m_glyphRange);
            }

            bool build() const {
                return m_fontAtlas->Build();
            }

            [[nodiscard]] ImFontAtlas* getAtlas() {
                auto result = m_fontAtlas;

                return result;
            }

            float calculateFontDescend(const ImHexApi::Fonts::Font &font, float fontSize) const {
                auto atlas = std::make_unique<ImFontAtlas>();
                auto cfg = m_config;

                // Calculate the expected font size
                auto size = fontSize;
                if (font.defaultSize.has_value())
                    size = font.defaultSize.value() * std::max(1.0F, std::floor(ImHexApi::Fonts::getFontSize() / ImHexApi::Fonts::DefaultFontSize));
                else
                    size = std::max(1.0F, std::floor(size / ImHexApi::Fonts::DefaultFontSize)) * ImHexApi::Fonts::DefaultFontSize;

                cfg.MergeMode = false;
                cfg.SizePixels = size;
                cfg.FontDataOwnedByAtlas = false;

                // Construct a range that only contains the first glyph of the font
                ImVector<ImWchar> queryRange;
                {
                    auto firstGlyph = font.glyphRanges.empty() ? m_glyphRange.front() : font.glyphRanges.front().begin;
                    queryRange.push_back(firstGlyph);
                    queryRange.push_back(firstGlyph);
                }
                queryRange.push_back(0x00);

                // Build the font atlas with the query range
                auto newFont = atlas->AddFontFromMemoryTTF(const_cast<u8 *>(font.fontData.data()), int(font.fontData.size()), 0, &cfg, queryRange.Data);
                atlas->Build();

                return newFont->Descent;
            }

            void reset() {
                IM_DELETE(m_fontAtlas);
                m_fontAtlas = IM_NEW(ImFontAtlas);

                m_fontData.clear();
                m_config.MergeMode = false;
            }

            void updateFontScaling(float newScaling) {
                for (int i = 0; i < m_fontAtlas->ConfigData.size(); i += 1) {
                    const auto &[scalable, fontSize] = m_fontSizes[i];
                    auto &configData = m_fontAtlas->ConfigData[i];

                    if (!scalable) {
                        configData.SizePixels = fontSize * std::floor(newScaling);
                    } else {
                        configData.SizePixels = fontSize * newScaling;
                    }
                }
            }

        private:
            float getAdjustedFontSize(float fontSize) const {
                // Since macOS reports half the framebuffer size that's actually available,
                // we'll multiply all font sizes by that and then divide the global font scale
                // by the same amount to get super crisp font rendering.
                return fontSize * hex::ImHexApi::System::getBackingScaleFactor();
            }

        private:
            ImFontAtlas* m_fontAtlas;
            std::vector<std::pair<bool, float>> m_fontSizes;
            ImFontConfig m_config;
            ImVector<ImWchar> m_glyphRange;

            std::list<std::vector<u8>> m_fontData;
        };

        std::fs::path findCustomFontPath() {
            // Find the custom font file specified in the settings
            auto fontFile = ContentRegistry::Settings::read<std::fs::path>("hex.builtin.setting.font", "hex.builtin.setting.font.font_path", "");
            if (!fontFile.empty()) {
                if (!wolv::io::fs::exists(fontFile) || !wolv::io::fs::isRegularFile(fontFile)) {
                    log::warn("Custom font file {} not found! Falling back to default font.", wolv::util::toUTF8String(fontFile));
                    fontFile.clear();
                }

                log::info("Loading custom font from {}", wolv::util::toUTF8String(fontFile));
            }

            // If no custom font has been specified, search for a file called "font.ttf" in one of the resource folders
            if (fontFile.empty()) {
                for (const auto &dir : paths::Resources.read()) {
                    auto path = dir / "font.ttf";
                    if (wolv::io::fs::exists(path)) {
                        log::info("Loading custom font from {}", wolv::util::toUTF8String(path));

                        fontFile = path;
                        break;
                    }
                }
            }

            return fontFile;
        }

        float getFontSize() {
            const auto pixelPerfectFont = ContentRegistry::Settings::read<bool>("hex.builtin.setting.font", "hex.builtin.setting.font.pixel_perfect_default_font", true);

            if (pixelPerfectFont)
                return 13.0F * ImHexApi::System::getGlobalScale();
            else
                return float(ContentRegistry::Settings::read<int>("hex.builtin.setting.font", "hex.builtin.setting.font.font_size", 13)) * ImHexApi::System::getGlobalScale();
        }

    }

    bool buildFontAtlasImpl(bool loadUnicodeCharacters) {
        static FontAtlas fontAtlas;
        fontAtlas.reset();

        // Check if Unicode support is enabled in the settings and that the user doesn't use the No GPU version on Windows
        // The Mesa3D software renderer on Windows identifies itself as "VMware, Inc."
        bool shouldLoadUnicode =
                ContentRegistry::Settings::read<bool>("hex.builtin.setting.font", "hex.builtin.setting.font.load_all_unicode_chars", false) &&
                ImHexApi::System::getGPUVendor() != "VMware, Inc.";

        if (!loadUnicodeCharacters)
            shouldLoadUnicode = false;

        fontAtlas.enableUnicodeCharacters(shouldLoadUnicode);

        // If a custom font is set in the settings, load the rest of the settings as well
        if (ContentRegistry::Settings::read<bool>("hex.builtin.setting.font", "hex.builtin.setting.font.custom_font_enable", false)) {
            fontAtlas.setBold(ContentRegistry::Settings::read<bool>("hex.builtin.setting.font", "hex.builtin.setting.font.font_bold", false));
            fontAtlas.setItalic(ContentRegistry::Settings::read<bool>("hex.builtin.setting.font", "hex.builtin.setting.font.font_italic", false));
            fontAtlas.setAntiAliasing(ContentRegistry::Settings::read<bool>("hex.builtin.setting.font", "hex.builtin.setting.font.font_antialias", true));

            ImHexApi::Fonts::impl::setCustomFontPath(findCustomFontPath());
        }
        ImHexApi::Fonts::impl::setFontSize(getFontSize());


        const auto fontSize = ImHexApi::Fonts::getFontSize();
        const auto &customFontPath = ImHexApi::Fonts::getCustomFontPath();

        // Try to load the custom font if one was set
        std::optional<Font> defaultFont;
        if (!customFontPath.empty()) {
            defaultFont = fontAtlas.addFontFromFile(customFontPath, fontSize, true, ImVec2());
            if (!fontAtlas.build()) {
                log::error("Failed to load custom font '{}'! Falling back to default font", wolv::util::toUTF8String(customFontPath));
                defaultFont.reset();
            }
        }

        // If there's no custom font set, or it failed to load, fall back to the default font
        if (!defaultFont.has_value()) {
            auto pixelPerfectFont = ContentRegistry::Settings::read<bool>("hex.builtin.setting.font", "hex.builtin.setting.font.pixel_perfect_default_font", true);

            if (pixelPerfectFont)
                defaultFont = fontAtlas.addDefaultFont();
            else
                defaultFont = fontAtlas.addFontFromRomFs("fonts/JetBrainsMono.ttf", fontSize, true, ImVec2());

            if (!fontAtlas.build()) {
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
                ImVec2 offset = { font.offset.x, font.offset.y - (defaultFont->getDescent() - fontAtlas.calculateFontDescend(font, fontSize)) };

                // Load the font
                fontAtlas.addFontFromMemory(font.fontData, font.defaultSize.value_or(fontSize), !font.defaultSize.has_value(), offset, glyphRanges.back());
            }
        }

        EventDPIChanged::subscribe([](float, float newScaling) {
            fontAtlas.updateFontScaling(newScaling);

            if (fontAtlas.build()) {
                ImGui_ImplOpenGL3_DestroyFontsTexture();
                ImGui_ImplOpenGL3_CreateFontsTexture();
                ImHexApi::Fonts::impl::setFontAtlas(fontAtlas.getAtlas());
            }
        });

        // Build the font atlas
        const bool result = fontAtlas.build();
        if (result) {
            // Set the font atlas if the build was successful
            ImHexApi::Fonts::impl::setFontAtlas(fontAtlas.getAtlas());
            return true;
        }

        // If the build wasn't successful and Unicode characters are enabled, try again without them
        // If they were disabled already, something went wrong, and we can't recover from it
        if (!shouldLoadUnicode) {
            // Reset Unicode loading and scaling factor settings back to default to make sure the user can still use the application
            ContentRegistry::Settings::write<bool>("hex.builtin.setting.font", "hex.builtin.setting.font.load_all_unicode_chars", false);

            ContentRegistry::Settings::write<float>("hex.builtin.setting.interface", "hex.builtin.setting.interface.scaling_factor", 1.0F);
            ImHexApi::System::impl::setGlobalScale(1.0F);

            return false;
        } else {
            return buildFontAtlasImpl(false);
        }
    }

    bool buildFontAtlas() {
        return buildFontAtlasImpl(true);
    }

}