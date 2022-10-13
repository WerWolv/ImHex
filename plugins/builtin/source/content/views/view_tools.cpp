#include "content/views/view_tools.hpp"
#include <imgui_internal.h>

#include <hex/api/content_registry.hpp>

namespace hex::plugin::builtin {

    ViewTools::ViewTools() : View("hex.builtin.view.tools.name") { }

    void ViewTools::drawContent() {
        auto &tools = ContentRegistry::Tools::getEntries();

        if (ImGui::Begin(View::toWindowName("hex.builtin.view.tools.name").c_str(), &this->getWindowOpenState(), ImGuiWindowFlags_NoCollapse)) {
            for (auto iter = tools.begin(); iter != tools.end(); iter++) {
                auto &[name, function, detached] = *iter;

                if (detached) continue;

                if (ImGui::CollapsingHeader(LangEntry(name))) {
                    function();
                } else {
                    if (ImGui::IsMouseClicked(0) && ImGui::IsItemActivated() && this->m_dragStartIterator == tools.end())
                        this->m_dragStartIterator = iter;

                    if (!ImGui::IsItemHovered() && this->m_dragStartIterator == iter) {
                        detached = true;
                    }

                    if (!ImGui::IsMouseDown(0))
                        this->m_dragStartIterator = tools.end();
                }
            }
        }
        ImGui::End();

        for (auto iter = tools.begin(); iter != tools.end(); iter++) {
            auto &[name, function, detached] = *iter;

            if (!detached) continue;

            ImGui::SetNextWindowSize(scaled(ImVec2(600, 0)), ImGuiCond_Appearing);
            if (ImGui::Begin(View::toWindowName(name).c_str(), &detached, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize)) {
                function();

                if (ImGui::IsWindowAppearing() && this->m_dragStartIterator == iter) {
                    this->m_dragStartIterator = tools.end();

                    // Attach the newly created window to the cursor, so it gets dragged around
                    GImGui->MovingWindow = ImGui::GetCurrentWindow();
                    GImGui->ActiveId = GImGui->MovingWindow->MoveId;
                    ImGui::DockContextQueueUndockWindow(GImGui, GImGui->MovingWindow);
                }
            }
            ImGui::End();
        }
    }

}