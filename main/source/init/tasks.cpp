#include "init/tasks.hpp"

#include <imgui.h>
#include <imgui_freetype.h>

#include <hex/helpers/net.hpp>
#include <hex/api/content_registry.hpp>
#include <hex/pattern_language/pattern_data.hpp>
#include <hex/helpers/paths.hpp>

#include <fontawesome_font.h>
#include <codicons_font.h>
#include <unifont_font.h>

#include <hex/api/plugin_manager.hpp>

#include <filesystem>

#include <nlohmann/json.hpp>

namespace hex::init {

    using namespace std::literals::string_literals;

    static bool checkForUpdates() {
        hex::Net net;

        auto releases = net.getJson(GitHubApiURL + "/releases/latest"s, 2000).get();
        if (releases.code != 200)
            return false;

        if (!releases.body.contains("tag_name") || !releases.body["tag_name"].is_string())
            return false;

        auto versionString = std::string(IMHEX_VERSION);
        auto currVersion   = "v" + versionString.substr(0, versionString.find_first_of('-'));
        auto latestVersion = releases.body["tag_name"].get<std::string_view>();

        if (latestVersion != currVersion)
            ImHexApi::System::getInitArguments().insert({ "update-available", latestVersion.data() });

        return true;
    }

    static bool downloadInformation() {
        hex::Net net;

        auto tip = net.getString(ImHexApiURL + "/tip"s, 200).get();
        if (tip.code != 200)
            return false;

        ImHexApi::System::getInitArguments().insert({ "tip-of-the-day", tip.body });

        return true;
    }

    bool createDirectories() {
        bool result = true;

        constexpr std::array paths = {
            ImHexPath::Patterns,
            ImHexPath::PatternsInclude,
            ImHexPath::Magic,
            ImHexPath::Plugins,
            ImHexPath::Resources,
            ImHexPath::Config,
            ImHexPath::Constants,
            ImHexPath::Yara,
            ImHexPath::Encodings,
            ImHexPath::Python,
            ImHexPath::Logs
        };

        for (auto path : paths) {
            for (auto &folder : hex::getPath(path, true)) {
                try {
                    fs::create_directories(folder);
                } catch (...) {
                    log::error("Failed to create folder {}!", folder.string());
                    result = false;
                }
            }
        }

        if (!result)
            ImHexApi::System::getInitArguments().insert({ "folder-creation-error", {} });

        return result;
    }

    bool loadFonts() {
        auto fonts       = IM_NEW(ImFontAtlas)();
        ImFontConfig cfg = {};

        fs::path fontFile;
        for (const auto &dir : hex::getPath(ImHexPath::Resources)) {
            auto path = dir / "font.ttf";
            if (fs::exists(path)) {
                log::info("Loading custom front from {}", path.string());

                fontFile = path;
                break;
            }
        }

        ImVector<ImWchar> ranges;
        {
            ImFontGlyphRangesBuilder glyphRangesBuilder;
            glyphRangesBuilder.AddRanges(fonts->GetGlyphRangesDefault());
            glyphRangesBuilder.AddRanges(fonts->GetGlyphRangesJapanese());
            glyphRangesBuilder.AddRanges(fonts->GetGlyphRangesChineseFull());
            glyphRangesBuilder.AddRanges(fonts->GetGlyphRangesCyrillic());
            glyphRangesBuilder.AddRanges(fonts->GetGlyphRangesKorean());
            glyphRangesBuilder.AddRanges(fonts->GetGlyphRangesThai());
            glyphRangesBuilder.AddRanges(fonts->GetGlyphRangesVietnamese());
            glyphRangesBuilder.BuildRanges(&ranges);
        }

        ImWchar fontAwesomeRange[] = {
            ICON_MIN_FA, ICON_MAX_FA, 0
        };

        ImWchar codiconsRange[] = {
            ICON_MIN_VS, ICON_MAX_VS, 0
        };

        ImWchar unifontRange[] = {
            0x0020, 0xFFF0, 0
        };

        float fontSize = 13.0F * ImHexApi::System::getGlobalScale();
        if (fontFile.empty()) {
            // Load default font

            fonts->Clear();

            cfg.OversampleH = cfg.OversampleV = 1, cfg.PixelSnapH = true;
            cfg.SizePixels = fontSize;
            fonts->AddFontDefault(&cfg);
        } else {
            // Load custom font

            fontSize = ContentRegistry::Settings::read("hex.builtin.setting.interface", "hex.builtin.setting.interface.font_size", 14) * ImHexApi::System::getGlobalScale();

            cfg.OversampleH = cfg.OversampleV = 1, cfg.PixelSnapH = true;
            cfg.SizePixels = fontSize;

            fonts->AddFontFromFileTTF(fontFile.string().c_str(), std::floor(fontSize), &cfg, ranges.Data);    // Needs conversion to char for Windows
        }

        cfg.MergeMode = true;

        fonts->AddFontFromMemoryCompressedTTF(font_awesome_compressed_data, font_awesome_compressed_size, fontSize, &cfg, fontAwesomeRange);
        fonts->AddFontFromMemoryCompressedTTF(codicons_compressed_data, codicons_compressed_size, fontSize, &cfg, codiconsRange);
        fonts->AddFontFromMemoryCompressedTTF(unifont_compressed_data, unifont_compressed_size, fontSize, &cfg, unifontRange);

        ImGuiFreeType::BuildFontAtlas(fonts);

        View::setFontAtlas(fonts);
        View::setFontConfig(cfg);

        return true;
    }

    bool deleteSharedData() {
        ImHexApi::Tasks::getDeferredCalls().clear();

        while (ImHexApi::Provider::isValid())
            ImHexApi::Provider::remove(ImHexApi::Provider::get());
        ContentRegistry::Provider::getEntries().clear();

        ContentRegistry::Settings::getEntries().clear();
        ContentRegistry::Settings::getSettingsData().clear();

        ContentRegistry::CommandPaletteCommands::getEntries().clear();
        ContentRegistry::PatternLanguage::getFunctions().clear();
        ContentRegistry::PatternLanguage::getPalettes().clear();

        for (auto &[name, view] : ContentRegistry::Views::getEntries())
            delete view;
        ContentRegistry::Views::getEntries().clear();

        ContentRegistry::Tools::getEntries().clear();
        ContentRegistry::DataInspector::getEntries().clear();

        ContentRegistry::Language::getLanguages().clear();
        ContentRegistry::Language::getLanguageDefinitions().clear();
        LangEntry::resetLanguageStrings();

        ContentRegistry::Interface::getWelcomeScreenEntries().clear();
        ContentRegistry::Interface::getFooterItems().clear();
        ContentRegistry::Interface::getToolbarItems().clear();
        ContentRegistry::Interface::getMainMenuItems().clear();
        ContentRegistry::Interface::getMenuItems().clear();
        ContentRegistry::Interface::getTitleBarButtons().clear();

        ShortcutManager::clearShortcuts();

        hex::Task::getRunningTasks().clear();

        ContentRegistry::DataProcessorNode::getEntries().clear();

        ContentRegistry::DataFormatter::getEntries().clear();
        ContentRegistry::FileHandler::getEntries().clear();

        return true;
    }

    bool loadPlugins() {
        for (const auto &dir : hex::getPath(ImHexPath::Plugins)) {
            PluginManager::load(dir);
        }

        auto &plugins = PluginManager::getPlugins();

        if (plugins.empty()) {
            log::error("No plugins found!");

            ImHexApi::System::getInitArguments().insert({ "no-plugins", {} });
            return false;
        }

        u32 builtinPlugins = 0;
        u32 loadErrors     = 0;
        for (const auto &plugin : plugins) {
            if (!plugin.isBuiltinPlugin()) continue;
            builtinPlugins++;
            if (builtinPlugins > 1) continue;

            if (!plugin.initializePlugin()) {
                log::error("Failed to initialize plugin {}", plugin.getPath().filename().string());
                loadErrors++;
            }
        }

        for (const auto &plugin : plugins) {
            if (plugin.isBuiltinPlugin()) continue;

            if (!plugin.initializePlugin()) {
                log::error("Failed to initialize plugin {}", plugin.getPath().filename().string());
                loadErrors++;
            }
        }

        if (loadErrors == plugins.size()) {
            log::error("No plugins loaded successfully!");
            ImHexApi::System::getInitArguments().insert({ "no-plugins", {} });
            return false;
        }

        if (builtinPlugins == 0) {
            log::error("Built-in plugin not found!");
            ImHexApi::System::getInitArguments().insert({ "no-builtin-plugin", {} });
            return false;
        } else if (builtinPlugins > 1) {
            log::error("Found more than one built-in plugin!");
            ImHexApi::System::getInitArguments().insert({ "multiple-builtin-plugins", {} });
            return false;
        }

        return true;
    }

    bool unloadPlugins() {
        PluginManager::unload();

        return true;
    }

    bool loadSettings() {
        try {
            ContentRegistry::Settings::load();
        } catch (std::exception &e) {
            log::error("Failed to load configuration! {}", e.what());
            return false;
        }

        float interfaceScaling = 1.0F;
        switch (ContentRegistry::Settings::read("hex.builtin.setting.interface", "hex.builtin.setting.interface.scaling", 0)) {
            default:
            case 0:
                // Native scaling
                break;
            case 1:
                interfaceScaling = 0.5F;
                break;
            case 2:
                interfaceScaling = 1.0F;
                break;
            case 3:
                interfaceScaling = 1.5F;
                break;
            case 4:
                interfaceScaling = 2.0F;
                break;
        }

        ImHexApi::System::impl::setGlobalScale(interfaceScaling);

        return true;
    }

    bool storeSettings() {
        try {
            ContentRegistry::Settings::store();
        } catch (std::exception &e) {
            log::error("Failed to store configuration! {}", e.what());
            return false;
        }
        return true;
    }

    std::vector<Task> getInitTasks() {
        return {
            {"Checking for updates...",     checkForUpdates    },
            { "Downloading information...", downloadInformation},
            { "Creating directories...",    createDirectories  },
            { "Loading settings...",        loadSettings       },
            { "Loading plugins...",         loadPlugins        },
            { "Loading fonts...",           loadFonts          },
        };
    }

    std::vector<Task> getExitTasks() {
        return {
            {"Saving settings...",          storeSettings   },
            { "Cleaning up shared data...", deleteSharedData},
            { "Unloading plugins...",       unloadPlugins   },
        };
    }

}