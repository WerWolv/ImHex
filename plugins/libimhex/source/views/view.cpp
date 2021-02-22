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

    bool View::isAvailable() {
        return SharedData::currentProvider != nullptr && SharedData::currentProvider->isAvailable();
    }

    std::vector<std::function<void()>>& View::getDeferedCalls() {
        return SharedData::deferredCalls;
    }

    std::vector<std::any> View::postEvent(Events eventType, const std::any &userData) {
        return EventManager::post(eventType, userData);
    }

    void View::openFileBrowser(std::string_view title, DialogMode mode, const std::vector<nfdfilteritem_t> &validExtensions, const std::function<void(std::string)> &callback) {
        NFD::Init();

        nfdchar_t *outPath;
        nfdresult_t result;
        switch (mode) {
            case DialogMode::Open:
                result = NFD::OpenDialog(outPath, validExtensions.data(), validExtensions.size(), nullptr);
                break;
            case DialogMode::Save:
                result = NFD::SaveDialog(outPath, validExtensions.data(), validExtensions.size(), nullptr);
                break;
            case DialogMode::Folder:
                result = NFD::PickFolder(outPath, nullptr);
                break;
            default: __builtin_unreachable();
        }

        if (result == NFD_OKAY) {
            callback(outPath);
            NFD::FreePath(outPath);
        }

        NFD::Quit();
    }

    void View::drawCommonInterfaces() {
        if (ImGui::BeginPopupModal("Error", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("%s", SharedData::errorPopupMessage.c_str());
            ImGui::NewLine();
            ImGui::Separator();
            if (ImGui::Button("Okay") || ImGui::IsKeyDown(ImGuiKey_Escape))
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }
    }

    void View::showErrorPopup(std::string_view errorMessage) {
        SharedData::errorPopupMessage = errorMessage;

        View::doLater([] { ImGui::OpenPopup("Error"); });
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