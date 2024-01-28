#include <hex/api/imhex_api.hpp>
#include <hex/api/content_registry.hpp>
#include <hex/api_urls.hpp>

#include <hex/api/task_manager.hpp>
#include <hex/helpers/http_requests.hpp>
#include <hex/helpers/fs.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/helpers/logger.hpp>

#include <wolv/utils/string.hpp>
#include <wolv/hash/uuid.hpp>

#include <imgui.h>
#include <imgui_freetype.h>

#include <cstring>

namespace hex::plugin::builtin {

    namespace {

        using namespace std::literals::string_literals;

        bool checkForUpdatesSync() {
            int checkForUpdates = ContentRegistry::Settings::read("hex.builtin.setting.general", "hex.builtin.setting.general.server_contact", 2);

            // Check if we should check for updates
            if (checkForUpdates == 1) {
                HttpRequest request("GET", GitHubApiURL + "/releases/latest"s);

                // Query the GitHub API for the latest release version
                auto response = request.execute().get();
                if (response.getStatusCode() != 200)
                    return false;

                nlohmann::json releases;
                try {
                    releases = nlohmann::json::parse(response.getData());
                } catch (const std::exception &) {
                    return false;
                }

                // Check if the response is valid
                if (!releases.contains("tag_name") || !releases["tag_name"].is_string())
                    return false;

                // Convert the current version string to a format that can be compared to the latest release
                auto versionString = ImHexApi::System::getImHexVersion();
                size_t versionLength = std::min(versionString.find_first_of('-'), versionString.length());
                auto currVersion   = "v" + versionString.substr(0, versionLength);

                // Get the latest release version string
                auto latestVersion = releases["tag_name"].get<std::string_view>();

                // Check if the latest release is different from the current version
                if (latestVersion != currVersion)
                    ImHexApi::System::impl::addInitArgument("update-available", latestVersion.data());

                // Check if there is a telemetry uuid
                auto uuid = ContentRegistry::Settings::read("hex.builtin.setting.general", "hex.builtin.setting.general.uuid", "").get<std::string>();
                if (uuid.empty()) {
                    // Generate a new uuid
                    uuid = wolv::hash::generateUUID();
                    // Save
                    ContentRegistry::Settings::write("hex.builtin.setting.general", "hex.builtin.setting.general.uuid", uuid);
                }

                TaskManager::createBackgroundTask("Sending statistics...", [uuid, versionString](auto&) {
                    // To avoid potentially flooding our database with lots of dead users
                    // from people just visiting the website, don't send telemetry data from
                    // the web version
                    #if defined(OS_WEB)
                        return;
                    #endif

                    // Make telemetry request
                    nlohmann::json telemetry = {
                            { "uuid", uuid },
                            { "format_version", "1" },
                            { "imhex_version", versionString },
                            { "imhex_commit", fmt::format("{}@{}", ImHexApi::System::getCommitHash(true), ImHexApi::System::getCommitBranch()) },
                            { "install_type", ImHexApi::System::isPortableVersion() ? "Portable" : "Installed" },
                            { "os", ImHexApi::System::getOSName() },
                            { "os_version", ImHexApi::System::getOSVersion() },
                            { "arch", ImHexApi::System::getArchitecture() },
                            { "gpu_vendor", ImHexApi::System::getGPUVendor() }
                    };

                    HttpRequest telemetryRequest("POST", ImHexApiURL + "/telemetry"s);
                    telemetryRequest.setTimeout(500);

                    telemetryRequest.setBody(telemetry.dump());
                    telemetryRequest.addHeader("Content-Type", "application/json");

                    // Execute request
                    telemetryRequest.execute();
                });
            }
            return true;
        }

        bool checkForUpdates() {
            TaskManager::createBackgroundTask("Checking for updates", [](auto&) { checkForUpdatesSync(); });
            return true;
        }

        bool configureUIScale() {
            int interfaceScaleSetting = int(ContentRegistry::Settings::read("hex.builtin.setting.interface", "hex.builtin.setting.interface.scaling_factor", 1.0F).get<float>() * 10.0F);

            float interfaceScaling;
            if (interfaceScaleSetting == 0)
                interfaceScaling = ImHexApi::System::getNativeScale();
            else
                interfaceScaling = interfaceScaleSetting / 10.0F;

            ImHexApi::System::impl::setGlobalScale(interfaceScaling);

            return true;
        }

        bool loadFontsImpl(bool loadUnicode) {
            float defaultFontSize = ImHexApi::Fonts::DefaultFontSize * ImHexApi::System::getGlobalScale();

            if (defaultFontSize == 0.0F)
                defaultFontSize = ImHexApi::Fonts::DefaultFontSize;

            // Reset used font size back to the default size
            ImHexApi::Fonts::impl::setFontSize(defaultFontSize);

            // Load custom font related settings
            if (ContentRegistry::Settings::read("hex.builtin.setting.font", "hex.builtin.setting.font.custom_font_enable", false).get<bool>()) {
                std::fs::path fontFile = ContentRegistry::Settings::read("hex.builtin.setting.font", "hex.builtin.setting.font.font_path", "").get<std::string>();
                if (!fontFile.empty()) {
                    if (!wolv::io::fs::exists(fontFile) || !wolv::io::fs::isRegularFile(fontFile)) {
                        log::warn("Custom font file {} not found! Falling back to default font.", wolv::util::toUTF8String(fontFile));
                        fontFile.clear();
                    }

                    log::info("Loading custom font from {}", wolv::util::toUTF8String(fontFile));
                }

                // If no custom font has been specified, search for a file called "font.ttf" in one of the resource folders
                if (fontFile.empty()) {
                    for (const auto &dir : fs::getDefaultPaths(fs::ImHexPath::Resources)) {
                        auto path = dir / "font.ttf";
                        if (wolv::io::fs::exists(path)) {
                            log::info("Loading custom font from {}", wolv::util::toUTF8String(path));

                            fontFile = path;
                            break;
                        }
                    }
                }

                ImHexApi::Fonts::impl::setCustomFontPath(fontFile);

                // If a custom font has been loaded now, also load the font size
                float fontSize = defaultFontSize;
                if (!fontFile.empty()) {
                    fontSize = float(ContentRegistry::Settings::read("hex.builtin.setting.font", "hex.builtin.setting.font.font_size", 13).get<int>()) * ImHexApi::System::getGlobalScale();
                }

                ImHexApi::Fonts::impl::setFontSize(fontSize);
            }

            float fontSize = ImHexApi::Fonts::getFontSize();

            const auto &fontFile = ImHexApi::Fonts::getCustomFontPath();

            // Setup basic font configuration
            auto fonts       = IM_NEW(ImFontAtlas)();
            ImFontConfig cfg = {};
            cfg.OversampleH = cfg.OversampleV = 1, cfg.PixelSnapH = true;
            cfg.SizePixels = fontSize;

            fonts->Flags |= ImFontAtlasFlags_NoPowerOfTwoHeight;
            fonts->TexDesiredWidth = 4096;

            // Configure font glyph ranges that should be loaded from the default font and unifont
            static ImVector<ImWchar> defaultGlyphRanges;
            defaultGlyphRanges = { };
            {
                ImFontGlyphRangesBuilder glyphRangesBuilder;

                {
                    constexpr static std::array<ImWchar, 3> controlCodeRange   = { 0x0001, 0x001F, 0 };
                    constexpr static std::array<ImWchar, 3> extendedAsciiRange = { 0x007F, 0x00FF, 0 };

                    glyphRangesBuilder.AddRanges(controlCodeRange.data());
                    glyphRangesBuilder.AddRanges(fonts->GetGlyphRangesDefault());
                    glyphRangesBuilder.AddRanges(extendedAsciiRange.data());
                }

                if (loadUnicode) {
                    constexpr static std::array<ImWchar, 3> fullRange = { 0x0100, 0xFFEF, 0 };

                    glyphRangesBuilder.AddRanges(fullRange.data());
                } else {
                    glyphRangesBuilder.AddRanges(fonts->GetGlyphRangesJapanese());
                    glyphRangesBuilder.AddRanges(fonts->GetGlyphRangesChineseFull());
                    glyphRangesBuilder.AddRanges(fonts->GetGlyphRangesCyrillic());
                    glyphRangesBuilder.AddRanges(fonts->GetGlyphRangesKorean());
                    glyphRangesBuilder.AddRanges(fonts->GetGlyphRangesThai());
                    glyphRangesBuilder.AddRanges(fonts->GetGlyphRangesVietnamese());
                }

                glyphRangesBuilder.BuildRanges(&defaultGlyphRanges);
            }

            if (fontFile.empty())
                fonts->Clear();

            if (ContentRegistry::Settings::read("hex.builtin.setting.font", "hex.builtin.setting.font.custom_font_enable", false).get<bool>()) {
                if (ContentRegistry::Settings::read("hex.builtin.setting.font", "hex.builtin.setting.font.font_bold", false))
                    cfg.FontBuilderFlags |= ImGuiFreeTypeBuilderFlags_Bold;
                if (ContentRegistry::Settings::read("hex.builtin.setting.font", "hex.builtin.setting.font.font_italic", false))
                    cfg.FontBuilderFlags |= ImGuiFreeTypeBuilderFlags_Oblique;
                if (!ContentRegistry::Settings::read("hex.builtin.setting.font", "hex.builtin.setting.font.font_antialias", true))
                    cfg.FontBuilderFlags |= ImGuiFreeTypeBuilderFlags_Monochrome | ImGuiFreeTypeBuilderFlags_MonoHinting;
            }

            auto loadDefaultFont = [&](const char *fontName, u32 flags = 0) {
                ImFontConfig defaultConfig = cfg;
                ImFont *defaultFont;

                defaultConfig.FontBuilderFlags |= flags;

                std::strncpy(defaultConfig.Name, fontName, sizeof(defaultConfig.Name) - 1);

                if (fontFile.empty()) {
                    defaultConfig.FontBuilderFlags |= ImGuiFreeTypeBuilderFlags_Monochrome | ImGuiFreeTypeBuilderFlags_MonoHinting;
                    defaultConfig.SizePixels = std::floor(ImHexApi::Fonts::getFontSize() / ImHexApi::Fonts::DefaultFontSize) * ImHexApi::Fonts::DefaultFontSize;
                    defaultFont = fonts->AddFontDefault(&defaultConfig);
                } else {
                    defaultFont = fonts->AddFontFromFileTTF(wolv::util::toUTF8String(fontFile).c_str(), 0, &defaultConfig, defaultGlyphRanges.Data);
                }

                return defaultFont;
            };

            // Load main font
            // If a custom font has been specified, load it, otherwise load the default ImGui font
            ImFont *defaultFont = loadDefaultFont("Default Font");
            if (defaultFont == nullptr) {
                log::warn("Failed to load custom font! Falling back to default font.");

                ImHexApi::Fonts::impl::setFontSize(defaultFontSize);
                cfg.SizePixels = defaultFontSize;
                defaultFont = fonts->AddFontDefault(&cfg);
            }

            fonts->Build();

            cfg.FontDataOwnedByAtlas = false;

            // Add all other fonts to the atlas
            auto startFlags = cfg.FontBuilderFlags;
            std::list<ImVector<ImWchar>> ranges;
            for (auto &font : ImHexApi::Fonts::impl::getFonts()) {
                ImVector<ImWchar> fontRange;
                if (font.glyphRanges.empty()) {
                    fontRange = defaultGlyphRanges;
                } else {
                    for (const auto &range : font.glyphRanges) {
                        fontRange.push_back(range.begin);
                        fontRange.push_back(range.end);
                    }
                    fontRange.push_back(0x00);
                }

                ranges.push_back(fontRange);

                cfg.FontBuilderFlags = font.flags;

                float descent = [&] {
                    ImFontAtlas atlas;

                    // Disable merge mode for this font but retain the rest of the configuration
                    cfg.MergeMode = false;
                    ON_SCOPE_EXIT { cfg.MergeMode = true; };

                    // Construct a range that only contains the first glyph of the font
                    ImVector<ImWchar> queryRange;
                    {
                        auto firstGlyph = font.glyphRanges.empty() ? defaultGlyphRanges.front() : font.glyphRanges.front().begin;
                        queryRange.push_back(firstGlyph);
                        queryRange.push_back(firstGlyph);
                    }
                    queryRange.push_back(0x00);

                    // Build the font atlas with the query range
                    auto newFont = atlas.AddFontFromMemoryTTF(font.fontData.data(), int(font.fontData.size()), 0, &cfg, queryRange.Data);
                    atlas.Build();

                    return newFont->Descent;
                }();


                std::memset(cfg.Name, 0x00, sizeof(cfg.Name));
                std::strncpy(cfg.Name, font.name.c_str(), sizeof(cfg.Name) - 1);
                cfg.GlyphOffset = { font.offset.x, font.offset.y - defaultFont->Descent + descent };
                fonts->AddFontFromMemoryTTF(font.fontData.data(), int(font.fontData.size()), 0, &cfg, ranges.back().Data);
            }
            cfg.FontBuilderFlags = startFlags;

            // Create bold and italic font
            cfg.MergeMode = false;
            ImFont *boldFont    = loadDefaultFont("Bold Font", ImGuiFreeTypeBuilderFlags_Bold);
            ImFont *italicFont  = loadDefaultFont("Italic Font", ImGuiFreeTypeBuilderFlags_Oblique);
            ImHexApi::Fonts::impl::setFonts(boldFont, italicFont);

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
                    ContentRegistry::Settings::write("hex.builtin.setting.font", "hex.builtin.setting.font.load_all_unicode_chars", false);

                    // Try to load the font atlas again
                    return loadFontsImpl(false);
                } else {
                    log::error("Failed to build font atlas! Check your Graphics driver!");
                    return false;
                }
            }

            // Configure ImGui to use the font atlas
            ImHexApi::Fonts::impl::setFontAtlas(fonts);

            return true;
        }

        bool loadFonts() {
            // Check if unicode support is enabled in the settings and that the user doesn't use the No GPU version on Windows
            // The Mesa3D software renderer on Windows identifies itself as "VMware, Inc."
            bool shouldLoadUnicode =
                    ContentRegistry::Settings::read("hex.builtin.setting.font", "hex.builtin.setting.font.load_all_unicode_chars", false) &&
                    ImHexApi::System::getGPUVendor() != "VMware, Inc.";

            return loadFontsImpl(shouldLoadUnicode);
        }

        bool loadWindowSettings() {
            bool multiWindowEnabled = ContentRegistry::Settings::read("hex.builtin.setting.interface", "hex.builtin.setting.interface.multi_windows", false);
            ImHexApi::System::impl::setMultiWindowMode(multiWindowEnabled);

            bool restoreWindowPos = ContentRegistry::Settings::read("hex.builtin.setting.interface", "hex.builtin.setting.interface.restore_window_pos", false);

            if (restoreWindowPos) {
                ImHexApi::System::InitialWindowProperties properties = {};

                properties.maximized = ContentRegistry::Settings::read("hex.builtin.setting.interface", "hex.builtin.setting.interface.window.maximized", 0).get<int>();
                properties.x       = ContentRegistry::Settings::read("hex.builtin.setting.interface", "hex.builtin.setting.interface.window.x", 0);
                properties.y       = ContentRegistry::Settings::read("hex.builtin.setting.interface", "hex.builtin.setting.interface.window.y", 0);
                properties.width   = ContentRegistry::Settings::read("hex.builtin.setting.interface", "hex.builtin.setting.interface.window.width", 0);
                properties.height  = ContentRegistry::Settings::read("hex.builtin.setting.interface", "hex.builtin.setting.interface.window.height", 0);

                ImHexApi::System::impl::setInitialWindowProperties(properties);
            }

            return true;
        }

    }

    void addInitTasks() {
        ImHexApi::System::addStartupTask("Load Window Settings", false, loadWindowSettings);
        ImHexApi::System::addStartupTask("Configuring UI scale", true, configureUIScale);
        ImHexApi::System::addStartupTask("Loading fonts", true, loadFonts);
        ImHexApi::System::addStartupTask("Checking for updates", true, checkForUpdates);
    }
}