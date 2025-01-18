#include <hex/plugin.hpp>

#include <hex/api/content_registry.hpp>
#include <hex/helpers/logger.hpp>

#include <romfs/romfs.hpp>
#include <font_atlas.hpp>
#include <font_settings.hpp>
#include <imgui_impl_opengl3.h>
#include <fonts/fonts.hpp>

namespace hex::fonts {

    void registerFonts();

    bool buildFontAtlas(FontAtlas *fontAtlas, std::fs::path fontPath, bool pixelPerfectFont, float fontSize, bool loadUnicodeCharacters, bool bold, bool italic, bool antialias);

    static AutoReset<std::map<ImFont*, FontAtlas>> s_fontAtlases;

    void loadFont(const ContentRegistry::Settings::Widgets::Widget &widget, const UnlocalizedString &name, ImFont **font) {
        const auto &settings = static_cast<const FontSelector&>(widget);

        FontAtlas atlas;

        const bool atlasBuilt = buildFontAtlas(
            &atlas,
            settings.getFontPath(),
            settings.isPixelPerfectFont(),
            settings.getFontSize(),
            true,
            settings.isBold(),
            settings.isItalic(),
            settings.isAntiAliased()
        );

        if (!atlasBuilt) {
            buildFontAtlas(
                &atlas,
                "",
                false,
                settings.getFontSize(),
                false,
                settings.isBold(),
                settings.isItalic(),
                settings.isAntiAliased()
            );

            log::error("Failed to load font {}! Reverting back to default font!", name.get());
        }

        if (*font != nullptr) {
            EventDPIChanged::unsubscribe(*font);
        }

        *font = atlas.getAtlas()->Fonts[0];

        s_fontAtlases->insert_or_assign(*font, std::move(atlas));
        (*s_fontAtlases)[*font] = std::move(atlas);

        EventDPIChanged::subscribe(*font, [&atlas, font](float, float newScaling) {
            atlas.updateFontScaling(newScaling);

            if (atlas.build()) {
                auto &io = ImGui::GetIO();
                auto prevFont = io.Fonts;

                {
                    io.Fonts = atlas.getAtlas();
                    ImGui_ImplOpenGL3_DestroyFontsTexture();
                    ImGui_ImplOpenGL3_CreateFontsTexture();
                    io.Fonts = prevFont;
                }

                *font = atlas.getAtlas()->Fonts[0];
            }
        });
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

                    loadFont(widget, name, &font);
                });

            loadFont(widget.getWidget(), name, &font);
        }

        return true;
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