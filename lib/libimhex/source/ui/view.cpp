#include <hex/ui/view.hpp>

#include <imgui.h>

#include <functional>
#include <string>
#include <vector>

namespace hex {

    ImFontAtlas *View::s_fontAtlas;
    ImFontConfig View::s_fontConfig;

    View::View(std::string unlocalizedName) : m_unlocalizedViewName(std::move(unlocalizedName)) { }

    bool View::isAvailable() const {
        return ImHexApi::Provider::isValid() && ImHexApi::Provider::get()->isAvailable();
    }

    void View::showInfoPopup(const std::string &message) {
        EventManager::post<RequestShowInfoPopup>(message);
    }

    void View::showErrorPopup(const std::string &message) {
        EventManager::post<RequestShowErrorPopup>(message);
    }

    void View::showFatalPopup(const std::string &message) {
        EventManager::post<RequestShowFatalErrorPopup>(message);
    }

    void View::showYesNoQuestionPopup(const std::string &message, const std::function<void()> &yesCallback, const std::function<void()> &noCallback) {
        EventManager::post<RequestShowYesNoQuestionPopup>(message, yesCallback, noCallback);

    }

    void View::showFileChooserPopup(const std::vector<std::fs::path> &paths, const std::vector<nfdfilteritem_t> &validExtensions, const std::function<void(std::fs::path)> &callback) {
        if (paths.empty()) {
            fs::openFileBrowser(fs::DialogMode::Open, validExtensions, [callback](const auto &path) {
                callback(path);
            });
        } else {
            EventManager::post<RequestShowFileChooserPopup>(paths, validExtensions, callback);
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