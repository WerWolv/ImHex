#include "content/views/view_about.hpp"

#include <hex/api_urls.hpp>
#include <hex/api/content_registry.hpp>
#include <hex/api/achievement_manager.hpp>

#include <hex/helpers/fmt.hpp>
#include <hex/helpers/fs.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/helpers/http_requests.hpp>

#include <content/popups/popup_docs_question.hpp>

#include <romfs/romfs.hpp>
#include <wolv/utils/string.hpp>

namespace hex::plugin::builtin {

    ViewAbout::ViewAbout() : View("hex.builtin.view.help.about.name") {

        // Add "About" menu item to the help menu
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.help", "hex.builtin.view.help.about.name" }, 1000, Shortcut::None, [this] {
            TaskManager::doLater([] { ImGui::OpenPopup(View::toWindowName("hex.builtin.view.help.about.name").c_str()); });
            this->m_aboutWindowOpen    = true;
            this->getWindowOpenState() = true;
        });

        ContentRegistry::Interface::addMenuItemSeparator({ "hex.builtin.menu.help" }, 2000);

        // Add documentation links to the help menu
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.help", "hex.builtin.view.help.documentation" }, 3000, Shortcut::None, [] {
            hex::openWebpage("https://docs.werwolv.net/imhex");
            AchievementManager::unlockAchievement("hex.builtin.achievement.starting_out", "hex.builtin.achievement.starting_out.docs.name");
        });

        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.help", "hex.builtin.menu.help.ask_for_help" }, 4000, CTRLCMD + SHIFT + Keys::D, [] {
            PopupDocsQuestion::open();
        });
    }

    static void link(const std::string &name, const std::string &author, const std::string &url) {
        // Draw the hyperlink and open the URL if clicked
        if (ImGui::BulletHyperlink(name.c_str()))
            hex::openWebpage(url);

        // Show the URL as a tooltip
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::TextFormatted("{}", url);
            ImGui::EndTooltip();
        }

        // Show the author if there is one
        if (!author.empty()) {
            ImGui::SameLine(0, 0);
            ImGui::TextFormatted("by {}", author);
        }
    }

    void ViewAbout::drawAboutMainPage() {
        // Draw main about table
        if (ImGui::BeginTable("about_table", 2, ImGuiTableFlags_SizingFixedFit)) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            // Draw the ImHex icon
            if (!this->m_logoTexture.isValid())
                this->m_logoTexture = ImGui::Texture(romfs::get("assets/common/logo.png").span());

            ImGui::Image(this->m_logoTexture, scaled({ 64, 64 }));
            if (ImGui::IsItemHovered() && ImGui::IsItemClicked()) {
                this->m_clickCount += 1;
            }
            ImGui::TableNextColumn();

            // Draw basic information about ImHex and its version
            ImGui::TextFormatted("ImHex Hex Editor v{} by WerWolv  " ICON_FA_CODE_BRANCH, ImHexApi::System::getImHexVersion());

            ImGui::SameLine();

            // Draw a clickable link to the current commit
            if (ImGui::Hyperlink(hex::format("{0}@{1}", ImHexApi::System::getCommitBranch(), ImHexApi::System::getCommitHash()).c_str()))
                hex::openWebpage("https://github.com/WerWolv/ImHex/commit/" + ImHexApi::System::getCommitHash(true));

            // Draw the build date and time
            ImGui::TextFormatted("{}, {}", __DATE__, __TIME__);

            // Draw the author of the current translation
            ImGui::TextUnformatted("hex.builtin.view.help.about.translator"_lang);

            // Draw information about the open-source nature of ImHex
            ImGui::TextUnformatted("hex.builtin.view.help.about.source"_lang);

            ImGui::SameLine();

            // Draw a clickable link to the GitHub repository
            if (ImGui::Hyperlink("WerWolv/ImHex"))
                hex::openWebpage("https://github.com/WerWolv/ImHex");

            ImGui::EndTable();
        }

        ImGui::NewLine();

        // Draw donation links
        ImGui::TextUnformatted("hex.builtin.view.help.about.donations"_lang);
        ImGui::Separator();

        constexpr std::array Links = { "https://werwolv.net/donate", "https://www.patreon.com/werwolv", "https://github.com/sponsors/WerWolv" };

        ImGui::TextFormattedWrapped("{}", static_cast<const char *>("hex.builtin.view.help.about.thanks"_lang));

        ImGui::NewLine();

        for (auto &link : Links) {
            if (ImGui::Hyperlink(link))
                hex::openWebpage(link);
        }
    }

    void ViewAbout::drawContributorPage() {
        ImGui::TextFormattedWrapped("These amazing people have contributed to ImHex in the past. If you'd like to become part of them, please submit a PR to the GitHub Repository!");
        ImGui::NewLine();

        // Draw main ImHex contributors
        link("iTrooz for a huge amount of help maintaining ImHex and the CI", "", "https://github.com/iTrooz");
        link("jumanji144 for a ton of help with the Pattern Language, API and usage stats", "", "https://github.com/Nowilltolife");

        ImGui::NewLine();

        // Draw additional contributors
        link("Mary for porting ImHex to MacOS", "", "https://github.com/marysaka");
        link("Roblabla for adding the MSI Windows installer", "", "https://github.com/roblabla");
        link("jam1garner for adding support for Rust plugins", "", "https://github.com/jam1garner");

        ImGui::NewLine();

        link("All other amazing contributors", "", "https://github.com/WerWolv/ImHex/graphs/contributors/");
    }

    void ViewAbout::drawLibraryCreditsPage() {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.2F, 0.2F, 0.2F, 0.3F));

        // Draw ImGui dependencies
        link("ImGui", "ocornut", "https://github.com/ocornut/imgui/");
        link("imgui_club", "ocornut", "https://github.com/ocornut/imgui_club/");
        link("imnodes", "Nelarius", "https://github.com/Nelarius/imnodes/");
        link("ImGuiColorTextEdit", "BalazsJako", "https://github.com/BalazsJako/ImGuiColorTextEdit/");
        link("ImPlot", "epezent", "https://github.com/epezent/implot/");

        ImGui::NewLine();

        // Draw dependencies maintained by individual people
        link("capstone", "aquynh", "https://github.com/aquynh/capstone/");
        link("JSON for Modern C++", "nlohmann", "https://github.com/nlohmann/json/");
        link("YARA", "VirusTotal", "https://github.com/VirusTotal/yara/");
        link("Native File Dialog Extended", "btzy and mlabbe", "https://github.com/btzy/nativefiledialog-extended/");
        link("libromfs", "WerWolv", "https://github.com/WerWolv/libromfs/");
        link("microtar", "rxi", "https://github.com/rxi/microtar/");
        link("xdgpp", "danyspin97", "https://sr.ht/~danyspin97/xdgpp/");
        link("FreeType", "David Turner", "https://gitlab.freedesktop.org/freetype/freetype/");
        link("mbedTLS", "ARM", "https://github.com/ARMmbed/mbedtls/");
        link("libcurl", "Daniel Stenberg", "https://curl.se/");
        link("libfmt", "vitaut", "https://fmt.dev/");

        ImGui::NewLine();

        // Draw dependencies maintained by groups
        link("GNU libmagic", "", "https://www.darwinsys.com/file/");
        link("GLFW3", "", "https://github.com/glfw/glfw/");
        link("LLVM", "", "https://github.com/llvm/llvm-project/");

        ImGui::PopStyleColor();

        ImGui::NewLine();
    }

    void ViewAbout::drawPathsPage() {
        if (ImGui::BeginTable("##imhex_paths", 2, ImGuiTableFlags_ScrollY | ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("Type");
            ImGui::TableSetupColumn("Paths");

            // Specify the types of paths to display
            constexpr static std::array<std::pair<const char *, fs::ImHexPath>, size_t(fs::ImHexPath::END) - 1U> PathTypes = {
                {
                    { "Patterns", fs::ImHexPath::Patterns },
                    { "Patterns Includes", fs::ImHexPath::PatternsInclude },
                    { "Magic", fs::ImHexPath::Magic },
                    { "Plugins", fs::ImHexPath::Plugins },
                    { "Libraries", fs::ImHexPath::Libraries },
                    { "Yara Patterns", fs::ImHexPath::Yara },
                    { "Config", fs::ImHexPath::Config },
                    { "Resources", fs::ImHexPath::Resources },
                    { "Constants lists", fs::ImHexPath::Constants },
                    { "Custom encodings", fs::ImHexPath::Encodings },
                    { "Logs", fs::ImHexPath::Logs },
                    { "Recent files", fs::ImHexPath::Recent },
                    { "Scripts", fs::ImHexPath::Scripts },
                    { "Themes", fs::ImHexPath::Themes },
                    { "Data inspector scripts", fs::ImHexPath::Inspectors },
                    { "Custom data processor nodes", fs::ImHexPath::Nodes },
                }
            };

            // Draw the table
            ImGui::TableHeadersRow();
            for (const auto &[name, type] : PathTypes) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(name);

                ImGui::TableNextColumn();
                for (auto &path : fs::getDefaultPaths(type, true)){
                    // Draw hyperlink to paths that exist or red text if they don't
                    if (wolv::io::fs::isDirectory(path)){
                        if (ImGui::Hyperlink(wolv::util::toUTF8String(path).c_str())) {
                            fs::openFolderExternal(path);
                        }
                    } else {
                        ImGui::TextFormattedColored(ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarRed), wolv::util::toUTF8String(path));
                    }
                }
            }

            ImGui::EndTable();
        }
    }

    void ViewAbout::drawReleaseNotesPage() {
        static std::string releaseTitle;
        static std::vector<std::string> releaseNotes;

        // Set up the request to get the release notes the first time the page is opened
        AT_FIRST_TIME {
            static HttpRequest request("GET", GitHubApiURL + std::string("/releases/tags/v") + ImHexApi::System::getImHexVersion(false));

            this->m_releaseNoteRequest = request.execute();
        };

        // Wait for the request to finish and parse the response
        if (this->m_releaseNoteRequest.valid()) {
            if (this->m_releaseNoteRequest.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                auto response = this->m_releaseNoteRequest.get();
                nlohmann::json json;

                if (response.isSuccess()) {
                    // A valid response was received, parse it
                    try {
                        json = nlohmann::json::parse(response.getData());

                        // Get the release title
                        releaseTitle = json["name"].get<std::string>();

                        // Get the release notes and split it into lines
                        auto body = json["body"].get<std::string>();
                        releaseNotes = wolv::util::splitString(body, "\r\n");
                    } catch (std::exception &e) {
                        releaseNotes.push_back("## Error: " + std::string(e.what()));
                    }
                } else {
                    // An error occurred, display it
                    releaseNotes.push_back("## HTTP Error: " + std::to_string(response.getStatusCode()));
                }
            } else {
                // Draw a spinner while the release notes are loading
                ImGui::TextSpinner("hex.builtin.common.loading"_lang);
            }
        }


        // Function to handle drawing of a regular text line
        static const auto drawRegularLine = [](const std::string &line) {
            ImGui::Bullet();
            ImGui::SameLine();

            // Check if the line contains bold text
            auto boldStart = line.find("**");
            if (boldStart != std::string::npos) {
                // Find the end of the bold text
                auto boldEnd = line.find("**", boldStart + 2);

                // Draw the line with the bold text highlighted
                ImGui::TextUnformatted(line.substr(0, boldStart).c_str());
                ImGui::SameLine(0, 0);
                ImGui::TextColored(ImGui::GetCustomColorVec4(ImGuiCustomCol_Highlight), line.substr(boldStart + 2, boldEnd - boldStart - 2).c_str());
                ImGui::SameLine(0, 0);
                ImGui::TextUnformatted(line.substr(boldEnd + 2).c_str());
            } else {
                // Draw the line normally
                ImGui::TextUnformatted(line.c_str());
            }
        };

        // Draw the release title
        if (!releaseTitle.empty()) {
            auto title = hex::format("v{}: {}", ImHexApi::System::getImHexVersion(false), releaseTitle);
            ImGui::Header(title.c_str(), true);
            ImGui::Separator();
        }

        // Draw the release notes and format them using parts of the GitHub Markdown syntax
        // This is not a full implementation of the syntax, but it's enough to make the release notes look good.
        for (const auto &line : releaseNotes) {
            if (line.starts_with("## ")) {
                // Draw H2 Header
                ImGui::Header(line.substr(3).c_str());
            } else if (line.starts_with("### ")) {
                // Draw H3 Header
                ImGui::Header(line.substr(4).c_str());
            } else if (line.starts_with("- ")) {
                // Draw bullet point
                drawRegularLine(line.substr(2));
            } else if (line.starts_with("    - ")) {
                // Draw further indented bullet point
                ImGui::Indent();
                ImGui::Indent();
                drawRegularLine(line.substr(6));
                ImGui::Unindent();
                ImGui::Unindent();
            }
        }
    }

    void ViewAbout::drawCommitHistoryPage() {
        struct Commit {
            std::string hash;
            std::string message;
            std::string description;
            std::string author;
            std::string date;
            std::string url;
        };

        static std::vector<Commit> commits;

        // Set up the request to get the commit history the first time the page is opened
        AT_FIRST_TIME {
            static HttpRequest request("GET", GitHubApiURL + std::string("/commits?per_page=100"));
            this->m_commitHistoryRequest = request.execute();
        };

        // Wait for the request to finish and parse the response
        if (this->m_commitHistoryRequest.valid()) {
            if (this->m_commitHistoryRequest.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                auto response = this->m_commitHistoryRequest.get();
                nlohmann::json json;

                if (response.isSuccess()) {
                    // A valid response was received, parse it
                    try {
                        json = nlohmann::json::parse(response.getData());

                        for (auto &commit : json) {
                            const auto message = commit["commit"]["message"].get<std::string>();

                            // Split commit title and description. They're separated by two newlines.
                            const auto messageEnd = message.find("\n\n");

                            auto commitTitle        = messageEnd == std::string::npos ? message : message.substr(0, messageEnd);
                            auto commitDescription  = messageEnd == std::string::npos ? "" : message.substr(commitTitle.size() + 2);

                            auto url    = commit["html_url"].get<std::string>();
                            auto sha    = commit["sha"].get<std::string>();
                            auto date   = commit["commit"]["author"]["date"].get<std::string>();
                            auto author = hex::format("{} <{}>",
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
                            "hex.builtin.common.error"_lang,
                            e.what(),
                            "",
                            "",
                            ""
                        );
                    }
                } else {
                    // An error occurred, display it
                    commits.emplace_back(
                        "hex.builtin.common.error"_lang,
                        "HTTP " + std::to_string(response.getStatusCode()),
                        "",
                        "",
                        ""
                    );
                }
            } else {
                // Draw a spinner while the commits are loading
                ImGui::TextSpinner("hex.builtin.common.loading"_lang);
            }
        }

        // Draw commits table
        if (!commits.empty()) {
            if (ImGui::BeginTable("##commits", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY)) {
                // Draw commits
                for (const auto &commit : commits) {
                    ImGui::PushID(commit.hash.c_str());
                    ImGui::TableNextRow();

                    // Draw hover tooltip
                    ImGui::TableNextColumn();
                    if (ImGui::Selectable("##commit", false, ImGuiSelectableFlags_SpanAllColumns)) {
                        hex::openWebpage(commit.url);
                    }

                    if (ImGui::IsItemHovered()) {
                        if (ImGui::BeginTooltip()) {
                            // Draw author and commit date
                            ImGui::TextFormattedColored(ImGui::GetCustomColorVec4(ImGuiCustomCol_Highlight), "{}", commit.author);
                            ImGui::SameLine();
                            ImGui::TextFormatted("@ {}", commit.date.c_str());

                            // Draw description if there is one
                            if (!commit.description.empty()) {
                                ImGui::Separator();
                                ImGui::TextFormatted("{}", commit.description);
                            }

                            ImGui::EndTooltip();
                        }

                    }

                    // Draw commit hash
                    ImGui::SameLine(0, 0);
                    ImGui::TextFormattedColored(ImGui::GetCustomColorVec4(ImGuiCustomCol_Highlight), "{}", commit.hash.substr(0, 7));

                    // Draw the commit message
                    ImGui::TableNextColumn();

                    const ImColor color = [&]{
                        if (commit.hash == ImHexApi::System::getCommitHash(true))
                            return ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive);
                        else
                            return ImGui::GetStyleColorVec4(ImGuiCol_Text);
                    }();
                    ImGui::TextFormattedColored(color, commit.message);

                    ImGui::PopID();
                }

                ImGui::EndTable();
            }
        }
    }

    void ViewAbout::drawLicensePage() {
        ImGui::TextFormattedWrapped("{}", romfs::get("licenses/LICENSE").string());
    }

    void ViewAbout::drawAboutPopup() {
        struct Tab {
            using Function = void (ViewAbout::*)();

            const char *unlocalizedName;
            Function function;
        };

        constexpr std::array Tabs = {
            Tab { "ImHex",                                      &ViewAbout::drawAboutMainPage       },
            Tab { "hex.builtin.view.help.about.contributor",    &ViewAbout::drawContributorPage     },
            Tab { "hex.builtin.view.help.about.libs",           &ViewAbout::drawLibraryCreditsPage  },
            Tab { "hex.builtin.view.help.about.paths",          &ViewAbout::drawPathsPage           },
            Tab { "hex.builtin.view.help.about.release_notes",  &ViewAbout::drawReleaseNotesPage    },
            Tab { "hex.builtin.view.help.about.commits",        &ViewAbout::drawCommitHistoryPage   },
            Tab { "hex.builtin.view.help.about.license",        &ViewAbout::drawLicensePage         },
        };

        if (ImGui::BeginPopupModal(View::toWindowName("hex.builtin.view.help.about.name").c_str(), &this->m_aboutWindowOpen)) {

            // Allow the window to be closed by pressing ESC
            if (ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_Escape)))
                ImGui::CloseCurrentPopup();

            if (ImGui::BeginTabBar("about_tab_bar")) {
                // Draw all tabs
                for (const auto &[unlocalizedName, function] : Tabs) {
                    if (ImGui::BeginTabItem(LangEntry(unlocalizedName))) {
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

            ImGui::EndPopup();
        }
    }

    void ViewAbout::drawContent() {
        if (!this->m_aboutWindowOpen)
            this->getWindowOpenState() = false;

        this->drawAboutPopup();
    }

}
