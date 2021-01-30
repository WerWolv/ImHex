#include <hex/views/view.hpp>

#include <imgui.h>

#include <functional>
#include <string>
#include <vector>

#include <hex/helpers/shared_data.hpp>

namespace hex {


    View::View(std::string viewName) : m_viewName(viewName) { }

    void View::drawMenu() { }
    bool View::handleShortcut(int key, int mods) { return false; }

    std::vector<std::function<void()>>& View::getDeferedCalls() {
        return SharedData::deferredCalls;
    }

    std::vector<std::any> View::postEvent(Events eventType, const std::any &userData) {
        return EventManager::post(eventType, userData);
    }

    void View::openFileBrowser(std::string title, imgui_addons::ImGuiFileBrowser::DialogMode mode, std::string validExtensions, const std::function<void(std::string)> &callback) {
        SharedData::fileBrowserTitle = title;
        SharedData::fileBrowserDialogMode = mode;
        SharedData::fileBrowserValidExtensions = std::move(validExtensions);
        SharedData::fileBrowserCallback = callback;

        View::doLater([title]{
           ImGui::OpenPopup(title.c_str());
        });
    }

    void View::drawCommonInterfaces() {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 10));
        if (ImGui::BeginPopupModal("Error", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("%s", SharedData::errorPopupMessage.c_str());
            ImGui::NewLine();
            ImGui::Separator();
            if (ImGui::Button("Okay") || ImGui::IsKeyDown(ImGuiKey_Escape))
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }
        ImGui::PopStyleVar();

        if (SharedData::fileBrowser.showFileDialog(SharedData::fileBrowserTitle, SharedData::fileBrowserDialogMode, ImVec2(0, 0), SharedData::fileBrowserValidExtensions)) {
            SharedData::fileBrowserCallback(SharedData::fileBrowser.selected_path);
            SharedData::fileBrowserTitle = "";
        }
    }

    void View::showErrorPopup(std::string_view errorMessage) {
        SharedData::errorPopupMessage = errorMessage;

        ImGui::OpenPopup("Error");
    }

    bool View::hasViewMenuItemEntry() {
        return true;
    }

    ImVec2 View::getMinSize() {
        return ImVec2(480, 720);
    }

    ImVec2 View::getMaxSize() {
        return ImVec2(FLT_MAX, FLT_MAX);
    }


    bool& View::getWindowOpenState() {
        return this->m_windowOpen;
    }

    std::string_view View::getName() const {
        return this->m_viewName;
    }

    void View::subscribeEvent(Events eventType, const std::function<std::any(const std::any&)> &callback) {
        EventManager::subscribe(eventType, this, callback);
    }

    void View::subscribeEvent(Events eventType, const std::function<void(const std::any&)> &callback) {
        EventManager::subscribe(eventType, this, [callback](auto userData) -> std::any { callback(userData); return { }; });
    }

    void View::unsubscribeEvent(Events eventType) {
        EventManager::unsubscribe(eventType, this);
    }

    void View::discardNavigationRequests() {
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows))
            ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
    }

    void View::doLater(std::function<void()> &&function) {
        SharedData::deferredCalls.push_back(function);
    }

    void View::confirmButtons(const char *textLeft, const char *textRight, const std::function<void()> &leftButtonFn, const std::function<void()> &rightButtonFn) {
        auto width = ImGui::GetWindowWidth();
        ImGui::SetCursorPosX(width / 9);
        if (ImGui::Button(textLeft, ImVec2(width / 3, 0)))
            leftButtonFn();
        ImGui::SameLine();
        ImGui::SetCursorPosX(width / 9 * 5);
        if (ImGui::Button(textRight, ImVec2(width / 3, 0)))
            rightButtonFn();
    }

}