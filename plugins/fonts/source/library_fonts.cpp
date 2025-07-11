#include <hex/plugin.hpp>

#include <hex/api/content_registry.hpp>
#include <hex/api/events/events_gui.hpp>
#include <hex/helpers/logger.hpp>

#include <romfs/romfs.hpp>
#include <font_settings.hpp>
#include <imgui_freetype.h>
#include <fonts/fonts.hpp>
#include <hex/api/task_manager.hpp>
#include <hex/api/events/events_lifecycle.hpp>

namespace hex::fonts {

    void registerFonts();
    void loadFont(const ContentRegistry::Settings::Widgets::Widget &widget, const UnlocalizedString &name, ImFont **imguiFont) {
        const auto &settings = static_cast<const FontSelector&>(widget);

        auto atlas = ImGui::GetIO().Fonts;

        {
            auto &font = *imguiFont;

            if (font != nullptr) {
                atlas->RemoveFont(font);

                font = nullptr;
            }
        }

        ImFontConfig config;
        config.MergeMode = false;
        config.FontDataOwnedByAtlas = false;
        config.SizePixels = settings.getFontSize();

        std::memcpy(config.Name, name.get().c_str(), std::min(name.get().size(), sizeof(config.Name) - 1));

        if (!settings.isPixelPerfectFont()) {
            if (settings.isBold())
                config.FontLoaderFlags |= ImGuiFreeTypeLoaderFlags_Bold;
            if (settings.isItalic())
                config.FontLoaderFlags |= ImGuiFreeTypeLoaderFlags_Oblique;
            switch (settings.getAntialiasingType()) {
                case AntialiasingType::None:
                    config.FontLoaderFlags |= ImGuiFreeTypeLoaderFlags_Monochrome | ImGuiFreeTypeLoaderFlags_MonoHinting;
                    break;
                case AntialiasingType::Grayscale:
                    break;
                case AntialiasingType::Lcd:
                    config.FontLoaderFlags |= ImGuiFreeTypeLoaderFlags_SubPixel;
                    break;
            }
        } else {
            config.FontLoaderFlags |= ImGuiFreeTypeLoaderFlags_NoHinting;
        }

        {
            const auto fontPath = settings.getFontPath();
            if (!fontPath.empty())
                *imguiFont = atlas->AddFontFromFileTTF(fontPath.string().c_str(), 0.0F, &config);

            if (*imguiFont == nullptr) {
                if (settings.isPixelPerfectFont()) {
                    auto defaultConfig = config;
                    defaultConfig.SizePixels = ImHexApi::Fonts::DefaultFontSize;
                    *imguiFont = atlas->AddFontDefault(&defaultConfig);
                } else {
                    static auto jetbrainsFont = romfs::get("fonts/JetBrainsMono.ttf");
                    *imguiFont = atlas->AddFontFromMemoryTTF(const_cast<u8 *>(jetbrainsFont.data<u8>()), jetbrainsFont.size(), 0.0F, &config);

                    if (*imguiFont == nullptr) {
                        log::error("Failed to load font '{}', using default font instead", name.get());
                        *imguiFont = atlas->AddFontDefault();
                    }
                }
            }
        }

        config.MergeMode = true;
        for (auto &extraFont : ImHexApi::Fonts::impl::getMergeFonts()) {
            config.GlyphOffset = { extraFont.offset.x, -extraFont.offset.y };
            config.GlyphOffset *= ImHexApi::System::getGlobalScale();
            atlas->AddFontFromMemoryTTF(const_cast<u8 *>(extraFont.fontData.data()), extraFont.fontData.size(), 0.0F, &config);
        }
    }

    bool setupFonts() {
        for (auto &[name, font] : ImHexApi::Fonts::impl::getFontDefinitions()) {
            auto &widget = addFontSettingsWidget(name)
                .setChangedCallback([name, &font, firstLoad = true](auto &widget) mutable {
                    if (firstLoad) {
                        firstLoad = false;
                        return;
                    }

                    TaskManager::doLater([name, &font, &widget] {
                        loadFont(widget, name, &font);
                    });
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

    hex::fonts::registerFonts();

    hex::EventImHexStartupFinished::subscribe([] {
        hex::fonts::setupFonts();
        hex::ImHexApi::Fonts::setDefaultFont(hex::fonts::Default());
    });

    hex::EventDPIChanged::subscribe([](float, float newScale) {
        ImGui::GetIO().ConfigDpiScaleFonts = newScale;
    });
}