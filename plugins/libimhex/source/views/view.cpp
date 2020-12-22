#include "views/view.hpp"

#include "imgui.h"

#include <functional>
#include <string>
#include <vector>

namespace hex {


    View::View(std::string viewName) : m_viewName(viewName) { }

    void View::createView(ImGuiContext *ctx) {
        ImGui::SetCurrentContext(ctx);
        this->createView();
    }

    void View::createMenu() { }
    bool View::handleShortcut(int key, int mods) { return false; }

    std::vector<std::function<void()>>& View::getDeferedCalls() {
        return View::s_deferedCalls;
    }

    void View::postEvent(Events eventType, const void *userData) {
        View::s_eventManager.post(eventType, userData);
    }

    void View::drawCommonInterfaces() {
        if (ImGui::BeginPopupModal("Error", nullptr, ImGuiWindowFlags_NoResize)) {
            ImGui::NewLine();
            if (ImGui::BeginChild("##scrolling", ImVec2(300, 100))) {
                ImGui::SetCursorPosX((300 - ImGui::CalcTextSize(View::s_errorMessage.c_str(), nullptr, false).x) / 2.0F);
                ImGui::TextWrapped("%s", View::s_errorMessage.c_str());
                ImGui::EndChild();
            }
            ImGui::NewLine();
            ImGui::SetCursorPosX(75);
            if (ImGui::Button("Okay", ImVec2(150, 20)))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
    }

    void View::showErrorPopup(std::string_view errorMessage) {
        View::s_errorMessage = errorMessage;

        ImGui::OpenPopup("Error");
    }

    void View::setWindowPosition(s32 x, s32 y) {
        View::s_windowPos = ImVec2(x, y);
    }

    const ImVec2& View::getWindowPosition() {
        return View::s_windowPos;
    }

    void View::setWindowSize(s32 width, s32 height) {
        View::s_windowSize = ImVec2(width, height);
    }

    const ImVec2& View::getWindowSize() {
        return View::s_windowSize;
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

    const std::string View::getName() const {
        return this->m_viewName;
    }

    void View::subscribeEvent(Events eventType, std::function<void(const void*)> callback) {
        View::s_eventManager.subscribe(eventType, this, callback);
    }

    void View::unsubscribeEvent(Events eventType) {
        View::s_eventManager.unsubscribe(eventType, this);
    }

    void View::doLater(std::function<void()> &&function) {
        View::s_deferedCalls.push_back(function);
    }

    void View::confirmButtons(const char *textLeft, const char *textRight, std::function<void()> leftButtonFn, std::function<void()> rightButtonFn) {
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