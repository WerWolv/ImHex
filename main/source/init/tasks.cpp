#include "init/tasks.hpp"

#include <imgui.h>
#include <romfs/romfs.hpp>

#include <hex/api/content_registry.hpp>
#include <hex/api/project_file_manager.hpp>
#include <hex/api/theme_manager.hpp>
#include <hex/helpers/http_requests.hpp>
#include <hex/helpers/fs.hpp>
#include <hex/helpers/logger.hpp>
#include <hex/ui/view.hpp>
#include <hex/ui/popup.hpp>

#include <fonts/fontawesome_font.h>
#include <fonts/codicons_font.h>
#include <fonts/unifont_font.h>

#include <hex/api/plugin_manager.hpp>

#include <filesystem>

#include <nlohmann/json.hpp>

#include <wolv/io/fs.hpp>
#include <wolv/io/file.hpp>

namespace hex::init {

    using namespace std::literals::string_literals;

    static bool checkForUpdates() {
        int showCheckForUpdates = ContentRegistry::Settings::read("hex.builtin.setting.general", "hex.builtin.setting.general.check_for_updates", 2);

        // Check if we should check for updates
        if (showCheckForUpdates == 1){
            HttpRequest request("GET", GitHubApiURL + "/releases/latest"s);
            request.setTimeout(2000);

            // Query the GitHub API for the latest release version
            auto response = request.execute().get();
            if (response.getStatusCode() != 200)
                return false;

            nlohmann::json releases;
            try {
                releases = nlohmann::json::parse(response.getData());
            } catch (std::exception &e) {
                return false;
            }

            // Check if the response is valid
            if (!releases.contains("tag_name") || !releases["tag_name"].is_string())
                return false;

            // Convert the current version string to a format that can be compared to the latest release
            auto versionString = std::string(IMHEX_VERSION);
            size_t versionLength = std::min(versionString.find_first_of('-'), versionString.length());
            auto currVersion   = "v" + versionString.substr(0, versionLength);

            // Get the latest release version string
            auto latestVersion = releases["tag_name"].get<std::string_view>();

            // Check if the latest release is different from the current version
            if (latestVersion != currVersion)
                ImHexApi::System::impl::addInitArgument("update-available", latestVersion.data());

        }
        return true;
    }

    bool setupEnvironment() {
        hex::log::debug("Using romfs: '{}'", romfs::name());

        // Load the SSL certificate
        constexpr static auto CaCertFileName = "cacert.pem";

        // Look for a custom certificate in the config folder
        std::fs::path caCertPath;
        for (const auto &folder : fs::getDefaultPaths(fs::ImHexPath::Config)) {
            for (const auto &file : std::fs::directory_iterator(folder)) {
                if (file.path().filename() == CaCertFileName) {
                    caCertPath = file.path();
                    break;
                }
            }
        }

        // If a custom certificate was found, use it, otherwise use the one from the romfs
        std::string caCertData;
        if (!caCertPath.empty())
            caCertData = wolv::io::File(caCertPath, wolv::io::File::Mode::Read).readString();
        else
            caCertData = std::string(romfs::get(CaCertFileName).string());

        HttpRequest::setCACert(caCertData);

        return true;
    }

    bool createDirectories() {
        bool result = true;

        using enum fs::ImHexPath;

        // Try to create all default directories
        for (u32 path = 0; path < u32(fs::ImHexPath::END); path++) {
            for (auto &folder : fs::getDefaultPaths(static_cast<fs::ImHexPath>(path), true)) {
                try {
                    wolv::io::fs::createDirectories(folder);
                } catch (...) {
                    log::error("Failed to create folder {}!", wolv::util::toUTF8String(folder));
                    result = false;
                }
            }
        }

        if (!result)
            ImHexApi::System::impl::addInitArgument("folder-creation-error");

        return result;
    }

    bool migrateConfig(){

        // Check if there is a new config in folder
        auto configPaths = hex::fs::getDefaultPaths(hex::fs::ImHexPath::Config, false);

        // There should always be exactly one config path on Linux
        std::fs::path newConfigPath = configPaths[0];
        wolv::io::File newConfigFile(newConfigPath / "settings.json", wolv::io::File::Mode::Read);
        if (!newConfigFile.isValid()) {

            // find an old config
            std::fs::path oldConfigPath;
            for (const auto &dir : hex::fs::appendPath(hex::fs::getDataPaths(), "config")) {
                wolv::io::File oldConfigFile(dir / "settings.json", wolv::io::File::Mode::Read);
                if (oldConfigFile.isValid()) {
                    oldConfigPath = dir;
                    break;
                }
            }

            if (!oldConfigPath.empty()) {
                log::info("Found config file in {}! Migrating to {}", oldConfigPath.string(), newConfigPath.string());

                std::fs::rename(oldConfigPath / "settings.json", newConfigPath / "settings.json");
                wolv::io::File oldIniFile(oldConfigPath / "interface.ini", wolv::io::File::Mode::Read);
                if (oldIniFile.isValid()) {
                    std::fs::rename(oldConfigPath / "interface.ini", newConfigPath / "interface.ini");
                }

                std::fs::remove(oldConfigPath);
            }
        }
        return true;
    }

    static bool loadFontsImpl(bool loadUnicode) {
        float fontSize = ImHexApi::System::getFontSize();

        const auto &fontFile = ImHexApi::System::getCustomFontPath();

        // Setup basic font configuration
        auto fonts       = IM_NEW(ImFontAtlas)();
        ImFontConfig cfg = {};
        cfg.OversampleH = cfg.OversampleV = 2, cfg.PixelSnapH = true;
        cfg.SizePixels = fontSize;

        fonts->Flags |= ImFontAtlasFlags_NoPowerOfTwoHeight;

        // Configure font glyph ranges that should be loaded from the default font and unifont
        ImVector<ImWchar> ranges;
        {
            ImFontGlyphRangesBuilder glyphRangesBuilder;

            {
                constexpr static ImWchar controlCodeRange[]   = { 0x0001, 0x001F, 0 };
                constexpr static ImWchar extendedAsciiRange[] = { 0x007F, 0x00FF, 0 };

                glyphRangesBuilder.AddRanges(controlCodeRange);
                glyphRangesBuilder.AddRanges(fonts->GetGlyphRangesDefault());
                glyphRangesBuilder.AddRanges(extendedAsciiRange);
            }

            if (loadUnicode) {
                constexpr static ImWchar fullRange[]   = { 0x0100, 0xFFEF, 0 };

                glyphRangesBuilder.AddRanges(fullRange);
            } else {
                glyphRangesBuilder.AddRanges(fonts->GetGlyphRangesJapanese());
                glyphRangesBuilder.AddRanges(fonts->GetGlyphRangesChineseFull());
                glyphRangesBuilder.AddRanges(fonts->GetGlyphRangesCyrillic());
                glyphRangesBuilder.AddRanges(fonts->GetGlyphRangesKorean());
                glyphRangesBuilder.AddRanges(fonts->GetGlyphRangesThai());
                glyphRangesBuilder.AddRanges(fonts->GetGlyphRangesVietnamese());
            }

            glyphRangesBuilder.BuildRanges(&ranges);
        }

        // Glyph range for font awesome icons
        ImWchar fontAwesomeRange[] = {
                ICON_MIN_FA, ICON_MAX_FA, 0
        };

        // Glyph range for codicons icons
        ImWchar codiconsRange[] = {
                ICON_MIN_VS, ICON_MAX_VS, 0
        };

        // Load main font
        // If a custom font has been specified, load it, otherwise load the default ImGui font
        if (fontFile.empty()) {
            fonts->Clear();
            fonts->AddFontDefault(&cfg);
        } else {
            fonts->AddFontFromFileTTF(wolv::util::toUTF8String(fontFile).c_str(), 0, &cfg, ranges.Data);
        }

        // Merge all fonts into one big font atlas
        cfg.MergeMode = true;

        // Add font awesome and codicons icons to font atlas
        fonts->AddFontFromMemoryCompressedTTF(font_awesome_compressed_data, font_awesome_compressed_size, 0, &cfg, fontAwesomeRange);
        fonts->AddFontFromMemoryCompressedTTF(codicons_compressed_data, codicons_compressed_size, 0, &cfg, codiconsRange);

        // Add unifont if unicode support is enabled
        if (loadUnicode)
            fonts->AddFontFromMemoryCompressedTTF(unifont_compressed_data, unifont_compressed_size, 0, &cfg, ranges.Data);

        // Try to build the font atlas
        if (!fonts->Build()) {
            // The main reason the font atlas failed to build is that the font is too big for the GPU to handle
            // If unicode support is enabled, therefor try to load the font atlas without unicode support
            // If that still didn't work, there's probably something else going on with the graphics drivers
            // Especially Intel GPU drivers are known to have various bugs

            if (loadUnicode) {
                log::error("Failed to build font atlas! Disabling Unicode support.");
                IM_DELETE(fonts);

                // Disable unicode support in settings
                ContentRegistry::Settings::write("hex.builtin.setting.general", "hex.builtin.setting.general.enable_unicode", false);

                // Try to load the font atlas again
                return loadFontsImpl(false);
            } else {
                log::error("Failed to build font atlas! Check your Graphics driver!");
                return false;
            }
        }

        // Configure ImGui to use the font atlas
        View::setFontAtlas(fonts);
        View::setFontConfig(cfg);

        return true;
    }

    bool loadFonts() {
        return loadFontsImpl(ContentRegistry::Settings::read("hex.builtin.setting.general", "hex.builtin.setting.general.enable_unicode", true));
    }

    bool deleteSharedData() {
        // This function is called when ImHex is closed. It deletes all shared data that was created by plugins
        // This is a bit of a hack but necessary because when ImHex gets closed, all plugins are unloaded in order for
        // destructors to be called correctly. To prevent crashes when ImHex exits, we need to delete all shared data

        EventManager::clear();

        while (ImHexApi::Provider::isValid())
            ImHexApi::Provider::remove(ImHexApi::Provider::get());
        ContentRegistry::Provider::impl::getEntries().clear();

        ImHexApi::System::getInitArguments().clear();
        ImHexApi::HexEditor::impl::getBackgroundHighlights().clear();
        ImHexApi::HexEditor::impl::getForegroundHighlights().clear();
        ImHexApi::HexEditor::impl::getBackgroundHighlightingFunctions().clear();
        ImHexApi::HexEditor::impl::getForegroundHighlightingFunctions().clear();
        ImHexApi::HexEditor::impl::getTooltips().clear();
        ImHexApi::HexEditor::impl::getTooltipFunctions().clear();

        ContentRegistry::Settings::impl::getEntries().clear();
        ContentRegistry::Settings::impl::getSettingsData().clear();

        ContentRegistry::CommandPaletteCommands::impl::getEntries().clear();
        ContentRegistry::CommandPaletteCommands::impl::getHandlers().clear();

        ContentRegistry::PatternLanguage::impl::getFunctions().clear();
        ContentRegistry::PatternLanguage::impl::getPragmas().clear();
        ContentRegistry::PatternLanguage::impl::getVisualizers().clear();

        ContentRegistry::Views::impl::getEntries().clear();
        impl::PopupBase::getOpenPopups().clear();


        ContentRegistry::Tools::impl::getEntries().clear();
        ContentRegistry::DataInspector::impl::getEntries().clear();

        ContentRegistry::Language::impl::getLanguages().clear();
        ContentRegistry::Language::impl::getLanguageDefinitions().clear();
        LangEntry::resetLanguageStrings();

        ContentRegistry::Interface::impl::getWelcomeScreenEntries().clear();
        ContentRegistry::Interface::impl::getFooterItems().clear();
        ContentRegistry::Interface::impl::getToolbarItems().clear();
        ContentRegistry::Interface::impl::getMainMenuItems().clear();
        ContentRegistry::Interface::impl::getMenuItems().clear();
        ContentRegistry::Interface::impl::getSidebarItems().clear();
        ContentRegistry::Interface::impl::getTitleBarButtons().clear();
        ContentRegistry::Interface::impl::getLayouts().clear();

        ShortcutManager::clearShortcuts();

        TaskManager::getRunningTasks().clear();

        ContentRegistry::DataProcessorNode::impl::getEntries().clear();

        ContentRegistry::DataFormatter::impl::getEntries().clear();
        ContentRegistry::FileHandler::impl::getEntries().clear();
        ContentRegistry::Hashes::impl::getHashes().clear();

        ThemeManager::reset();

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
        // Load plugins
        for (const auto &dir : fs::getDefaultPaths(fs::ImHexPath::Plugins)) {
            PluginManager::load(dir);
        }

        // Get loaded plugins
        auto &plugins = PluginManager::getPlugins();

        // If no plugins were loaded, ImHex wasn't installed properly. This will trigger an error popup later on
        if (plugins.empty()) {
            log::error("No plugins found!");

            ImHexApi::System::impl::addInitArgument("no-plugins");
            return false;
        }

        const auto shouldLoadPlugin = [executablePath = wolv::io::fs::getExecutablePath()](const Plugin &plugin) {
            // In debug builds, ignore all plugins that are not part of the executable directory
            #if !defined(DEBUG)
                return true;
            #endif

            if (!executablePath.has_value())
                return true;

            // Check if the plugin is somewhere in the same directory tree as the executable
            return !std::fs::relative(plugin.getPath(), executablePath->parent_path()).string().starts_with("..");
        };

        u32 builtinPlugins = 0;
        u32 loadErrors     = 0;

        // Load the builtin plugin first, so it can initialize everything that's necessary for ImHex to work
        for (const auto &plugin : plugins) {
            if (!plugin.isBuiltinPlugin()) continue;

            if (!shouldLoadPlugin(plugin)) {
                log::debug("Skipping built-in plugin {}", plugin.getPath().string());
                continue;
            }

            // Make sure there's only one built-in plugin
            builtinPlugins++;
            if (builtinPlugins > 1) continue;

            // Initialize the plugin
            if (!plugin.initializePlugin()) {
                log::error("Failed to initialize plugin {}", wolv::util::toUTF8String(plugin.getPath().filename()));
                loadErrors++;
            }
        }

        // Load all other plugins
        for (const auto &plugin : plugins) {
            if (plugin.isBuiltinPlugin()) continue;

            if (!shouldLoadPlugin(plugin)) {
                log::debug("Skipping plugin {}", plugin.getPath().string());
                continue;
            }

            // Initialize the plugin
            if (!plugin.initializePlugin()) {
                log::error("Failed to initialize plugin {}", wolv::util::toUTF8String(plugin.getPath().filename()));
                loadErrors++;
            }
        }

        // If no plugins were loaded successfully, ImHex wasn't installed properly. This will trigger an error popup later on
        if (loadErrors == plugins.size()) {
            log::error("No plugins loaded successfully!");
            ImHexApi::System::impl::addInitArgument("no-plugins");
            return false;
        }

        // ImHex requires exactly one built-in plugin
        // If no built-in plugin or more than one was found, something's wrong and we can't continue
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
            // Try to load settings from file
            ContentRegistry::Settings::impl::load();
        } catch (std::exception &e) {
            // If that fails, create a new settings file

            log::error("Failed to load configuration! {}", e.what());

            ContentRegistry::Settings::impl::clear();
            ContentRegistry::Settings::impl::store();

            return false;
        }

        return true;
    }

    bool storeSettings() {
        try {
            ContentRegistry::Settings::impl::store();
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
            #if defined(OS_LINUX)
            { "Migrate config to .config", migrateConfig,     false },
            #endif
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