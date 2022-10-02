#include "init/tasks.hpp"

#include <imgui.h>
#include <imgui_freetype.h>

#include <hex/api/content_registry.hpp>
#include <hex/api/project_file_manager.hpp>
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

        return true;
    }

    static bool downloadInformation() {
        hex::Net net;

        auto tip = net.getString(ImHexApiURL + "/tip"s, 200).get();
        if (tip.code != 200)
            return false;

        ImHexApi::System::impl::addInitArgument("tip-of-the-day", tip.body);

        return true;
    }

    bool createDirectories() {
        bool result = true;

        using enum fs::ImHexPath;

        // Check if ImHex is installed in portable mode
        {
            if (const auto executablePath = fs::getExecutablePath(); executablePath.has_value()) {
                const auto flagFile = executablePath->parent_path() / "PORTABLE";

                if (fs::exists(flagFile) && fs::isRegularFile(flagFile))
                    ImHexApi::System::impl::setPortableVersion(true);
            }
        }

        // Create all folders
        for (u32 path = 0; path < u32(fs::ImHexPath::END); path++) {
            for (auto &folder : fs::getDefaultPaths(static_cast<fs::ImHexPath>(path), true)) {
                try {
                    fs::createDirectories(folder);
                } catch (...) {
                    log::error("Failed to create folder {}!", folder.string());
                    result = false;
                }
            }
        }

        if (!result)
            ImHexApi::System::impl::addInitArgument("folder-creation-error");

        return result;
    }

    bool loadFonts() {
        auto fonts       = IM_NEW(ImFontAtlas)();
        ImFontConfig cfg = {};

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
            0x0100, 0xFFF0, 0
        };

        const auto &fontFile = ImHexApi::System::getCustomFontPath();
        float fontSize = ImHexApi::System::getFontSize();
        if (fontFile.empty()) {
            // Load default font if no custom one has been specified

            fonts->Clear();

            cfg.OversampleH = cfg.OversampleV = 1, cfg.PixelSnapH = true;
            cfg.SizePixels = fontSize;
            fonts->AddFontDefault(&cfg);
        } else {
            // Load custom font

            cfg.OversampleH = cfg.OversampleV = 1, cfg.PixelSnapH = true;
            cfg.SizePixels = fontSize;

            fonts->AddFontFromFileTTF(fontFile.string().c_str(), std::floor(fontSize), &cfg, ranges.Data);    // Needs conversion to char for Windows
        }

        cfg.MergeMode = true;

        fonts->AddFontFromMemoryCompressedTTF(font_awesome_compressed_data, font_awesome_compressed_size, fontSize, &cfg, fontAwesomeRange);
        fonts->AddFontFromMemoryCompressedTTF(codicons_compressed_data, codicons_compressed_size, fontSize, &cfg, codiconsRange);
        fonts->AddFontFromMemoryCompressedTTF(unifont_compressed_data, unifont_compressed_size, fontSize, &cfg, unifontRange);

        fonts->Build();

        View::setFontAtlas(fonts);
        View::setFontConfig(cfg);

        return true;
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

        {
            auto &visualizers = ContentRegistry::HexEditor::impl::getVisualizers();
            for (auto &[name, visualizer] : visualizers)
                delete visualizer;
            visualizers.clear();
        }

        ProjectFile::getHandlers().clear();
        ProjectFile::getProviderHandlers().clear();

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
            { "Creating directories...",    createDirectories,   false },
            { "Loading settings...",        loadSettings,        false },
            { "Loading plugins...",         loadPlugins,         false },
            { "Checking for updates...",    checkForUpdates,     true  },
            { "Downloading information...", downloadInformation, true  },
            { "Loading fonts...",           loadFonts,           true  },
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