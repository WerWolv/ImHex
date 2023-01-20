#include "init/tasks.hpp"

#include <imgui.h>
#include <imgui_freetype.h>
#include <romfs/romfs.hpp>

#include <hex/api/content_registry.hpp>
#include <hex/api/project_file_manager.hpp>
#include <hex/api/theme_manager.hpp>
#include <hex/ui/view.hpp>
#include <hex/helpers/net.hpp>
#include <hex/helpers/fs.hpp>
#include <hex/helpers/logger.hpp>

#include <fonts/fontawesome_font.h>
#include <fonts/codicons_font.h>
#include <fonts/unifont_font.h>

#include <hex/api/plugin_manager.hpp>

#include <filesystem>

#include <nlohmann/json.hpp>

namespace hex::init {

    using namespace std::literals::string_literals;

    static bool checkForUpdates() {
        // documentation of the value above the setting definition
        int showCheckForUpdates = ContentRegistry::Settings::read("hex.builtin.setting.general", "hex.builtin.setting.general.check_for_updates", 2);
        if (showCheckForUpdates == 1){
            hex::Net net;

            auto releases = net.getJson(GitHubApiURL + "/releases/latest"s, 2000).get();
            if (releases.code != 200)
                return false;

            if (!releases.body.contains("tag_name") || !releases.body["tag_name"].is_string())
                return false;

            auto versionString = std::string(IMHEX_VERSION);
            size_t versionLength = std::min(versionString.find_first_of('-'), versionString.length());
            auto currVersion   = "v" + versionString.substr(0, versionLength);
            auto latestVersion = releases.body["tag_name"].get<std::string_view>();

            if (latestVersion != currVersion)
                ImHexApi::System::impl::addInitArgument("update-available", latestVersion.data());

        }
        return true;
    }

    bool setupEnvironment() {
        hex::log::debug("Using romfs: '{}'", romfs::name());

        Net::setCACert(std::string(romfs::get("cacert.pem").string()));

        return true;
    }

    bool createDirectories() {
        bool result = true;

        using enum fs::ImHexPath;

        // Create all folders
        for (u32 path = 0; path < u32(fs::ImHexPath::END); path++) {
            for (auto &folder : fs::getDefaultPaths(static_cast<fs::ImHexPath>(path), true)) {
                try {
                    fs::createDirectories(folder);
                } catch (...) {
                    log::error("Failed to create folder {}!", hex::toUTF8String(folder));
                    result = false;
                }
            }
        }

        if (!result)
            ImHexApi::System::impl::addInitArgument("folder-creation-error");

        return result;
    }

    static bool loadFontsImpl(bool loadUnicode) {
        float fontSize = ImHexApi::System::getFontSize();

        const auto &fontFile = ImHexApi::System::getCustomFontPath();

        auto fonts       = IM_NEW(ImFontAtlas)();
        ImFontConfig cfg = {};
        cfg.OversampleH = cfg.OversampleV = 1, cfg.PixelSnapH = true;
        cfg.SizePixels = fontSize;

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

            {
                constexpr static ImWchar controlCodeRange[]   = { 0x0000, 0x001F, 0 };
                constexpr static ImWchar extendedAsciiRange[] = { 0x007F, 0x00FF, 0 };
                glyphRangesBuilder.AddRanges(controlCodeRange);
                glyphRangesBuilder.AddRanges(extendedAsciiRange);
            }

            glyphRangesBuilder.BuildRanges(&ranges);
        }

        ImWchar fontAwesomeRange[] = {
                ICON_MIN_FA, ICON_MAX_FA, 0
        };

        ImWchar codiconsRange[] = {
                ICON_MIN_VS, ICON_MAX_VS, 0
        };

        if (fontFile.empty()) {
            // Load default font if no custom one has been specified

            fonts->Clear();
            fonts->AddFontDefault(&cfg);
        } else {
            // Load custom font
            fonts->AddFontFromFileTTF(hex::toUTF8String(fontFile).c_str(), 0, &cfg, ranges.Data);
        }

        cfg.MergeMode = true;

        fonts->AddFontFromMemoryCompressedTTF(font_awesome_compressed_data, font_awesome_compressed_size, 0, &cfg, fontAwesomeRange);
        fonts->AddFontFromMemoryCompressedTTF(codicons_compressed_data, codicons_compressed_size, 0, &cfg, codiconsRange);

        if (loadUnicode)
            fonts->AddFontFromMemoryCompressedTTF(unifont_compressed_data, unifont_compressed_size, 0, &cfg, ranges.Data);

        if (!fonts->Build()) {
            if (loadUnicode) {
                log::error("Failed to build font atlas! Disabling Unicode support.");
                ContentRegistry::Settings::write("hex.builtin.setting.general", "hex.builtin.setting.general.enable_unicode", false);
                IM_DELETE(fonts);
                return loadFontsImpl(false);
            } else {
                log::error("Failed to build font atlas! Check your Graphics driver!");
                return false;
            }
        }

        View::setFontAtlas(fonts);
        View::setFontConfig(cfg);

        return true;
    }

    bool loadFonts() {
        return loadFontsImpl(ContentRegistry::Settings::read("hex.builtin.setting.general", "hex.builtin.setting.general.enable_unicode", true));
    }

    bool deleteSharedData() {
        EventManager::clear();

        while (ImHexApi::Provider::isValid())
            ImHexApi::Provider::remove(ImHexApi::Provider::get());
        ContentRegistry::Provider::getEntries().clear();

        ImHexApi::System::getInitArguments().clear();
        ImHexApi::HexEditor::impl::getBackgroundHighlights().clear();
        ImHexApi::HexEditor::impl::getForegroundHighlights().clear();
        ImHexApi::HexEditor::impl::getBackgroundHighlightingFunctions().clear();
        ImHexApi::HexEditor::impl::getForegroundHighlightingFunctions().clear();
        ImHexApi::HexEditor::impl::getTooltips().clear();
        ImHexApi::HexEditor::impl::getTooltipFunctions().clear();

        ContentRegistry::Settings::getEntries().clear();
        ContentRegistry::Settings::getSettingsData().clear();

        ContentRegistry::CommandPaletteCommands::getEntries().clear();

        ContentRegistry::PatternLanguage::getFunctions().clear();
        ContentRegistry::PatternLanguage::getPragmas().clear();
        ContentRegistry::PatternLanguage::impl::getVisualizers().clear();

        {
            auto &views = ContentRegistry::Views::getEntries();
            for (auto &[name, view] : views)
                delete view;
            views.clear();
        }


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
        ContentRegistry::Interface::getSidebarItems().clear();
        ContentRegistry::Interface::getTitleBarButtons().clear();
        ContentRegistry::Interface::getLayouts().clear();

        ShortcutManager::clearShortcuts();

        TaskManager::getRunningTasks().clear();

        ContentRegistry::DataProcessorNode::getEntries().clear();

        ContentRegistry::DataFormatter::getEntries().clear();
        ContentRegistry::FileHandler::getEntries().clear();
        ContentRegistry::Hashes::impl::getHashes().clear();

        api::ThemeManager::reset();

        {
            auto &visualizers = ContentRegistry::HexEditor::impl::getVisualizers();
            for (auto &[name, visualizer] : visualizers)
                delete visualizer;
            visualizers.clear();
        }

        ProjectFile::getHandlers().clear();
        ProjectFile::getProviderHandlers().clear();

        fs::setFileBrowserErrorCallback(nullptr);

        return true;
    }

    bool loadPlugins() {
        for (const auto &dir : fs::getDefaultPaths(fs::ImHexPath::Plugins)) {
            PluginManager::load(dir);
        }

        auto &plugins = PluginManager::getPlugins();

        if (plugins.empty()) {
            log::error("No plugins found!");

            ImHexApi::System::impl::addInitArgument("no-plugins");
            return false;
        }

        const auto shouldLoadPlugin = [executablePath = hex::fs::getExecutablePath()](const Plugin &plugin) {
            #if !defined(DEBUG)
                return true;
            #endif

            if (!executablePath.has_value())
                return true;

            // In debug builds, ignore all plugins that are not part of the executable directory
            return !std::fs::relative(plugin.getPath(), executablePath->parent_path()).string().starts_with("..");
        };

        u32 builtinPlugins = 0;
        u32 loadErrors     = 0;
        for (const auto &plugin : plugins) {
            if (!plugin.isBuiltinPlugin()) continue;

            if (!shouldLoadPlugin(plugin)) {
                log::debug("Skipping built-in plugin {}", plugin.getPath().string());
                continue;
            }

            builtinPlugins++;
            if (builtinPlugins > 1) continue;

            if (!plugin.initializePlugin()) {
                log::error("Failed to initialize plugin {}", hex::toUTF8String(plugin.getPath().filename()));
                loadErrors++;
            }
        }

        for (const auto &plugin : plugins) {
            if (plugin.isBuiltinPlugin()) continue;

            if (!shouldLoadPlugin(plugin)) {
                log::debug("Skipping plugin {}", plugin.getPath().string());
                continue;
            }

            if (!plugin.initializePlugin()) {
                log::error("Failed to initialize plugin {}", hex::toUTF8String(plugin.getPath().filename()));
                loadErrors++;
            }
        }

        if (loadErrors == plugins.size()) {
            log::error("No plugins loaded successfully!");
            ImHexApi::System::impl::addInitArgument("no-plugins");
            return false;
        }

        if (builtinPlugins == 0) {
            log::error("Built-in plugin not found!");
            ImHexApi::System::impl::addInitArgument("no-builtin-plugin");
            return false;
        } else if (builtinPlugins > 1) {
            log::error("Found more than one built-in plugin!");
            ImHexApi::System::impl::addInitArgument("multiple-builtin-plugins");
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

            ContentRegistry::Settings::clear();
            ContentRegistry::Settings::store();

            return false;
        }

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
            { "Setting up environment",  setupEnvironment,    false },
            { "Creating directories",    createDirectories,   false },
            { "Loading settings",        loadSettings,        false },
            { "Loading plugins",         loadPlugins,         false },
            { "Checking for updates",    checkForUpdates,     true  },
            { "Loading fonts",           loadFonts,           true  },
        };
    }

    std::vector<Task> getExitTasks() {
        return {
            { "Saving settings...",         storeSettings,    false },
            { "Cleaning up shared data...", deleteSharedData, false },
            { "Unloading plugins...",       unloadPlugins,    false },
        };
    }

}