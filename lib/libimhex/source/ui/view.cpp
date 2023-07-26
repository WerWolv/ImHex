#include <hex/ui/view.hpp>

#include <imgui.h>

#include <functional>
#include <string>
#include <vector>

namespace hex {

    namespace {

        ImFontAtlas *s_fontAtlas;
        ImFontConfig s_fontConfig;

    }

    View::View(std::string unlocalizedName) : m_unlocalizedViewName(std::move(unlocalizedName)) { }

    bool View::isAvailable() const {
        return ImHexApi::Provider::isValid() && ImHexApi::Provider::get()->isAvailable();
    }

    bool View::hasViewMenuItemEntry() const {
        return true;
    }

    ImVec2 View::getMinSize() const {
        return scaled(ImVec2(300, 400));
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

    ImFontAtlas *View::getFontAtlas() { return s_fontAtlas; }
    void View::setFontAtlas(ImFontAtlas *atlas) { s_fontAtlas = atlas; }

    ImFontConfig View::getFontConfig() { return s_fontConfig; }
    void View::setFontConfig(ImFontConfig config) { s_fontConfig = config; }

}