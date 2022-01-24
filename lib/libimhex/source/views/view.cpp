#include <hex/views/view.hpp>

#include <imgui.h>

#include <functional>
#include <string>
#include <vector>

#include <hex/helpers/shared_data.hpp>

namespace hex {

    View::View(std::string unlocalizedName) : m_unlocalizedViewName(unlocalizedName) { }

    bool View::isAvailable() const {
        return ImHexApi::Provider::isValid() && ImHexApi::Provider::get()->isAvailable();
    }

    std::vector<std::function<void()>> &View::getDeferedCalls() {
        return SharedData::deferredCalls;
    }

    void View::drawCommonInterfaces() {
        ImGui::SetNextWindowSizeConstraints(scaled(ImVec2(400, 100)), scaled(ImVec2(600, 300)));
        if (ImGui::BeginPopupModal("hex.common.info"_lang, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextFormattedWrapped("{}", SharedData::popupMessage.c_str());
            ImGui::NewLine();
            ImGui::Separator();
            if (ImGui::Button("hex.common.okay"_lang) || ImGui::IsKeyDown(ImGuiKey_Escape))
                ImGui::CloseCurrentPopup();

            ImGui::SetWindowPos((SharedData::windowSize - ImGui::GetWindowSize()) / 2, ImGuiCond_Appearing);
            ImGui::EndPopup();
        }

        ImGui::SetNextWindowSizeConstraints(scaled(ImVec2(400, 100)), scaled(ImVec2(600, 300)));
        if (ImGui::BeginPopupModal("hex.common.error"_lang, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextFormattedWrapped("{}", SharedData::popupMessage.c_str());
            ImGui::NewLine();
            ImGui::Separator();
            if (ImGui::Button("hex.common.okay"_lang) || ImGui::IsKeyDown(ImGuiKey_Escape))
                ImGui::CloseCurrentPopup();

            ImGui::SetWindowPos((SharedData::windowSize - ImGui::GetWindowSize()) / 2, ImGuiCond_Appearing);
            ImGui::EndPopup();
        }

        ImGui::SetNextWindowSizeConstraints(scaled(ImVec2(400, 100)), scaled(ImVec2(600, 300)));
        if (ImGui::BeginPopupModal("hex.common.fatal"_lang, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextFormattedWrapped("{}", SharedData::popupMessage.c_str());
            ImGui::NewLine();
            ImGui::Separator();
            if (ImGui::Button("hex.common.okay"_lang) || ImGui::IsKeyDown(ImGuiKey_Escape)) {
                ImHexApi::Common::closeImHex();
                ImGui::CloseCurrentPopup();
            }

            ImGui::SetWindowPos((SharedData::windowSize - ImGui::GetWindowSize()) / 2, ImGuiCond_Appearing);
            ImGui::EndPopup();
        }

        bool opened = true;
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5F, 0.5F));
        if (ImGui::BeginPopupModal("hex.common.choose_file"_lang, &opened, ImGuiWindowFlags_AlwaysAutoResize)) {

            if (ImGui::BeginListBox("##files", ImVec2(300_scaled, 0))) {

                u32 index = 0;
                for (auto &path : SharedData::selectableFiles) {
                    if (ImGui::Selectable(path.filename().string().c_str(), index == SharedData::selectableFileIndex))
                        SharedData::selectableFileIndex = index;
                    index++;
                }

                ImGui::EndListBox();
            }

            if (ImGui::Button("hex.common.open"_lang)) {
                SharedData::selectableFileOpenCallback(SharedData::selectableFiles[SharedData::selectableFileIndex]);
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();

            if (ImGui::Button("hex.common.browse"_lang)) {
                hex::openFileBrowser("hex.common.open"_lang, DialogMode::Open, SharedData::selectableFilesValidExtensions, [](const auto &path) {
                    SharedData::selectableFileOpenCallback(path);
                    ImGui::CloseCurrentPopup();
                });
            }

            ImGui::EndPopup();
        }
    }

    void View::showMessagePopup(const std::string &message) {
        SharedData::popupMessage = message;

        View::doLater([] { ImGui::OpenPopup("hex.common.info"_lang); });
    }

    void View::showErrorPopup(const std::string &errorMessage) {
        SharedData::popupMessage = errorMessage;

        View::doLater([] { ImGui::OpenPopup("hex.common.error"_lang); });
    }

    void View::showFatalPopup(const std::string &errorMessage) {
        SharedData::popupMessage = errorMessage;

        View::doLater([] { ImGui::OpenPopup("hex.common.fatal"_lang); });
    }

    void View::showFileChooserPopup(const std::vector<fs::path> &paths, const std::vector<nfdfilteritem_t> &validExtensions, const std::function<void(fs::path)> &callback) {
        SharedData::selectableFileIndex = 0;
        SharedData::selectableFiles = paths;
        SharedData::selectableFilesValidExtensions = validExtensions;
        SharedData::selectableFileOpenCallback = callback;

        View::doLater([] { ImGui::OpenPopup("hex.common.choose_file"_lang); });
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

    void View::doLater(std::function<void()> &&function) {
        SharedData::deferredCalls.push_back(function);
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