#pragma once

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_freetype.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_LCD_FILTER_H
#include FT_BITMAP_H

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

        float calculateFontDescend(FT_Library ft, float fontSize) const {

            if (ft == nullptr) {
                log::fatal("FreeType not initialized");
                return 0.0f;
            }

            FT_Face face;
            if (FT_New_Memory_Face(ft, reinterpret_cast<const FT_Byte *>(m_font->ConfigData->FontData), m_font->ConfigData->FontDataSize, 0, &face) != 0) {
                log::fatal("Failed to load face");
                return 0.0f;
            }

            // Calculate the expected font size
            auto size = fontSize;
            if (m_font->FontSize > 0.0F)
                size = m_font->FontSize * std::max(1.0F, std::floor(ImHexApi::System::getGlobalScale()));
            else
                size = std::max(1.0F, std::floor(size / ImHexApi::Fonts::DefaultFontSize)) * ImHexApi::Fonts::DefaultFontSize;

            if (FT_Set_Pixel_Sizes(face, size, size) != 0) {
                log::fatal("Failed to set pixel size");
                return 0.0f;
            }

            return face->size->metrics.descender / 64.0F;
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

        float calculateFontDescend( FT_Library ft, const ImHexApi::Fonts::Font &font, float fontSize) const {
            if (ft == nullptr) {
                log::fatal("FreeType not initialized");
                return 0.0f;
            }
            FT_Face face;
            if (FT_New_Memory_Face(ft, reinterpret_cast<const FT_Byte *>(font.fontData.data()), font.fontData.size(), 0, &face) != 0) {
                log::fatal("Failed to load face");
                return 0.0f;
            }

            // Calculate the expected font size
            auto size = fontSize;
            if (font.defaultSize.has_value())
                size = font.defaultSize.value() * std::max(1.0F, std::floor(ImHexApi::System::getGlobalScale()));
            else
                size = std::max(1.0F, std::floor(size / ImHexApi::Fonts::DefaultFontSize)) * ImHexApi::Fonts::DefaultFontSize;

            if (FT_Set_Pixel_Sizes(face, size, size) != 0) {
                log::fatal("Failed to set pixel size");
                return false;
            }

            return face->size->metrics.descender / 64.0F;
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


    class Bitmap {
        ImU32 m_width;
        ImU32 m_height;
        ImU32 m_pitch;
        std::vector<ImU8> m_data;
    public:
        Bitmap(ImU32 width, ImU32 height, ImU32 pitch, ImU8 *data) : m_width(width), m_height(height), m_pitch(pitch), m_data(std::vector<ImU8>(data, data + pitch * height)) {}
        Bitmap(ImU32 width, ImU32 height, ImU32 pitch) : m_width(width), m_height(height), m_pitch(pitch) { m_data.resize(pitch * height); }
        void clear() { m_data.clear(); }
        //ImU8 convertToGray(ImU32 x, ImU32 y) const;
        void resizeWithoutFilter(ImU32 &width, ImU32 &height, ImU32 &pitch, ImU8 *data);
        void resize(ImU32 &width, ImU32 &height, ImU32 skipX, ImU32 skipY);
        ImU32 getWidth() const { return m_width; }
        ImU32 getHeight() const { return m_height; }
        ImU32 getPitch() const { return m_pitch; }
        const std::vector<ImU8> &getData() const { return m_data; }
        ImU8 *getData() { return m_data.data(); }
        void setData( ImU32 width, ImU32 height, ImU32 pitch, std::vector<ImU8> data) {
            m_width = width;
            m_height = height;
            m_pitch = pitch;
            m_data = std::move(data);
        }
        ImU32 crossCorrelate( ImU8 *input, ImU32 inLen, ImU32 skipY);
        void lcdFilter();
        //void blend( Bitmap &bitmap, ImU32 x, ImU32 y);
    };

    class RGBA {


    public:

        static float IntToFloat(ImU8 col) {
            return col / 255.0F;
        }

        static ImVec4 ConvertToFloatVec() {
            return ImVec4(IntToFloat(color.rgbaVec[0]), IntToFloat(color.rgbaVec[1]), IntToFloat(color.rgbaVec[2]), IntToFloat(color.rgbaVec[3]));
        }

        static ImU8 floatToInt(float col) {
            return std::clamp((unsigned)std::floor(col*255u),0u,255u);
        }

        static void convertFromFloatVec(ImVec4 colorVec) {
            color.rgbaVec[0] = floatToInt(colorVec.x);
            color.rgbaVec[1] = floatToInt(colorVec.y);
            color.rgbaVec[2] = floatToInt(colorVec.z);
            color.rgbaVec[3] = floatToInt(colorVec.w);
        }

        static void fromLinearRGB() {
            auto lRGB = ConvertToFloatVec();
            color.rgbaVec[0] =  lRGB.x <= 0.04045 ? lRGB.x * 12.92 : pow(lRGB.x, 1.0/2.4) * 1.055 - 0.055;
            color.rgbaVec[1] =  lRGB.y <= 0.04045 ? lRGB.y * 12.92 : pow(lRGB.y, 1.0/2.4) * 1.055 - 0.055;
            color.rgbaVec[2] =  lRGB.z <= 0.04045 ? lRGB.z * 12.92 : pow(lRGB.z, 1.0/2.4) * 1.055 - 0.055;
        }

        static void toLinearRGB() {
            auto sRGB = ConvertToFloatVec();
            color.rgbaVec[0] =  sRGB.x <= 0.0031308 ? sRGB.x / 12.92 : pow((sRGB.x + 0.055) / 1.055, 2.4);
            color.rgbaVec[1] =  sRGB.y <= 0.0031308 ? sRGB.y / 12.92 : pow((sRGB.y + 0.055) / 1.055, 2.4);
            color.rgbaVec[2] =  sRGB.z <= 0.0031308 ? sRGB.z / 12.92 : pow((sRGB.z + 0.055) / 1.055, 2.4);
        }

        static ImU8 LuminosityFromLinearRGB()
        {
            // Luminosity Y is defined by the CIE 1931 XYZ color space. Linear RGB to Y is a weighted average based on factors from the color conversion matrix:
            // Y = 0.2126*R + 0.7152*G + 0.0722*B. Computed on the integer pipe.
            return (4732UL * color.rgbaVec[0] + 46871UL * color.rgbaVec[1] + 13933UL * color.rgbaVec[2]) >> 16;
        }
        static ImU8 LuminosityFromSRGB()
        {
            auto save = RGBA::getRgba();
            toLinearRGB();
            // Luminosity Y is defined by the CIE 1931 XYZ color space. sRGB to Y is a weighted average based on factors from the color conversion matrix:
            // Y = 0.2126*R + 0.7152*G + 0.0722*B. Computed on the integer pipe.
            ImU8 grayScale = (4732UL * color.rgbaVec[0] + 46871UL * color.rgbaVec[1] + 13933UL * color.rgbaVec[2]) >> 16;
            setRgbaVec(grayScale, grayScale, grayScale, grayScale);
            fromLinearRGB();
            auto result = getRgbaVec()[0];
            setRgba(save);
            return result;
        }


        static ImU8 Luminance() {
            float value = std::max(color.rgbaVec[0], std::max(color.rgbaVec[1], color.rgbaVec[2])) / 255.0;
            float X2 = std::min(color.rgbaVec[0], std::min(color.rgbaVec[1], color.rgbaVec[2])) / 255.0;
            return (value + X2) / 2.0;
        }

        static void PreMultiplyAlpha() {
            color.rgbaVec[0] = color.rgbaVec[0] * color.rgbaVec[3] / 255;
            color.rgbaVec[1] = color.rgbaVec[1] * color.rgbaVec[3] / 255;
            color.rgbaVec[2] = color.rgbaVec[2] * color.rgbaVec[3] / 255;
        }

        static ImU32  AddAlpha(ImU8 r, ImU8 g, ImU8 b) {
            color.rgbaVec[0] = r;
            color.rgbaVec[1] = g;
            color.rgbaVec[2] = b;
            color.rgbaVec[3] = (r+g+b)/3;//luminance
            return color.rgba;
        }

        RGBA (ImU8 r, ImU8 g, ImU8 b, ImU8 a) {
            color.rgbaVec[0] = r;
            color.rgbaVec[1] = b;
            color.rgbaVec[2] = g;
            color.rgbaVec[3] = a;
        }

        RGBA (ImU32 rgba) {
            color.rgba = rgba;
        }

        bool isColorDark() {
            return Luminance() < 0.5f;
        }

        static ImU8 *getRgbaVec() {
            return color.rgbaVec;
        }

        static ImU32 getRgba() {
            return color.rgba;
        }

        static void setRgba(ImU32 rgba) {
            color.rgba = rgba;
        }

        static void setRgbaVec(ImU8 r, ImU8 g, ImU8 b, ImU8 a) {
            color.rgbaVec[0] = r;
            color.rgbaVec[1] = g;
            color.rgbaVec[2] = b;
            color.rgbaVec[3] = a;
        }

        union RGBAU{
            ImU8 rgbaVec[4];
            ImU32 rgba;
        };
        inline static RGBAU color;
    };
}