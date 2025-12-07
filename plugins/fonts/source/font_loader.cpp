#include <hex/api/content_registry/settings.hpp>
#include <hex/helpers/logger.hpp>

#include <romfs/romfs.hpp>
#include <font_settings.hpp>
#include <fonts/fonts.hpp>
#include <hex/api/task_manager.hpp>

#include <imgui_freetype.h>

namespace hex::fonts::loader {

    void loadFont(const ContentRegistry::Settings::Widgets::Widget &widget, const UnlocalizedString &name, ImGuiFreeTypeLoaderFlags extraFlags, ImFont **imguiFont) {
        const auto &settings = static_cast<const FontSelector&>(widget);

        const auto atlas = ImGui::GetIO().Fonts;

        if (auto &font = *imguiFont; font != nullptr) {
            atlas->RemoveFont(font);

            font = nullptr;
        }

        ImFontConfig config;
        config.MergeMode = false;
        config.SizePixels = settings.getFontSize() / ImHexApi::System::getNativeScale();
        config.Flags |= ImFontFlags_NoLoadError;

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

            config.FontLoaderFlags |= extraFlags;
            if (extraFlags & ImGuiFreeTypeLoaderFlags_Bold)
                std::strncat(config.Name, " Bold", sizeof(config.Name) - 1);
            else if (extraFlags & ImGuiFreeTypeLoaderFlags_Oblique)
                std::strncat(config.Name, " Italic", sizeof(config.Name) - 1);
            else
                std::strncat(config.Name, " Regular", sizeof(config.Name) - 1);
        } else {
            config.FontLoaderFlags |= ImGuiFreeTypeLoaderFlags_NoHinting;
        }

        {
            config.FontDataOwnedByAtlas = true;
            if (const auto fontPath = settings.getFontPath(); !fontPath.empty()) {
                *imguiFont = atlas->AddFontFromFileTTF(fontPath.string().c_str(), 0.0F, &config);
            }

            if (*imguiFont == nullptr) {
                if (settings.isPixelPerfectFont()) {
                    auto defaultConfig = config;
                    defaultConfig.SizePixels = 0;
                    *imguiFont = atlas->AddFontDefault(&defaultConfig);
                    atlas->Sources.back().FontDataOwnedByAtlas = false;
                } else {
                    static auto jetbrainsFont = romfs::get("fonts/JetBrainsMono.ttf");
                    *imguiFont = atlas->AddFontFromMemoryTTF(const_cast<u8 *>(jetbrainsFont.data<u8>()), jetbrainsFont.size(), 0.0F, &config);

                    if (*imguiFont == nullptr) {
                        log::error("Failed to load font '{}', using default font instead", name.get());
                        *imguiFont = atlas->AddFontDefault();
                    } else {
                        atlas->Sources.back().FontDataOwnedByAtlas = false;
                    }
                }
            }
        }

        config.MergeMode = true;
        for (auto &extraFont : ImHexApi::Fonts::impl::getMergeFonts()) {
            config.OversampleH = 2;
            config.OversampleV = 1;
            config.RasterizerDensity = 2;
            config.GlyphOffset = { extraFont.offset.x, -extraFont.offset.y };
            config.SizePixels = settings.getFontSize() * extraFont.fontSizeMultiplier.value_or(1) / ImHexApi::System::getNativeScale();
            atlas->AddFontFromMemoryTTF(const_cast<u8 *>(extraFont.fontData.data()), extraFont.fontData.size(), 0.0F, &config);
            atlas->Sources.back().FontDataOwnedByAtlas = false;
        }
    }

    void loadFontVariations(const ContentRegistry::Settings::Widgets::Widget &widget, const UnlocalizedString &name, ImHexApi::Fonts::FontDefinition &fontDefinition) {
        loadFont(widget, name, 0, &fontDefinition.regular);
        loadFont(widget, name, ImGuiFreeTypeLoaderFlags_Bold, &fontDefinition.bold);
        loadFont(widget, name, ImGuiFreeTypeLoaderFlags_Oblique, &fontDefinition.italic);
    }

    bool loadFonts() {
        for (auto &[name, fontDefinition] : ImHexApi::Fonts::impl::getFontDefinitions()) {
            auto &widget = addFontSettingsWidget(name)
                .setChangedCallback([name, &fontDefinition](auto &widget) mutable {
                    TaskManager::doLater([name, &fontDefinition, &widget] {
                        loadFontVariations(widget, name, fontDefinition);
                    });
                });

            loadFontVariations(widget.getWidget(), name, fontDefinition);
        }

        return true;
    }
}