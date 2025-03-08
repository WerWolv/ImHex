#include <hex/plugin.hpp>

#include <hex/api/content_registry.hpp>
#include <hex/api/events/events_gui.hpp>
#include <hex/helpers/logger.hpp>

#include <romfs/romfs.hpp>
#include <font_atlas.hpp>
#include <font_settings.hpp>
#include <imgui_impl_opengl3.h>
#include <fonts/fonts.hpp>

namespace hex::fonts {

    void registerFonts();

    bool buildFontAtlas(FontAtlas *fontAtlas, std::fs::path fontPath, bool pixelPerfectFont, float fontSize, bool loadUnicodeCharacters, bool bold, bool italic, bool antialias);

    static AutoReset<std::map<UnlocalizedString, std::unique_ptr<FontAtlas>>> s_fontAtlases;

    void loadFont(const ContentRegistry::Settings::Widgets::Widget &widget, const UnlocalizedString &name, ImFont **font, float scale) {
        const auto &settings = static_cast<const FontSelector&>(widget);

        auto atlas = std::make_unique<FontAtlas>();

        const bool atlasBuilt = buildFontAtlas(
            atlas.get(),
            settings.getFontPath(),
            settings.isPixelPerfectFont(),
            settings.getFontSize() * scale,
            true,
            settings.isBold(),
            settings.isItalic(),
            settings.isAntiAliased()
        );

        if (!atlasBuilt) {
            buildFontAtlas(
                atlas.get(),
                "",
                false,
                settings.getFontSize() * scale,
                false,
                settings.isBold(),
                settings.isItalic(),
                settings.isAntiAliased()
            );

            log::error("Failed to load font {}! Reverting back to default font!", name.get());
        }

        *font = atlas->getAtlas()->Fonts[0];

        (*s_fontAtlases)[name] = std::move(atlas);
    }

    bool setupFonts() {
        ContentRegistry::Settings::add<ContentRegistry::Settings::Widgets::Checkbox>("hex.fonts.setting.font", "hex.fonts.setting.font.glyphs", "hex.fonts.setting.font.load_all_unicode_chars", false).requiresRestart();

        for (auto &[name, font] : ImHexApi::Fonts::impl::getFontDefinitions()) {
            auto &widget = addFontSettingsWidget(name)
                .setChangedCallback([name, &font, firstLoad = true](auto &widget) mutable {
                    if (firstLoad) {
                        firstLoad = false;
                        return;
                    }

                    TaskManager::doLater([&name, &font, &widget] {
                        loadFont(widget, name, &font, ImHexApi::System::getGlobalScale() * ImHexApi::System::getBackingScaleFactor());
                    });
                });

            loadFont(widget.getWidget(), name, &font, ImHexApi::System::getGlobalScale() * ImHexApi::System::getBackingScaleFactor());
            EventDPIChanged::subscribe(font, [&widget, name, &font](float, float newScaling) {
                loadFont(widget.getWidget(), name, &font, ImHexApi::System::getGlobalScale() * newScaling);
            });
        }

        return true;
    }

    ImU32 Bitmap::crossCorrelate(ImU8 *input, ImU32 inLen, ImU32 skipY) {
        if (inLen >= m_width)
            return 0;
        ImU32 steps = (m_width - inLen)/3+1;
        std::vector<ImU8> y(steps, 0);
        ImU32 maxVal = 0;
        ImU32 maxOffset = 0;
        for (ImU32 n = 0; n < steps; ++n) {
            for (ImU32 k = 0; k < inLen; ++k)
                y[n] += input[k] * m_data[skipY * m_pitch + 3*n + k];
            if (y[n] > maxVal) {
                maxVal = y[n];
                maxOffset = n;
            }
        }
        return maxOffset*3;
    }

    ImU8 absDiff(ImU8 *a, ImU8 *b, ImU32 len) {
        ImU8 diff = 0;
        for (ImU32 i = 0; i < len; i++)
            diff += std::abs(a[i] - b[i]);
        return diff;
    }

    void Bitmap::lcdFilter() {
        if (m_width * m_height == 0)
            return;
        std::vector<ImU8> h = {8,0x4D,0x55,0x4D,8};
        auto paddedWidth = m_width + 4;
        auto fWidth = paddedWidth + ((3 - (paddedWidth % 3)) % 3);
        auto fPitch = (fWidth + 3) & (-4);
        Bitmap f(fWidth, m_height, fPitch);
        //std::vector<std::vector<ImU8>> fir(3, std::vector<ImU8>(3, 0));
        // first output: h[0]*input[0]/255.0f;
        // 2nd output:  (h[1]*input[0]   + h[0]*input[1])/255.0f;
        // 3rd output:  (h[2]*input[0]   + h[1]*input[1]   + h[0]*input[2])/255.0f;
        // 4th output:  (h[1]*input[0]   + h[2]*input[1]   + h[1]*input[2]   + h[0]*input[3])/255.0f;
        // ith output:  (h[0]*input[5+i] + h[1]*input[i+6] + h[2]*input[i+7] + h[1]*input[i+8] + h[0]*input[i+9])/255.0f;
        // aap output:  (h[0]*input[N-4] + h[1]*input[N-3] + h[2]*input[N-2] + h[1]*input[N-1])/255.0f;
        // ap output:   (h[0]*input[N-3] + h[1]*input[N-2] + h[2]*input[N-1])/255.0f;
        // p output:    (h[0]*input[N-2] + h[1]*input[N-1] )/255.0f;
        // last output:  h[0]*input[N-1]/255.0f;
        for (ImU32 y = 0; y < m_height; y++) {
            auto fp = y * fPitch;
            auto yp = y * m_pitch;
            bool done = false;
            for (ImU32 x = 0; x < 4; x++) {
                ImU32 head = 0;
                ImU32 tail = 0;
                if (m_width >= x + 1) {
                    for (ImU32 i = 0; i < x + 1; i++) {
                        head += h[x - i] * m_data[yp + i];
                        tail += h[i] * m_data[yp + m_width - 1 - x + i];
                    }
                    f.m_data[fp + x] = head >> 8;
                    f.m_data[fp + paddedWidth - 1 - x] = tail >> 8;
                } else {
                    done = true;
                    break;
                }
            }
            if (done)
                continue;
            for (ImU32 x = 4; x < paddedWidth - 4; x++) {
                ImU32 head = 0;
                for (ImS32 i = 0; i < 5; i++) {
                    head += h[i] * m_data[yp + i + x - 4];
                }
                f.m_data[fp + x] = head >> 8;
            }
        }
        clear();
        m_width = f.m_width;
        m_height = f.m_height;
        m_pitch = f.m_pitch;
        m_data = std::move(f.m_data);
    }

    void Bitmap::resize(ImU32 &width, ImU32 &height, ImU32 skipX, ImU32 skipY) {
        if (m_width < width + skipX || m_height < height + skipY)
            return;
        auto pitch = (width + 3) & (-4);
        auto padding = pitch - width;
        bool clearPadding = true;
        if (pitch <= m_width) {
            width = pitch;
            clearPadding = false;
        }
        Bitmap newBitmap(width, height, pitch);
        for (ImU32 y = 0; y < height; y++) {
            for (ImU32 x = 0; x < width; x++) {
                newBitmap.m_data[y * pitch + x] = m_data[(y + skipY) * m_pitch + x + skipX];
            }
            if (clearPadding) {
                for (ImU32 x = 0; x < padding; x++)
                    newBitmap.m_data[y * pitch + width + x] = 0;
            }
        }
        clear();
        m_width = newBitmap.m_width;
        m_height = newBitmap.m_height;
        m_pitch = newBitmap.m_pitch;
        m_data = std::move(newBitmap.m_data);
    }

}

IMHEX_LIBRARY_SETUP("Fonts") {
    hex::log::debug("Using romfs: '{}'", romfs::name());
    for (auto &path : romfs::list("lang"))
        hex::ContentRegistry::Language::addLocalization(nlohmann::json::parse(romfs::get(path).string()));

    hex::ImHexApi::Fonts::registerFont("hex.fonts.font.default");
    hex::ImHexApi::Fonts::registerFont("hex.fonts.font.hex_editor");
    hex::ImHexApi::Fonts::registerFont("hex.fonts.font.code_editor");

    hex::ImHexApi::System::addStartupTask("Loading fonts", true, hex::fonts::setupFonts);
    hex::fonts::registerFonts();
}