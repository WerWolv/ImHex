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
        if (ImGui::BeginPopupModal("Error", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::NewLine();
            if (ImGui::BeginChild("##scrolling", ImVec2(300, 100))) {
                ImGui::SetCursorPosX((300 - ImGui::CalcTextSize(SharedData::errorPopupMessage.c_str(), nullptr, false).x) / 2.0F);
                ImGui::TextWrapped("%s", SharedData::errorPopupMessage.c_str());
                ImGui::EndChild();
            }
            ImGui::NewLine();
            ImGui::SetCursorPosX(75);
            if (ImGui::Button("Okay", ImVec2(150, 20)) || ImGui::IsKeyDown(ImGuiKey_Escape))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

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