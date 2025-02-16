#pragma once

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_freetype.h>

#include <memory>
#include <list>

#include <hex/api/task_manager.hpp>

#include <romfs/romfs.hpp>

#include <wolv/io/file.hpp>


namespace hex::fonts {

    class Font {
    public:
        Font() = default;

        float getDescent() const {
            return m_font->Descent;
        }

        ImFont* getFont() { return m_font; }

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
            m_defaultConfig.OversampleH = m_defaultConfig.OversampleV = 1;
            m_defaultConfig.PixelSnapH = true;
            m_defaultConfig.MergeMode = false;

            // Make sure the font atlas doesn't get too large, otherwise weaker GPUs might reject it
            m_fontAtlas->Flags |= ImFontAtlasFlags_NoPowerOfTwoHeight;
            m_fontAtlas->TexDesiredWidth = 4096;
        }

        FontAtlas(const FontAtlas &) = delete;
        FontAtlas &operator=(const FontAtlas &) = delete;

        FontAtlas(FontAtlas &&other) noexcept {
            m_fontAtlas = other.m_fontAtlas;
            other.m_fontAtlas = nullptr;

            m_defaultConfig = other.m_defaultConfig;
            m_fontSizes = std::move(other.m_fontSizes);
            m_fontConfigs = std::move(other.m_fontConfigs);
            m_glyphRange = std::move(other.m_glyphRange);
            m_fontData = std::move(other.m_fontData); 
        }

        FontAtlas& operator=(FontAtlas &&other) noexcept {
            if (this != &other) {
                if (m_fontAtlas != nullptr) {
                    IM_DELETE(m_fontAtlas);
                }

                m_fontAtlas = other.m_fontAtlas;
                other.m_fontAtlas = nullptr;

                m_defaultConfig = other.m_defaultConfig;
                m_fontSizes = std::move(other.m_fontSizes);
                m_fontConfigs = std::move(other.m_fontConfigs);
                m_glyphRange = std::move(other.m_glyphRange);
                m_fontData = std::move(other.m_fontData);
            }

            return *this;
        }

        ~FontAtlas() {
            if (m_fontAtlas != nullptr) {
                m_fontAtlas->Locked = false;
                IM_DELETE(m_fontAtlas);
                m_fontAtlas = nullptr;
            }
        }

        Font addDefaultFont() {
            auto &config = m_fontConfigs.emplace_back(m_defaultConfig);
            config.FontBuilderFlags |= ImGuiFreeTypeBuilderFlags_Monochrome | ImGuiFreeTypeBuilderFlags_MonoHinting;
            config.SizePixels = std::max(1.0F, std::floor(ImHexApi::System::getGlobalScale() * ImHexApi::System::getBackingScaleFactor() * 13.0F));

            auto font = m_fontAtlas->AddFontDefault(&config);

            font->Scale = 1.0 / std::floor(ImHexApi::System::getBackingScaleFactor());

            m_fontSizes.emplace_back(false, config.SizePixels);

            m_defaultConfig.MergeMode = true;

            return Font(font);
        }

        Font addFontFromMemory(const std::vector<u8> &fontData, float fontSize, bool scalable, ImVec2 offset, const ImVector<ImWchar> &glyphRange = {}) {
            auto &storedFontData = m_fontData.emplace_back(fontData);

            auto &config = m_fontConfigs.emplace_back(m_defaultConfig);
            config.FontDataOwnedByAtlas = false;

            config.GlyphOffset = { offset.x, offset.y };
            auto font = m_fontAtlas->AddFontFromMemoryTTF(storedFontData.data(), int(storedFontData.size()), fontSize, &config, !glyphRange.empty() ? glyphRange.Data : m_glyphRange.Data);
            font->Scale = 1.0 / ImHexApi::System::getBackingScaleFactor();

            m_fontSizes.emplace_back(scalable, fontSize);

            m_defaultConfig.MergeMode = true;

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
                m_defaultConfig.FontBuilderFlags |= ImGuiFreeTypeBuilderFlags_Bold;
            else
                m_defaultConfig.FontBuilderFlags &= ~ImGuiFreeTypeBuilderFlags_Bold;
        }

        void setItalic(bool enabled) {
            if (enabled)
                m_defaultConfig.FontBuilderFlags |= ImGuiFreeTypeBuilderFlags_Oblique;
            else
                m_defaultConfig.FontBuilderFlags &= ~ImGuiFreeTypeBuilderFlags_Oblique;
        }

        void setAntiAliasing(bool enabled) {
            if (enabled)
                m_defaultConfig.FontBuilderFlags &= ~ImGuiFreeTypeBuilderFlags_Monochrome | ImGuiFreeTypeBuilderFlags_MonoHinting;
            else
                m_defaultConfig.FontBuilderFlags |= ImGuiFreeTypeBuilderFlags_Monochrome | ImGuiFreeTypeBuilderFlags_MonoHinting;
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
            return m_fontAtlas;
        }

        float calculateFontDescend(const ImHexApi::Fonts::Font &font, float fontSize) const {
            auto atlas = std::make_unique<ImFontAtlas>();
            auto cfg = m_defaultConfig;

            // Calculate the expected font size
            auto size = fontSize;
            if (font.defaultSize.has_value())
                size = font.defaultSize.value() * std::max(1.0F, std::floor(ImHexApi::System::getGlobalScale()));
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
            m_fontData.clear();
            m_defaultConfig.MergeMode = false;
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
        ImFontAtlas* m_fontAtlas = nullptr;
        std::vector<std::pair<bool, float>> m_fontSizes;
        ImFontConfig m_defaultConfig;
        std::list<ImFontConfig> m_fontConfigs;
        ImVector<ImWchar> m_glyphRange;

        std::list<std::vector<u8>> m_fontData;
    };

}