#pragma once

#include <hex.hpp>

#include <hex/api/localization_manager.hpp>

#include <span>
#include <optional>
#include <vector>

#if !defined(HEX_MODULE_EXPORT)
    struct ImFont;
#endif

EXPORT_MODULE namespace hex {

    /* Functions for adding new font types */
    namespace ImHexApi::Fonts {

        struct Offset { float x, y; };

        struct MergeFont {
            std::string name;
            std::span<const u8> fontData;
            Offset offset;
            std::optional<float> fontSizeMultiplier;
        };

        class Font {
        public:
            explicit Font(UnlocalizedString fontName);

            void push(float size = 0.0F) const;
            void pushBold(float size = 0.0F) const;
            void pushItalic(float size = 0.0F) const;

            void pop() const;

            [[nodiscard]] operator ImFont*() const;
            [[nodiscard]] const UnlocalizedString& getUnlocalizedName() const { return m_fontName; }

        private:
            void push(float size, ImFont *font) const;

        private:
            UnlocalizedString m_fontName;
        };

        struct FontDefinition {
            ImFont* regular;
            ImFont* bold;
            ImFont* italic;
        };

        namespace impl {

            const std::vector<MergeFont>& getMergeFonts();
            std::map<UnlocalizedString, FontDefinition>& getFontDefinitions();

        }

        void registerMergeFont(const std::string &name, const std::span<const u8> &data, Offset offset = {}, std::optional<float> fontSizeMultiplier = std::nullopt);

        void registerFont(const Font& font);
        FontDefinition getFont(const UnlocalizedString &fontName);

        void setDefaultFont(const Font& font);
        const Font& getDefaultFont();

        float getDpi();
        float pixelsToPoints(float pixels);
        float pointsToPixels(float points);

    }


}