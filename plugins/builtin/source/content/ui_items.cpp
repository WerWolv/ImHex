#include <hex/api/content_registry.hpp>
#include <hex/api/localization.hpp>

#include <hex/ui/view.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/helpers/logger.hpp>

#include <codicons_font.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <hex/ui/imgui_imhex_extensions.h>

namespace hex::plugin::builtin {

    static std::string s_popupMessage;
    static std::function<void()> s_yesCallback, s_noCallback;
    static u32 s_selectableFileIndex;
    static std::vector<std::fs::path> s_selectableFiles;
    static std::function<void(std::fs::path)> s_selectableFileOpenCallback;
    static std::vector<nfdfilteritem_t> s_selectableFilesValidExtensions;

    static void drawGlobalPopups() {

        // "Are you sure you want to exit?" Popup
        if (ImGui::BeginPopupModal("hex.builtin.popup.exit_application.title"_lang, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::NewLine();
            ImGui::TextUnformatted("hex.builtin.popup.exit_application.desc"_lang);
            ImGui::NewLine();

            View::confirmButtons(
                "hex.builtin.common.yes"_lang, "hex.builtin.common.no"_lang, [] { ImHexApi::Common::closeImHex(true); }, [] { ImGui::CloseCurrentPopup(); });

            if (ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_Escape)))
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }

        auto windowSize = ImHexApi::System::getMainWindowSize();

        // Info popup
        ImGui::SetNextWindowSizeConstraints(scaled(ImVec2(400, 100)), scaled(ImVec2(600, 300)));
        if (ImGui::BeginPopupModal("hex.builtin.common.info"_lang, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextFormattedWrapped("{}", s_popupMessage.c_str());
            ImGui::NewLine();
            ImGui::Separator();
            if (ImGui::Button("hex.builtin.common.okay"_lang) || ImGui::IsKeyDown(ImGuiKey_Escape))
                ImGui::CloseCurrentPopup();

            ImGui::SetWindowPos((windowSize - ImGui::GetWindowSize()) / 2, ImGuiCond_Appearing);
            ImGui::EndPopup();
        }

        // Error popup
        ImGui::SetNextWindowSizeConstraints(scaled(ImVec2(400, 100)), scaled(ImVec2(600, 300)));
        if (ImGui::BeginPopupModal("hex.builtin.common.error"_lang, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextFormattedWrapped("{}", s_popupMessage.c_str());
            ImGui::NewLine();
            ImGui::Separator();
            if (ImGui::Button("hex.builtin.common.okay"_lang) || ImGui::IsKeyDown(ImGuiKey_Escape))
                ImGui::CloseCurrentPopup();

            ImGui::SetWindowPos((windowSize - ImGui::GetWindowSize()) / 2, ImGuiCond_Appearing);
            ImGui::EndPopup();
        }

        // Fatal error popup
        ImGui::SetNextWindowSizeConstraints(scaled(ImVec2(400, 100)), scaled(ImVec2(600, 300)));
        if (ImGui::BeginPopupModal("hex.builtin.common.fatal"_lang, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextFormattedWrapped("{}", s_popupMessage.c_str());
            ImGui::NewLine();
            ImGui::Separator();
            if (ImGui::Button("hex.builtin.common.okay"_lang) || ImGui::IsKeyDown(ImGuiKey_Escape)) {
                ImHexApi::Common::closeImHex();
                ImGui::CloseCurrentPopup();
            }

            ImGui::SetWindowPos((windowSize - ImGui::GetWindowSize()) / 2, ImGuiCond_Appearing);
            ImGui::EndPopup();
        }

        // Yes/No question popup
        ImGui::SetNextWindowSizeConstraints(scaled(ImVec2(400, 100)), scaled(ImVec2(600, 300)));
        if (ImGui::BeginPopupModal("hex.builtin.common.question"_lang, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextFormattedWrapped("{}", s_popupMessage.c_str());
            ImGui::NewLine();
            ImGui::Separator();

            View::confirmButtons(
                    "hex.builtin.common.yes"_lang, "hex.builtin.common.no"_lang, [] {
                        s_yesCallback();
                        ImGui::CloseCurrentPopup(); }, [] {
                        s_noCallback();
                        ImGui::CloseCurrentPopup(); });

            ImGui::SetWindowPos((windowSize - ImGui::GetWindowSize()) / 2, ImGuiCond_Appearing);
            ImGui::EndPopup();
        }

        // File chooser popup
        bool opened = true;
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5F, 0.5F));
        if (ImGui::BeginPopupModal("hex.builtin.common.choose_file"_lang, &opened, ImGuiWindowFlags_AlwaysAutoResize)) {

            if (ImGui::BeginListBox("##files", ImVec2(300_scaled, 0))) {

                u32 index = 0;
                for (auto &path : s_selectableFiles) {
                    if (ImGui::Selectable(path.filename().string().c_str(), index == s_selectableFileIndex))
                        s_selectableFileIndex = index;
                    index++;
                }

                ImGui::EndListBox();
            }

            if (ImGui::Button("hex.builtin.common.open"_lang)) {
                s_selectableFileOpenCallback(s_selectableFiles[s_selectableFileIndex]);
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();

            if (ImGui::Button("hex.builtin.common.browse"_lang)) {
                fs::openFileBrowser(fs::DialogMode::Open, s_selectableFilesValidExtensions, [](const auto &path) {
                    s_selectableFileOpenCallback(path);
                    ImGui::CloseCurrentPopup();
                });
            }

            ImGui::EndPopup();
        }
    }

    void addGlobalUIItems() {
        EventManager::subscribe<EventFrameEnd>(drawGlobalPopups);

        EventManager::subscribe<RequestShowInfoPopup>([](const std::string &message) {
            s_popupMessage = message;

            ImHexApi::Tasks::doLater([] { ImGui::OpenPopup("hex.builtin.common.info"_lang); });
        });

        EventManager::subscribe<RequestShowErrorPopup>([](const std::string &message) {
            s_popupMessage = message;

            ImHexApi::Tasks::doLater([] { ImGui::OpenPopup("hex.builtin.common.error"_lang); });
        });

        EventManager::subscribe<RequestShowFatalErrorPopup>([](const std::string &message) {
            s_popupMessage = message;

            ImHexApi::Tasks::doLater([] { ImGui::OpenPopup("hex.builtin.common.fatal"_lang); });
        });

        EventManager::subscribe<RequestShowYesNoQuestionPopup>([](const std::string &message, const std::function<void()> &yesCallback, const std::function<void()> &noCallback) {
            s_popupMessage = message;

            s_yesCallback = yesCallback;
            s_noCallback  = noCallback;

            ImHexApi::Tasks::doLater([] { ImGui::OpenPopup("hex.builtin.common.question"_lang); });
        });

        EventManager::subscribe<RequestShowFileChooserPopup>([](const std::vector<std::fs::path> &paths, const std::vector<nfdfilteritem_t> &validExtensions, const std::function<void(std::fs::path)> &callback) {
            s_selectableFileIndex            = 0;
            s_selectableFiles                = paths;
            s_selectableFilesValidExtensions = validExtensions;
            s_selectableFileOpenCallback     = callback;

            ImHexApi::Tasks::doLater([] { ImGui::OpenPopup("hex.builtin.common.choose_file"_lang); });
        });
    }

    void addFooterItems() {

        if (hex::isProcessElevated()) {
            ContentRegistry::Interface::addFooterItem([] {
                ImGui::TextUnformatted(ICON_VS_SHIELD);
            });
        }

        ContentRegistry::Interface::addFooterItem([] {
            static float framerate = 0;
            if (ImGui::HasSecondPassed()) {
                framerate = 1.0F / ImGui::GetIO().DeltaTime;
            }

            ImGui::TextFormatted("FPS {0:2}.{1:02}", u32(framerate), u32(framerate * 100) % 100);
        });

        ContentRegistry::Interface::addFooterItem([] {
            size_t taskCount    = 0;
            double taskProgress = 0.0;
            std::string taskName;

            {
                std::scoped_lock lock(Task::getTaskMutex());

                taskCount = Task::getRunningTasks().size();
                if (taskCount > 0) {
                    auto frontTask = Task::getRunningTasks().front();
                    taskProgress   = frontTask->getProgress();
                    taskName       = frontTask->getName();
                }
            }

            if (taskCount > 0) {
                if (taskCount > 0)
                    ImGui::TextSpinner(hex::format("({})", taskCount).c_str());
                else
                    ImGui::TextSpinner("");

                ImGui::SameLine();

                ImGui::SmallProgressBar(taskProgress, (ImGui::GetCurrentWindow()->MenuBarHeight() - 10_scaled) / 2.0);
                ImGui::InfoTooltip(taskName.c_str());
            }
        });
    }

    void addToolbarItems() {
        ContentRegistry::Interface::addToolbarItem([] {
            auto provider      = ImHexApi::Provider::get();
            bool providerValid = provider != nullptr;

            // Undo
            ImGui::BeginDisabled(!providerValid || !provider->canUndo());
            {
                if (ImGui::ToolBarButton(ICON_VS_DISCARD, ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarBlue)))
                    provider->undo();
            }
            ImGui::EndDisabled();

            // Redo
            ImGui::BeginDisabled(!providerValid || !provider->canRedo());
            {
                if (ImGui::ToolBarButton(ICON_VS_REDO, ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarBlue)))
                    provider->redo();
            }
            ImGui::EndDisabled();

            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

            // Create new file
            if (ImGui::ToolBarButton(ICON_VS_FILE, ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarGray)))
                EventManager::post<RequestOpenWindow>("Create File");

            // Open file
            if (ImGui::ToolBarButton(ICON_VS_FOLDER_OPENED, ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarBrown)))
                EventManager::post<RequestOpenWindow>("Open File");


            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

            // Save file
            ImGui::BeginDisabled(!providerValid || !provider->isWritable() || !provider->isSavable());
            {
                if (ImGui::ToolBarButton(ICON_VS_SAVE, ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarBlue)))
                    provider->save();
            }
            ImGui::EndDisabled();

            // Save file as
            ImGui::BeginDisabled(!providerValid || !provider->isSavable());
            {
                if (ImGui::ToolBarButton(ICON_VS_SAVE_AS, ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarBlue)))
                    fs::openFileBrowser(fs::DialogMode::Save, {}, [&provider](auto path) {
                        provider->saveAs(path);
                    });
            }
            ImGui::EndDisabled();


            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);


            // Create bookmark
            ImGui::BeginDisabled(!providerValid || !provider->isReadable() || !ImHexApi::HexEditor::isSelectionValid());
            {
                if (ImGui::ToolBarButton(ICON_VS_BOOKMARK, ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarGreen))) {
                    auto region = ImHexApi::HexEditor::getSelection();

                    if (region.has_value())
                        ImHexApi::Bookmarks::add(region->address, region->size, {}, {});
                }
            }
            ImGui::EndDisabled();

            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            ImGui::Spacing();

            // Provider switcher
            ImGui::BeginDisabled(!providerValid);
            {
                auto &providers = ImHexApi::Provider::getProviders();

                std::string preview;
                if (ImHexApi::Provider::isValid())
                    preview = ImHexApi::Provider::get()->getName();

                ImGui::SetNextItemWidth(200_scaled);
                if (ImGui::BeginCombo("", preview.c_str())) {

                    for (size_t i = 0; i < providers.size(); i++) {
                        if (ImGui::Selectable(providers[i]->getName().c_str())) {
                            ImHexApi::Provider::setCurrentProvider(i);
                        }
                    }

                    ImGui::EndCombo();
                }
            }
            ImGui::EndDisabled();
        });

    }

    void handleBorderlessWindowMode() {
        // Intel's OpenGL driver has weird bugs that cause the drawn window to be offset to the bottom right.
        // This can be fixed by either using Mesa3D's OpenGL Software renderer or by simply disabling it.
        // If you want to try if it works anyways on your GPU, set the hex.builtin.setting.interface.force_borderless_window_mode setting to 1
        if (ImHexApi::System::isBorderlessWindowModeEnabled()) {
            bool isIntelGPU = hex::containsIgnoreCase(ImHexApi::System::getGPUVendor(), "Intel");
            ImHexApi::System::impl::setBorderlessWindowMode(!isIntelGPU);

            if (isIntelGPU)
                log::warn("Intel GPU detected! Intel's OpenGL driver has bugs that can cause issues when using ImHex. If you experience any rendering bugs, please try the Mesa3D Software Renderer");
        }
    }

}