#include "content/views/view_about.hpp"
#include "hex/ui/popup.hpp"

#include <hex/api_urls.hpp>
#include <hex/api/content_registry/user_interface.hpp>
#include <hex/api/achievement_manager.hpp>
#include <hex/api/plugin_manager.hpp>

#include <hex/helpers/fmt.hpp>
#include <hex/helpers/fs.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/helpers/http_requests.hpp>
#include <hex/helpers/default_paths.hpp>
#include <hex/helpers/menu_items.hpp>

#include <fonts/vscode_icons.hpp>

#include <romfs/romfs.hpp>
#include <wolv/utils/string.hpp>
#include <nlohmann/json.hpp>

#include <string>
#include <ui/markdown.hpp>

namespace hex::plugin::builtin {

    class PopupEE : public Popup<PopupEE> {
    public:
        PopupEE() : Popup("Se" /* Not going to */ "cr" /* make it that easy */ "et") {

        }

        void drawContent() override {
            ImHexApi::System::unlockFrameRate();

            ImGuiIO& io = ImGui::GetIO();
            ImVec2 size = scaled({ 320, 180 });
            ImGui::InvisibleButton("canvas", size);
            ImVec2 p0 = ImGui::GetItemRectMin();
            ImVec2 p1 = ImGui::GetItemRectMax();

            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->PushClipRect(p0, p1);

            ImVec4 mouseData;
            mouseData.x = (io.MousePos.x - p0.x) / size.x;
            mouseData.y = (io.MousePos.y - p0.y) / size.y;
            mouseData.z = io.MouseDownDuration[0];
            mouseData.w = io.MouseDownDuration[1];

            fx(drawList, p0, p1, size, mouseData, float(ImGui::GetTime()));
        }

        void fx(ImDrawList* drawList, ImVec2 startPos, ImVec2 endPos, ImVec2, ImVec4, float t) {
            const float CircleRadius = 5_scaled;
            const float Gap = 1_scaled;

            constexpr static auto func = [](i32 x, i32 y, float t) {
                return std::sin(t - std::sqrt(std::pow((x - 14), 2) + std::pow((y - 8), 2)));
            };

            float x = startPos.x + CircleRadius + Gap;
            u32 ix = 0;
            while (x < endPos.x) {
                float y = startPos.y + CircleRadius + Gap;
                u32 iy = 0;
                while (y < endPos.y) {
                    const float result = func(ix, iy, t);
                    const float radius = CircleRadius * std::abs(result);
                    const auto  color  = result < 0 ? ImColor(0xFF, 0, 0, 0xFF) : ImColor(0xFF, 0xFF, 0xFF, 0xFF);
                    drawList->AddCircleFilled(ImVec2(x, y), radius, color);

                    y  += CircleRadius * 2 + Gap;
                    iy += 1;
                }

                x  += CircleRadius * 2 + Gap;
                ix += 1;
            }
        }
    };

    ViewAbout::ViewAbout() : View::Modal("hex.builtin.view.help.about.name", ICON_VS_HEART) {
        // Add "About" menu item to the help menu
        ContentRegistry::UserInterface::addMenuItem({ "hex.builtin.menu.help", "hex.builtin.view.help.about.name" }, ICON_VS_INFO, 1000, Shortcut::None, [this] {
            this->getWindowOpenState() = true;
        });

        ContentRegistry::UserInterface::addMenuItemSeparator({ "hex.builtin.menu.help" }, 2000);

        ContentRegistry::UserInterface::addMenuItemSubMenu({ "hex.builtin.menu.help" }, 3000, [] {
            if (menu::isNativeMenuBarUsed())
                return;

            static std::string content;
            ImGui::PushStyleVarY(ImGuiStyleVar_FramePadding, 2_scaled);
            if (ImGui::InputTextWithHint("##search", "hex.builtin.view.help.documentation_search"_lang, content, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EscapeClearsAll | ImGuiInputTextFlags_EnterReturnsTrue)) {
                openWebpage(fmt::format("https://docs.werwolv.net/imhex?q={}", content));
                content.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopStyleVar();
        });

        ContentRegistry::UserInterface::addMenuItemSeparator({ "hex.builtin.menu.help" }, 4000);


        // Add documentation link to the help menu
        ContentRegistry::UserInterface::addMenuItem({ "hex.builtin.menu.help", "hex.builtin.view.help.documentation" }, ICON_VS_BOOK, 5000, Shortcut::None, [] {
            hex::openWebpage("https://docs.werwolv.net/imhex");
            AchievementManager::unlockAchievement("hex.builtin.achievement.starting_out", "hex.builtin.achievement.starting_out.docs.name");
        });
    }


    void ViewAbout::drawAboutMainPage() {
        // Draw main about table
        if (ImGui::BeginTable("about_table", 2, ImGuiTableFlags_SizingFixedFit)) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            // Draw the ImHex icon
            if (!m_logoTexture.isValid())
                m_logoTexture = ImGuiExt::Texture::fromSVG(romfs::get("assets/common/logo.svg").span(), 160_scaled, 160_scaled, ImGuiExt::Texture::Filter::Linear);

            ImGui::Image(m_logoTexture, m_logoTexture.getSize());
            if (ImGui::IsItemClicked()) {
                m_clickCount += 1;
            }

            if (m_clickCount >= (2 * 3 + 4)) {
                this->getWindowOpenState() = false;
                PopupEE::open();
                m_clickCount = 0;
            }

            ImGui::TableNextColumn();

            if (ImGuiExt::BeginSubWindow("Build Information", nullptr, ImVec2(450_scaled, 0), ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY)) {
                this->drawBuildInformation();
            }
            ImGuiExt::EndSubWindow();

            ImGui::EndTable();
        }

        ImGui::NewLine();

        if (ImGui::BeginTable("##box", 1, ImGuiTableFlags_BordersOuter | ImGuiTableFlags_SizingStretchSame, ImGui::GetContentRegionAvail())) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            // Draw donation links
            if (ImGui::BeginChild("##ThanksWrapper", ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetTextLineHeightWithSpacing() * 3))) {
                ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x * 0.8F);
                ImGuiExt::TextFormattedCentered("{}", static_cast<const char *>("hex.builtin.view.help.about.thanks"_lang));
                ImGui::PopTextWrapPos();
            }
            ImGui::EndChild();

            struct DonationPage {
                DonationPage(const std::fs::path &path, std::string link) :
                    texture(ImGuiExt::Texture::fromImage(romfs::get(path).span<std::byte>(), ImGuiExt::Texture::Filter::Linear)),
                    link(std::move(link)) { }

                AutoReset<ImGuiExt::Texture> texture;
                std::string link;
            };

            static std::array DonationPages = {
                DonationPage("assets/common/donation/paypal.png", "https://werwolv.net/donate"),
                DonationPage("assets/common/donation/github.png", "https://github.com/sponsors/WerWolv"),
                DonationPage("assets/common/donation/patreon.png", "https://patreon.com/werwolv")
            };

            if (ImGui::BeginTable("DonationLinks", 5, ImGuiTableFlags_SizingStretchSame)) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                for (const auto &page : DonationPages) {
                    ImGui::TableNextColumn();

                    const auto size = (page.texture->getSize() * 1_scaled) / 1.5F;
                    const auto startPos = ImGui::GetCursorScreenPos();
                    ImGui::Image(*page.texture, size);

                    if (ImGui::IsItemHovered()) {
                        ImGui::GetForegroundDrawList()->AddShadowCircle(startPos + size / 2, size.x / 2, ImGui::GetColorU32(ImGuiCol_Button), 100.0F, ImVec2(), ImDrawFlags_ShadowCutOutShapeBackground);
                    }

                    if (ImGui::IsItemClicked()) {
                        hex::openWebpage(page.link);
                    }
                }

                ImGui::EndTable();
            }

            ImGui::NewLine();
            ImGui::EndTable();
        }
    }

    void ViewAbout::drawBuildInformation() {
        if (ImGui::BeginTable("Information", 1, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInner)) {
            ImGui::Indent(5_scaled);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            {
                // Draw basic information about ImHex and its version
                ImGuiExt::TextFormattedSelectable("ImHex Hex Editor v{} by WerWolv", ImHexApi::System::getImHexVersion().get());
                ImGui::Indent(25_scaled);
                ImGuiExt::TextFormattedSelectable("Powered by Dear ImGui v{}", ImGui::GetVersion());
                ImGui::Unindent(25_scaled);
            }

            ImGui::TableNextColumn();
            {
                ImGuiExt::TextFormatted(" {} ", ICON_VS_SOURCE_CONTROL);

                ImGui::SameLine(0, 0);

                // Draw a clickable link to the current commit
                if (ImGuiExt::Hyperlink(fmt::format("{0}@{1}", ImHexApi::System::getCommitBranch(), ImHexApi::System::getCommitHash()).c_str()))
                    hex::openWebpage("https://github.com/WerWolv/ImHex/commit/" + ImHexApi::System::getCommitHash(true));
            }

            ImGui::TableNextColumn();
            {
                // Draw the build date and time
                ImGuiExt::TextFormattedSelectable("Compiled on {} at {}", __DATE__, __TIME__);
            }

            ImGui::TableNextColumn();
            {
                // Draw the author of the current translation
                ImGui::TextUnformatted("hex.builtin.view.help.about.translator"_lang);
            }

            ImGui::TableNextColumn();
            {
                // Draw information about the open-source nature of ImHex
                ImGui::TextUnformatted("hex.builtin.view.help.about.source"_lang);

                ImGui::SameLine();

                // Draw a clickable link to the GitHub repository
                if (ImGuiExt::Hyperlink(ICON_VS_GITHUB " " "WerWolv/ImHex"))
                    hex::openWebpage("https://github.com/WerWolv/ImHex");
            }

            ImGui::Unindent();

            ImGui::EndTable();
        }
    }

    static void drawContributorTable(const char *title, const auto &contributors) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2());
        auto result = ImGuiExt::BeginSubWindow(title, nullptr, ImVec2(ImGui::GetContentRegionAvail().x, 0), ImGuiChildFlags_AutoResizeX);
        ImGui::PopStyleVar();
        if (result) {
            if (ImGui::BeginTable(title, 1, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
                for (const auto &contributor : contributors) {
                    ImGui::TableNextRow();
                    if (contributor.mainContributor) {
                        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(ImGuiCol_PlotHistogram) & 0x1FFFFFFF);
                    }
                    ImGui::TableNextColumn();

                    if (ImGuiExt::Hyperlink(contributor.name))
                        hex::openWebpage(contributor.link);

                    if (contributor.description[0] != '\0') {
                        ImGui::Indent();
                        ImGui::TextUnformatted(contributor.description);
                        ImGui::Unindent();
                    }
                }

                ImGui::EndTable();
            }
        }
        ImGuiExt::EndSubWindow();
    }

    void ViewAbout::drawContributorPage() {
        struct Contributor {
            const char *name;
            const char *description;
            const char *link;
            bool mainContributor;
        };

        constexpr static std::array Contributors = {
            Contributor { .name="iTrooz", .description="A huge amount of help maintaining ImHex and the CI", .link="https://github.com/iTrooz", .mainContributor=true },
            Contributor { .name="jumanji144", .description="A ton of help with the Pattern Language, API and usage stats", .link="https://github.com/jumanji144", .mainContributor=true },
            Contributor { .name="AxCut", .description="A ton of great pattern language improvements and help with the issue tracker", .link="https://github.com/paxcut", .mainContributor=true },
            Contributor { .name="Mary", .description="Porting ImHex to macOS originally", .link="https://github.com/marysaka", .mainContributor=false },
            Contributor { .name="Roblabla", .description="Adding the MSI Windows installer", .link="https://github.com/roblabla", .mainContributor=false },
            Contributor { .name="jam1garner", .description="Adding support for Rust plugins", .link="https://github.com/jam1garner", .mainContributor=false },
            Contributor { .name="All other amazing contributors", .description="Being part of the community, opening issues, PRs and donating", .link="https://github.com/WerWolv/ImHex/graphs/contributors", .mainContributor=false }
        };

        constexpr static std::array Testers = {
            Contributor { .name="Nemoumbra", .description="Breaking my code literal seconds after I push it", .link="https://github.com/Nemoumbra", .mainContributor=true },
            Contributor { .name="Berylskid", .description="", .link="https://github.com/Berylskid", .mainContributor=false },
            Contributor { .name="Jan Polak", .description="", .link="https://github.com/polak-jan", .mainContributor=false },
            Contributor { .name="Ken-Kaneki", .description="", .link="https://github.com/loneicewolf", .mainContributor=false },
            Contributor { .name="Everybody who has reported issues", .description="Helping me find bugs and improve the software", .link="https://github.com/WerWolv/ImHex/issues", .mainContributor=false }

        };

        ImGuiExt::TextFormattedWrapped("These amazing people have contributed some incredible things to ImHex in the past.\nConsider opening a PR on the Git Repository to take your place among them!");
        ImGui::NewLine();
        drawContributorTable("Contributors", Contributors);
        ImGui::NewLine();

        ImGuiExt::TextFormattedWrapped("All of these great people made ImHex work much much smoother.\nConsider joining our Tester team to help making ImHex better for everyone!");
        ImGui::NewLine();
        drawContributorTable("Testers", Testers);
    }

    void ViewAbout::drawLibraryCreditsPage() {
        struct ExternalResource {
            const char *name;
            const char *author;
            const char *link;
        };

        constexpr static std::array ImGuiLibraries = {
            ExternalResource { .name="ImGui", .author="ocornut", .link="https://github.com/ocornut/imgui" },
            ExternalResource { .name="ImPlot", .author="epezent", .link="https://github.com/epezent/implot" },
            ExternalResource { .name="ImPlot3D", .author="brenocq", .link="https://github.com/brenocq/implot3d" },
            ExternalResource { .name="imnodes", .author="Nelarius", .link="https://github.com/Nelarius/imnodes" },
            ExternalResource { .name="ImGuiColorTextEdit", .author="BalazsJako", .link="https://github.com/BalazsJako/ImGuiColorTextEdit" },
        };

        constexpr static std::array ExternalLibraries = {
            ExternalResource { .name="PatternLanguage", .author="WerWolv", .link="https://github.com/WerWolv/PatternLanguage" },
            ExternalResource { .name="libwolv", .author="WerWolv", .link="https://github.com/WerWolv/libwolv" },
            ExternalResource { .name="libromfs", .author="WerWolv", .link="https://github.com/WerWolv/libromfs" },
        };

        constexpr static std::array ThirdPartyLibraries = {
            ExternalResource { .name="json", .author="nlohmann", .link="https://github.com/nlohmann/json" },
            ExternalResource { .name="fmt", .author="fmtlib", .link="https://github.com/fmtlib/fmt" },
            ExternalResource { .name="nativefiledialog-extended", .author="btzy", .link="https://github.com/btzy/nativefiledialog-extended" },
            ExternalResource { .name="xdgpp", .author="danyspin97", .link="https://sr.ht/~danyspin97/xdgpp" },
            ExternalResource { .name="capstone", .author="aquynh", .link="https://github.com/aquynh/capstone" },
            ExternalResource { .name="microtar", .author="rxi", .link="https://github.com/rxi/microtar" },
            ExternalResource { .name="yara", .author="VirusTotal", .link="https://github.com/VirusTotal/yara" },
            ExternalResource { .name="edlib", .author="Martinsos", .link="https://github.com/Martinsos/edlib" },
            ExternalResource { .name="HashLibPlus", .author="ron4fun", .link="https://github.com/ron4fun/HashLibPlus" },
            ExternalResource { .name="miniaudio", .author="mackron", .link="https://github.com/mackron/miniaudio" },
            ExternalResource { .name="freetype", .author="freetype", .link="https://gitlab.freedesktop.org/freetype/freetype" },
            ExternalResource { .name="mbedTLS", .author="ARMmbed", .link="https://github.com/ARMmbed/mbedtls" },
            ExternalResource { .name="curl", .author="curl", .link="https://github.com/curl/curl" },
            ExternalResource { .name="file", .author="file", .link="https://github.com/file/file" },
            ExternalResource { .name="glfw", .author="glfw", .link="https://github.com/glfw/glfw" },
            ExternalResource { .name="llvm", .author="LLVM Maintainers", .link="https://github.com/llvm/llvm-project" },
            ExternalResource { .name="Boost.Regex", .author="John Maddock", .link="https://github.com/boostorg/regex" },
            ExternalResource { .name="md4c", .author="mity", .link="https://github.com/mity/md4c" },
            ExternalResource { .name="lunasvg", .author="sammycage", .link="https://github.com/sammycage/lunasvg" },
            ExternalResource { .name="zlib", .author="madler", .link="https://github.com/madler/zlib" },
            ExternalResource { .name="bzip2", .author="federicomenaquintero", .link="https://gitlab.com/federicomenaquintero/bzip2" },
            ExternalResource { .name="liblzma", .author="tukaani", .link="https://github.com/tukaani-project/xz" },
            ExternalResource { .name="zstd", .author="Facebook", .link="https://github.com/facebook/zstd" },
            ExternalResource { .name="libssh2", .author="libssh2 Maintainers", .link="https://github.com/libssh2/libssh2" },
        };

        constexpr static std::array ThirdPartyResources {
            ExternalResource { .name="VSCode Icons", .author="Microsoft", .link="https://github.com/microsoft/vscode-codicons" },
            ExternalResource { .name="Blender Icons", .author="Blender Maintainers", .link="https://github.com/blender/blender" },
            ExternalResource { .name="Tabler Icons", .author="codecalm", .link="https://github.com/tabler/tabler-icons" },
            ExternalResource { .name="JetBrains Mono", .author="JetBrains", .link="https://github.com/JetBrains/JetBrainsMono" },
            ExternalResource { .name="Unifont", .author="GNU", .link="https://unifoundry.com/unifont" },
        };

        constexpr static auto drawTable = [](const char *category, const auto &libraries) {
            const auto width = ImGui::GetContentRegionAvail().x;
            if (ImGuiExt::BeginSubWindow(category)) {
                for (const auto &library : libraries) {
                    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetColorU32(ImGuiCol_TableHeaderBg));
                    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 50);
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, scaled({ 12, 3 }));

                    if (ImGui::BeginChild(library.link, ImVec2(), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY)) {
                        if (ImGuiExt::Hyperlink(fmt::format("{}/{}", library.author, library.name).c_str())) {
                            hex::openWebpage(library.link);
                        }
                        ImGui::SetItemTooltip("%s", library.link);
                    }
                    ImGui::EndChild();

                    ImGui::SameLine();
                    if (ImGui::GetCursorPosX() > (width - 100_scaled))
                        ImGui::NewLine();

                    ImGui::PopStyleColor();
                    ImGui::PopStyleVar(2);
                }

            }
            ImGuiExt::EndSubWindow();

            ImGui::NewLine();
        };

        ImGuiExt::TextFormattedWrapped("ImHex builds on top of the amazing work of a ton of talented library developers without which this project wouldn't stand.");
        ImGui::NewLine();

        drawTable("ImGui", ImGuiLibraries);
        drawTable("External", ExternalLibraries);
        drawTable("Third Party", ThirdPartyLibraries);
        drawTable("Resources", ThirdPartyResources);
    }

    void ViewAbout::drawLoadedPlugins() {
        const auto &plugins = PluginManager::getPlugins();

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2());
        auto result = ImGuiExt::BeginSubWindow("hex.builtin.view.help.about.plugins"_lang);
        ImGui::PopStyleVar();

        if (result) {
            if (ImGui::BeginTable("plugins", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn("hex.builtin.view.help.about.plugins.plugin"_lang);
                ImGui::TableSetupColumn("hex.builtin.view.help.about.plugins.author"_lang);
                ImGui::TableSetupColumn("hex.builtin.view.help.about.plugins.desc"_lang, ImGuiTableColumnFlags_WidthStretch, 0.5);
                ImGui::TableSetupColumn("##loaded", ImGuiTableColumnFlags_WidthFixed, ImGui::GetTextLineHeight());

                ImGui::TableHeadersRow();

                for (const auto &plugin : plugins) {
                    this->drawPluginRow(plugin);
                }

                ImGui::EndTable();
            }

        }
        ImGuiExt::EndSubWindow();
    }

    void ViewAbout::drawPluginRow(const hex::Plugin& plugin) {
        if (plugin.isLibraryPlugin())
            return;

        auto features = plugin.getFeatures();

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        bool open = false;

        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_Text));
        if (features.empty())
            ImGui::BulletText("%s", plugin.getPluginName().c_str());
        else
            open = ImGui::TreeNode(plugin.getPluginName().c_str());
        ImGui::PopStyleColor();

        ImGui::TableNextColumn();
        ImGui::TextUnformatted(plugin.getPluginAuthor().c_str());
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(plugin.getPluginDescription().c_str());
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(plugin.isInitialized() ? ICON_VS_CHECK : ICON_VS_CLOSE);

        if (open) {
            for (const auto &feature : plugin.getFeatures()) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGuiExt::TextFormatted("  {}", feature.name.c_str());
                ImGui::TableNextColumn();
                ImGui::TableNextColumn();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(feature.enabled ? ICON_VS_CHECK : ICON_VS_CLOSE);

            }

            ImGui::TreePop();
        }
    }


    void ViewAbout::drawPathsPage() {
        constexpr static std::array<std::pair<const char *, const paths::impl::DefaultPath*>, paths::All.size()> PathTypes = {
            {
                { "Patterns",                       &paths::Patterns             },
                { "Patterns Includes",              &paths::PatternsInclude      },
                { "Magic",                          &paths::Magic                },
                { "Plugins",                        &paths::Plugins              },
                { "Yara Patterns",                  &paths::Yara                 },
                { "Yara Advanced Analysis",         &paths::YaraAdvancedAnalysis },
                { "Config",                         &paths::Config               },
                { "Updates",                        &paths::Updates              },
                { "Backups",                        &paths::Backups              },
                { "Resources",                      &paths::Resources            },
                { "Constants lists",                &paths::Constants            },
                { "Custom encodings",               &paths::Encodings            },
                { "Logs",                           &paths::Logs                 },
                { "Recent files",                   &paths::Recent               },
                { "Scripts",                        &paths::Scripts              },
                { "Data inspector scripts",         &paths::Inspectors           },
                { "Themes",                         &paths::Themes               },
                { "Native Libraries",               &paths::Libraries            },
                { "Custom data processor nodes",    &paths::Nodes                },
                { "Layouts",                        &paths::Layouts              },
                { "Workspaces",                     &paths::Workspaces           },
                { "Disassemblers",                  &paths::Disassemblers        },
            }
        };
        static_assert(PathTypes.back().first != nullptr, "All path items need to be populated!");

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2());
        if (ImGuiExt::BeginSubWindow("Paths", nullptr, ImGui::GetContentRegionAvail())) {
            if (ImGui::BeginTable("##imhex_paths", 2, ImGuiTableFlags_ScrollY | ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn("Type");
                ImGui::TableSetupColumn("Paths");

                // Draw the table
                ImGui::TableHeadersRow();
                for (const auto &[name, paths] : PathTypes) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(name);

                    ImGui::TableNextColumn();
                    for (auto &path : paths->all()){
                        // Draw hyperlink to paths that exist or red text if they don't
                        if (wolv::io::fs::isDirectory(path)){
                            if (ImGuiExt::Hyperlink(wolv::util::toUTF8String(path).c_str())) {
                                fs::openFolderExternal(path);
                            }
                        } else {
                            ImGuiExt::TextFormattedColored(ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_ToolbarRed), wolv::util::toUTF8String(path));
                        }
                    }
                }

                ImGui::EndTable();
            }

        }
        ImGuiExt::EndSubWindow();
        ImGui::PopStyleVar();

    }

    struct ReleaseNotes {
        std::string title;
        std::string versionString;
        AutoReset<std::shared_ptr<ui::Markdown>> markdown;
    };

    static ReleaseNotes parseReleaseNotes(const HttpRequest::Result<std::string>& response) {
        ReleaseNotes notes;
        nlohmann::json json;

        if (!response.isSuccess()) {
            // An error occurred, display it
            notes.markdown = std::make_shared<ui::Markdown>("## HTTP Error: " + std::to_string(response.getStatusCode()));

            return notes;
        }

        // A valid response was received, parse it
        try {
            json = nlohmann::json::parse(response.getData());

            // Get the release title
            notes.title = json["name"].get<std::string>();

            // Get the release version string
            notes.versionString = json["tag_name"].get<std::string>();

            // Get the release notes and split it into lines
            auto body = json["body"].get<std::string>();

            std::string content;
            content += fmt::format("# {} | {}\n", notes.versionString, notes.title);
            content += fmt::format("---\n");
            content += body;
            notes.markdown = std::make_shared<ui::Markdown>(content);
        } catch (std::exception &e) {
            notes.markdown = std::make_shared<ui::Markdown>("## Error: " + std::string(e.what()));
        }

        return notes;
    }

    void ViewAbout::drawReleaseNotesPage() {
        static ReleaseNotes notes;

        // Set up the request to get the release notes the first time the page is opened
        const static auto ImHexVersion = ImHexApi::System::getImHexVersion();
        AT_FIRST_TIME {
            static HttpRequest request("GET", GitHubApiURL + std::string("/releases/") + (ImHexVersion.nightly() ? "latest" : ( "tags/v" + ImHexVersion.get(false))));

            m_releaseNoteRequest = request.execute();
        };

        // Wait for the request to finish and parse the response
        if (m_releaseNoteRequest.valid()) {
            if (m_releaseNoteRequest.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                notes = parseReleaseNotes(m_releaseNoteRequest.get());
            } else {
                // Draw a spinner while the release notes are loading
                ImGuiExt::TextSpinner("hex.ui.common.loading"_lang);
            }
        }

        if (*notes.markdown != nullptr)
            (*notes.markdown)->draw();
    }

    struct Commit {
        std::string hash;
        std::string message;
        std::string description;
        std::string author;
        std::string date;
        std::string url;
    };

    static std::vector<Commit> parseCommits(const HttpRequest::Result<std::string>& response) {
        nlohmann::json json;
        std::vector<Commit> commits;

        if (!response.isSuccess()) {
            // An error occurred, display it
            commits.emplace_back(
                    "hex.ui.common.error"_lang,
                    "HTTP " + std::to_string(response.getStatusCode()),
                    "",
                    "",
                    ""
            );

            return { };
        }

        // A valid response was received, parse it
        try {
            json = nlohmann::json::parse(response.getData());

            for (auto &commit: json) {
                const auto message = commit["commit"]["message"].get<std::string>();

                // Split commit title and description. They're separated by two newlines.
                const auto messageEnd = message.find("\n\n");

                auto commitTitle = messageEnd == std::string::npos ? message : message.substr(0, messageEnd);
                auto commitDescription =
                        messageEnd == std::string::npos ? "" : message.substr(commitTitle.size() + 2);

                auto url = commit["html_url"].get<std::string>();
                auto sha = commit["sha"].get<std::string>();
                auto date = commit["commit"]["author"]["date"].get<std::string>();
                auto author = fmt::format("{} <{}>",
                                          commit["commit"]["author"]["name"].get<std::string>(),
                                          commit["commit"]["author"]["email"].get<std::string>()
                );

                // Move the commit data into the list of commits
                commits.emplace_back(
                        std::move(sha),
                        std::move(commitTitle),
                        std::move(commitDescription),
                        std::move(author),
                        std::move(date),
                        std::move(url)
                );
            }

        } catch (std::exception &e) {
            commits.emplace_back(
                    "hex.ui.common.error"_lang,
                    e.what(),
                    "",
                    "",
                    ""
            );
        }

        return commits;
    }

    void ViewAbout::drawCommitHistoryPage() {
        static std::vector<Commit> commits;

        // Set up the request to get the commit history the first time the page is opened
        AT_FIRST_TIME {
            static HttpRequest request("GET", GitHubApiURL + std::string("/commits?per_page=100"));
            m_commitHistoryRequest = request.execute();
        };

        // Wait for the request to finish and parse the response
        if (m_commitHistoryRequest.valid()) {
            if (m_commitHistoryRequest.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                commits = parseCommits(m_commitHistoryRequest.get());
            } else {
                // Draw a spinner while the commits are loading
                ImGuiExt::TextSpinner("hex.ui.common.loading"_lang);
            }
        }

        // Draw commits table
        if (commits.empty()) return;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2());
        auto result = ImGuiExt::BeginSubWindow("Commits", nullptr, ImGui::GetContentRegionAvail());
        ImGui::PopStyleVar();

        if (result) {
            this->drawCommitsTable(commits);
        }
        ImGuiExt::EndSubWindow();
    }

    void ViewAbout::drawCommitsTable(const auto& commits) {
        if (ImGui::BeginTable("##commits", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY)) {
            // Draw commits
            for (const auto &commit: commits) {
                ImGui::PushID(commit.hash.c_str());
                ImGui::TableNextRow();

                this->drawCommitRow(commit);

                ImGui::PopID();
            }

            ImGui::EndTable();
        }
    }

    void ViewAbout::drawCommitRow(const auto &commit) {
        // Draw hover tooltip
        ImGui::TableNextColumn();
        if (ImGui::Selectable("##commit", false, ImGuiSelectableFlags_SpanAllColumns)) {
            hex::openWebpage(commit.url);
        }

        if (ImGui::IsItemHovered()) {
            if (ImGui::BeginTooltip()) {
                // Draw author and commit date
                ImGuiExt::TextFormattedColored(ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_Highlight), "{}",
                                               commit.author);
                ImGui::SameLine();
                ImGuiExt::TextFormatted("@ {}", commit.date.c_str());

                // Draw description if there is one
                if (!commit.description.empty()) {
                    ImGui::Separator();
                    ImGuiExt::TextFormatted("{}", commit.description);
                }

                ImGui::EndTooltip();
            }

        }

        // Draw commit hash
        ImGui::SameLine(0, 0);
        ImGuiExt::TextFormattedColored(ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_Highlight), "{}",
                                       commit.hash.substr(0, 7));

        // Draw the commit message
        ImGui::TableNextColumn();

        const ImColor color = [&] {
            if (commit.hash == ImHexApi::System::getCommitHash(true))
                return ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive);
            else
                return ImGui::GetStyleColorVec4(ImGuiCol_Text);
        }();
        ImGuiExt::TextFormattedColored(color, commit.message);
    }

    void ViewAbout::drawLicensePage() {
        const auto indentation = 50_scaled;

        ImGui::Indent(indentation);
        ImGuiExt::TextFormattedWrapped("{}", romfs::get("licenses/LICENSE").string());
        ImGui::Unindent(indentation);

        static bool enabled = false;
        if (ImGuiExt::DimmedButtonToggle("N" "E" "R" "D", &enabled)) {
            if (enabled) {
                ImHexApi::System::setPostProcessingShader(
                    romfs::get("shaders/retro/vertex.glsl").data<char>(),
                    romfs::get("shaders/retro/fragment.glsl").data<char>()
                );
            } else {
                ImHexApi::System::setPostProcessingShader("", "");
            }
        }
    }

    void ViewAbout::drawAboutPopup() {
        struct Tab {
            using Function = void (ViewAbout::*)();

            const char *unlocalizedName;
            Function function;
        };

        constexpr std::array Tabs = {
            Tab { .unlocalizedName="ImHex",                                      .function=&ViewAbout::drawAboutMainPage       },
            Tab { .unlocalizedName="hex.builtin.view.help.about.contributor",    .function=&ViewAbout::drawContributorPage     },
            Tab { .unlocalizedName="hex.builtin.view.help.about.libs",           .function=&ViewAbout::drawLibraryCreditsPage  },
            Tab { .unlocalizedName="hex.builtin.view.help.about.plugins",        .function=&ViewAbout::drawLoadedPlugins       },
            Tab { .unlocalizedName="hex.builtin.view.help.about.paths",          .function=&ViewAbout::drawPathsPage           },
            Tab { .unlocalizedName="hex.builtin.view.help.about.release_notes",  .function=&ViewAbout::drawReleaseNotesPage    },
            Tab { .unlocalizedName="hex.builtin.view.help.about.commits",        .function=&ViewAbout::drawCommitHistoryPage   },
            Tab { .unlocalizedName="hex.builtin.view.help.about.license",        .function=&ViewAbout::drawLicensePage         },
        };

        // Allow the window to be closed by pressing ESC
        if (ImGui::IsKeyDown(ImGuiKey_Escape))
            ImGui::CloseCurrentPopup();

        if (ImGui::BeginTabBar("about_tab_bar")) {
            // Draw all tabs
            for (const auto &[unlocalizedName, function] : Tabs) {
                if (ImGui::BeginTabItem(Lang(unlocalizedName))) {
                    ImGui::NewLine();

                    if (ImGui::BeginChild(1)) {
                        (this->*function)();
                    }
                    ImGui::EndChild();

                    ImGui::EndTabItem();
                }
            }

            ImGui::EndTabBar();
        }
    }

    void ViewAbout::drawContent() {
        this->drawAboutPopup();
    }

}
