#include "init/tasks.hpp"

#include <imgui.h>
#include <imgui_freetype.h>

#include <hex/helpers/net.hpp>
#include <hex/api/content_registry.hpp>
#include <hex/pattern_language/pattern_data.hpp>

#include <fontawesome_font.h>
#include <codicons_font.h>
#include <unifont_font.h>

#include "views/view_hexeditor.hpp"
#include "views/view_pattern_editor.hpp"
#include "views/view_pattern_data.hpp"
#include "views/view_hashes.hpp"
#include "views/view_information.hpp"
#include "views/view_help.hpp"
#include "views/view_tools.hpp"
#include "views/view_strings.hpp"
#include "views/view_data_inspector.hpp"
#include "views/view_disassembler.hpp"
#include "views/view_bookmarks.hpp"
#include "views/view_patches.hpp"
#include "views/view_command_palette.hpp"
#include "views/view_settings.hpp"
#include "views/view_data_processor.hpp"
#include "views/view_yara.hpp"
#include "views/view_constants.hpp"
#include "views/view_store.hpp"
#include "views/view_diff.hpp"

#include "helpers/plugin_manager.hpp"

#include <filesystem>

#include <nlohmann/json.hpp>

namespace hex::init {

    using namespace std::literals::string_literals;

    static bool checkForUpdates() {
        hex::Net net;

        auto releases = net.getJson(GitHubApiURL + "/releases/latest"s).get();
        if (releases.code != 200)
            return false;

        if (!releases.body.contains("tag_name") || !releases.body["tag_name"].is_string())
            return false;

        auto versionString = std::string(IMHEX_VERSION);
        auto currVersion = "v" + versionString.substr(0, versionString.find_first_of('-'));
        auto latestVersion = releases.body["tag_name"].get<std::string_view>();

        if (latestVersion != currVersion)
            getInitArguments().push_back({ "update-available", latestVersion.data() });

        return true;
    }

    static bool downloadInformation() {
        hex::Net net;

        auto tip = net.getString(ImHexApiURL + "/tip"s).get();
        if (tip.code != 200)
            return false;

        getInitArguments().push_back({ "tip-of-the-day", tip.body });

        return true;
    }

    bool createDirectories() {
        bool result = true;

        std::array paths = {
            ImHexPath::Patterns,
            ImHexPath::PatternsInclude,
            ImHexPath::Magic,
            ImHexPath::Plugins,
            ImHexPath::Resources,
            ImHexPath::Config,
            ImHexPath::Constants,
            ImHexPath::Yara,
            ImHexPath::Python
        };

        for (auto path : paths) {
            for (auto &folder : hex::getPath(path)) {
                try {
                    std::filesystem::create_directories(folder);
                } catch (...) {
                    result = false;
                }
            }
        }

        if (!result)
            getInitArguments().push_back({ "folder-creation-error", { } });

        return result;
    }

    bool loadFonts() {
        auto &fonts = SharedData::fontAtlas;
        auto &cfg = SharedData::fontConfig;

        fonts = IM_NEW(ImFontAtlas)();
        cfg = { };

        std::string fontFile;
        for (const auto &dir : hex::getPath(ImHexPath::Resources)) {
            auto path = dir + "/font.ttf";
            if (std::filesystem::exists(path)) {
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
                ICON_MIN_FA, ICON_MAX_FA,
                0
        };

        ImWchar codiconsRange[] = {
                ICON_MIN_VS, ICON_MAX_VS,
                0
        };

        ImWchar unifontRange[] = {
                0x0020, 0xFFF0,
                0
        };


        if (fontFile.empty()) {
            // Load default font

            fonts->Clear();

            cfg.OversampleH = cfg.OversampleV = 1, cfg.PixelSnapH = true;
            cfg.SizePixels = 13.0f * SharedData::fontScale;
            fonts->AddFontDefault(&cfg);
        } else {
            // Load custom font

            auto fontSize = ContentRegistry::Settings::read("hex.builtin.setting.interface", "hex.builtin.setting.interface.font_size", 14);

            cfg.OversampleH = cfg.OversampleV = 1, cfg.PixelSnapH = true;
            cfg.SizePixels = fontSize * SharedData::fontScale;

            fonts->AddFontFromFileTTF(fontFile.c_str(), std::floor(fontSize * SharedData::fontScale), &cfg, ranges.Data); // Needs conversion to char for Windows
        }

        cfg.MergeMode = true;

        fonts->AddFontFromMemoryCompressedTTF(font_awesome_compressed_data, font_awesome_compressed_size, 13.0f * SharedData::fontScale, &cfg, fontAwesomeRange);
        fonts->AddFontFromMemoryCompressedTTF(codicons_compressed_data, codicons_compressed_size, 13.0f * SharedData::fontScale, &cfg, codiconsRange);
        fonts->AddFontFromMemoryCompressedTTF(unifont_compressed_data, unifont_compressed_size, 13.0f * SharedData::fontScale, &cfg, unifontRange);

        ImGuiFreeType::BuildFontAtlas(fonts);

        return true;
    }

    bool loadDefaultViews() {

        ContentRegistry::Views::add<ViewHexEditor>();
        ContentRegistry::Views::add<ViewPatternEditor>();
        ContentRegistry::Views::add<ViewPatternData>();
        ContentRegistry::Views::add<ViewDataInspector>();
        ContentRegistry::Views::add<ViewHashes>();
        ContentRegistry::Views::add<ViewInformation>();
        ContentRegistry::Views::add<ViewStrings>();
        ContentRegistry::Views::add<ViewDisassembler>();
        ContentRegistry::Views::add<ViewBookmarks>();
        ContentRegistry::Views::add<ViewPatches>();
        ContentRegistry::Views::add<ViewTools>();
        ContentRegistry::Views::add<ViewCommandPalette>();
        ContentRegistry::Views::add<ViewHelp>();
        ContentRegistry::Views::add<ViewSettings>();
        ContentRegistry::Views::add<ViewDataProcessor>();
        ContentRegistry::Views::add<ViewYara>();
        ContentRegistry::Views::add<ViewConstants>();
        ContentRegistry::Views::add<ViewStore>();
        ContentRegistry::Views::add<ViewDiff>();

        return true;
    }

    bool deleteSharedData() {
        SharedData::deferredCalls.clear();

        while (ImHexApi::Provider::isValid())
            ImHexApi::Provider::remove(ImHexApi::Provider::get());

        SharedData::settingsEntries.clear();
        SharedData::settingsJson.clear();

        SharedData::commandPaletteCommands.clear();
        SharedData::patternLanguageFunctions.clear();

        for (auto &view : SharedData::views)
            delete view;
        SharedData::views.clear();

        SharedData::toolsEntries.clear();

        SharedData::dataInspectorEntries.clear();

        SharedData::bookmarkEntries.clear();

        for (auto &pattern : SharedData::patternData)
            delete pattern;
        SharedData::patternData.clear();

        SharedData::languageNames.clear();
        SharedData::languageDefinitions.clear();
        SharedData::loadedLanguageStrings.clear();

        SharedData::welcomeScreenEntries.clear();
        SharedData::footerItems.clear();

        SharedData::dataProcessorNodes.clear();

        SharedData::recentFilePaths.clear();

        SharedData::clearVariables();

        return true;
    }

    bool loadPlugins() {
        for (const auto &dir : hex::getPath(ImHexPath::Plugins)) {
            PluginManager::load(dir);
        }

        if (PluginManager::getPlugins().empty()) {
            getInitArguments().push_back({ "no-plugins", { } });
            return false;
        }

        for (const auto &plugin : PluginManager::getPlugins()) {
            plugin.initializePlugin();
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
        } catch (...) {
            return false;
        }

        switch (ContentRegistry::Settings::read("hex.builtin.setting.interface", "hex.builtin.setting.interface.scaling", 0)) {
            default:
            case 0:
                // Native scaling
                break;
            case 1:
                SharedData::globalScale = SharedData::fontScale = 0.5F;
                break;
            case 2:
                SharedData::globalScale = SharedData::fontScale = 1.0F;
                break;
            case 3:
                SharedData::globalScale = SharedData::fontScale = 1.5F;
                break;
            case 4:
                SharedData::globalScale = SharedData::fontScale = 2.0F;
                break;
        }

        return true;
    }

    bool storeSettings() {
        try {
            ContentRegistry::Settings::store();
        } catch (...) {
            return false;
        }
        return true;
    }

    std::vector<Task> getInitTasks() {
        return {
                { "Checking for updates...",    checkForUpdates     },
                { "Downloading information...", downloadInformation },
                { "Creating directories...",    createDirectories   },
                { "Loading default views...",   loadDefaultViews    },
                { "Loading settings...",        loadSettings        },
                { "Loading plugins...",         loadPlugins         },
                { "Loading fonts...",           loadFonts           },
        };
    }

    std::vector<Task> getExitTasks() {
        return {
                { "Saving settings...",         storeSettings       },
                { "Cleaning up shared data...", deleteSharedData    },
                { "Unloading plugins...",       unloadPlugins       },
        };
    }

    std::vector<Argument>& getInitArguments() {
        static std::vector<Argument> initArguments;

        return initArguments;
    }

}