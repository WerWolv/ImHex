#include <hex/views/view.hpp>

#include <imgui.h>

#include <functional>
#include <string>
#include <vector>

#include <hex/helpers/shared_data.hpp>

namespace hex {


    View::View(std::string unlocalizedName) : m_unlocalizedViewName(unlocalizedName) { }

    void View::drawMenu() { }
    bool View::handleShortcut(bool keys[512], bool ctrl, bool shift, bool alt) { return false; }

    bool View::isAvailable() {
        return SharedData::currentProvider != nullptr && SharedData::currentProvider->isAvailable();
    }

    std::vector<std::function<void()>>& View::getDeferedCalls() {
        return SharedData::deferredCalls;
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
        if (ImGui::BeginPopupModal("hex.common.error"_lang, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("%s", SharedData::errorPopupMessage.c_str());
            ImGui::NewLine();
            ImGui::Separator();
            if (ImGui::Button("hex.common.okay"_lang) || ImGui::IsKeyDown(ImGuiKey_Escape))
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }

        if (ImGui::BeginPopupModal("hex.common.fatal"_lang, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("%s", SharedData::errorPopupMessage.c_str());
            ImGui::NewLine();
            ImGui::Separator();
            if (ImGui::Button("hex.common.okay"_lang) || ImGui::IsKeyDown(ImGuiKey_Escape)) {
                EventManager::post<RequestCloseImHex>();
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

    void View::showErrorPopup(std::string_view errorMessage) {
        SharedData::errorPopupMessage = errorMessage;

        View::doLater([] { ImGui::OpenPopup("hex.common.error"_lang); });
    }

    void View::showFatalPopup(std::string_view errorMessage) {
        SharedData::errorPopupMessage = errorMessage;

        View::doLater([] { ImGui::OpenPopup("hex.common.fatal"_lang); });
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

    std::string_view View::getUnlocalizedName() const {
        return this->m_unlocalizedViewName;
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