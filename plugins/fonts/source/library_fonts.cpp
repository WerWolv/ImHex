#include <hex/plugin.hpp>

#include <hex/api/content_registry.hpp>
#include <hex/api/events/events_gui.hpp>
#include <hex/helpers/logger.hpp>

#include <romfs/romfs.hpp>
#include <font_atlas.hpp>
#include <font_settings.hpp>

namespace hex::fonts {

    void registerFonts();

    bool buildFontAtlas(FontAtlas *fontAtlas, std::fs::path fontPath, bool pixelPerfectFont, float fontSize, bool loadUnicodeCharacters, bool bold, bool italic,const std::string &antialias);

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
            settings.antiAliasingType()
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
                settings.antiAliasingType()
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
}

IMHEX_LIBRARY_SETUP("Fonts") {
    hex::log::debug("Using romfs: '{}'", romfs::name());
    for (auto &path : romfs::list("lang"))
        hex::ContentRegistry::Language::addLocalization(nlohmann::json::parse(romfs::get(path).string()));

    hex::ImHexApi::Fonts::registerFont("hex.fonts.font.default");
    hex::ImHexApi::Fonts::registerFont("hex.fonts.font.hex_editor");
    hex::ImHexApi::Fonts::registerFont("hex.fonts.font.code_editor");

    hex::fonts::registerFonts();
}