#include <hex/ui/view.hpp>

#include <imgui.h>

#include <functional>
#include <string>
#include <vector>

namespace hex {

    std::string View::s_popupMessage;

    std::function<void()> View::s_yesCallback, View::s_noCallback;

    u32 View::s_selectableFileIndex;
    std::vector<std::fs::path> View::s_selectableFiles;
    std::function<void(std::fs::path)> View::s_selectableFileOpenCallback;
    std::vector<nfdfilteritem_t> View::s_selectableFilesValidExtensions;

    ImFontAtlas *View::s_fontAtlas;
    ImFontConfig View::s_fontConfig;

    View::View(std::string unlocalizedName) : m_unlocalizedViewName(std::move(unlocalizedName)) { }

    bool View::isAvailable() const {
        return ImHexApi::Provider::isValid() && ImHexApi::Provider::get()->isAvailable();
    }

    void View::drawCommonInterfaces() {
        auto windowSize = ImHexApi::System::getMainWindowSize();

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

        bool opened = true;
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5F, 0.5F));
        if (ImGui::BeginPopupModal("hex.builtin.common.choose_file"_lang, &opened, ImGuiWindowFlags_AlwaysAutoResize)) {

            if (ImGui::BeginListBox("##files", ImVec2(300_scaled, 0))) {

                u32 index = 0;
                for (auto &path : View::s_selectableFiles) {
                    if (ImGui::Selectable(path.filename().string().c_str(), index == View::s_selectableFileIndex))
                        View::s_selectableFileIndex = index;
                    index++;
                }

                ImGui::EndListBox();
            }

            if (ImGui::Button("hex.builtin.common.open"_lang)) {
                View::s_selectableFileOpenCallback(View::s_selectableFiles[View::s_selectableFileIndex]);
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();

            if (ImGui::Button("hex.builtin.common.browse"_lang)) {
                fs::openFileBrowser("hex.builtin.common.open"_lang, fs::DialogMode::Open, View::s_selectableFilesValidExtensions, [](const auto &path) {
                    View::s_selectableFileOpenCallback(path);
                    ImGui::CloseCurrentPopup();
                });
            }

            ImGui::EndPopup();
        }
    }

    void View::showMessagePopup(const std::string &message) {
        s_popupMessage = message;

        ImHexApi::Tasks::doLater([] { ImGui::OpenPopup("hex.builtin.common.info"_lang); });
    }

    void View::showErrorPopup(const std::string &errorMessage) {
        s_popupMessage = errorMessage;

        ImHexApi::Tasks::doLater([] { ImGui::OpenPopup("hex.builtin.common.error"_lang); });
    }

    void View::showFatalPopup(const std::string &errorMessage) {
        s_popupMessage = errorMessage;

        ImHexApi::Tasks::doLater([] { ImGui::OpenPopup("hex.builtin.common.fatal"_lang); });
    }

    void View::showYesNoQuestionPopup(const std::string &message, const std::function<void()> &yesCallback, const std::function<void()> &noCallback) {
        s_popupMessage = message;

        s_yesCallback = yesCallback;
        s_noCallback  = noCallback;

        ImHexApi::Tasks::doLater([] { ImGui::OpenPopup("hex.builtin.common.question"_lang); });
    }

    void View::showFileChooserPopup(const std::vector<std::fs::path> &paths, const std::vector<nfdfilteritem_t> &validExtensions, const std::function<void(std::fs::path)> &callback) {
        if (paths.empty()) {
            fs::openFileBrowser("hex.builtin.common.open"_lang, fs::DialogMode::Open, validExtensions, [callback](const auto &path) {
                callback(path);
            });
        } else {
            View::s_selectableFileIndex            = 0;
            View::s_selectableFiles                = paths;
            View::s_selectableFilesValidExtensions = validExtensions;
            View::s_selectableFileOpenCallback     = callback;

            ImHexApi::Tasks::doLater([] { ImGui::OpenPopup("hex.builtin.common.choose_file"_lang); });
        }
    }

    bool View::hasViewMenuItemEntry() const {
        return true;
    }

    ImVec2 View::getMinSize() const {
        return scaled(ImVec2(480, 720));
    }

    ImVec2 View::getMaxSize() const {
        return { FLT_MAX, FLT_MAX };
    }


    bool &View::getWindowOpenState() {
        return this->m_windowOpen;
    }

    const bool &View::getWindowOpenState() const {
        return this->m_windowOpen;
    }

    const std::string &View::getUnlocalizedName() const {
        return this->m_unlocalizedViewName;
    }

    std::string View::getName() const {
        return View::toWindowName(this->m_unlocalizedViewName);
    }

    void View::discardNavigationRequests() {
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows))
            ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
    }

    void View::confirmButtons(const std::string &textLeft, const std::string &textRight, const std::function<void()> &leftButtonFn, const std::function<void()> &rightButtonFn) {
        auto width = ImGui::GetWindowWidth();
        ImGui::SetCursorPosX(width / 9);
        if (ImGui::Button(textLeft.c_str(), ImVec2(width / 3, 0)))
            leftButtonFn();
        ImGui::SameLine();
        ImGui::SetCursorPosX(width / 9 * 5);
        if (ImGui::Button(textRight.c_str(), ImVec2(width / 3, 0)))
            rightButtonFn();
    }

}