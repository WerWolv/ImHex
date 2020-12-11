#pragma once

#include <hex.hpp>

#include "imgui.h"

#include "helpers/event.hpp"

#include <functional>
#include <string>
#include <vector>


namespace hex {

    class View {
    public:
        View(std::string viewName) : m_viewName(viewName) { }
        virtual ~View() { }

        virtual void createView() = 0;
        virtual void createMenu() { }
        virtual bool handleShortcut(int key, int mods) { return false; }

        static std::vector<std::function<void()>>& getDeferedCalls() {
            return View::s_deferedCalls;
        }

        static void postEvent(Events eventType, const void *userData = nullptr) {
            View::s_eventManager.post(eventType, userData);
        }

        static void drawCommonInterfaces() {
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

        static void showErrorPopup(std::string_view errorMessage) {
            View::s_errorMessage = errorMessage;

            ImGui::OpenPopup("Error");
        }

        virtual bool hasViewMenuItemEntry() { return true; }

        bool& getWindowOpenState() {
            return this->m_windowOpen;
        }

        const std::string getName() const {
            return this->m_viewName;
        }

    protected:
        void subscribeEvent(Events eventType, std::function<void(const void*)> callback) {
            View::s_eventManager.subscribe(eventType, this, callback);
        }

        void unsubscribeEvent(Events eventType) {
            View::s_eventManager.unsubscribe(eventType, this);
        }

        void doLater(std::function<void()> &&function) {
            View::s_deferedCalls.push_back(function);
        }



    private:
        std::string m_viewName;
        bool m_windowOpen = false;

        static inline EventManager s_eventManager;
        static inline std::vector<std::function<void()>> s_deferedCalls;

        static inline std::string s_errorMessage;
    };

    void confirmButtons(const char *textLeft, const char *textRight, auto leftButtonFn, auto rightButtonFn) {
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