#include <hex.hpp>

#include <views/view.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <glad/glad.h>

#include "windows_api.hpp"

struct Process {
    u64 pid;
    std::string name;
    GLuint iconTextureId;
};

static std::vector<Process> processes;

IMHEX_PLUGIN {

    View* createView() {
        return nullptr;
    }

    void drawToolsEntry() {
        if (ImGui::CollapsingHeader("Windows Tools")) {

            if (ImGui::BeginTable("##processes", 3, ImGuiTableFlags_Borders)) {
                ImGui::TableSetupColumn("Icon", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("PID", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableHeadersRow();

                ImGuiListClipper clipper;
                clipper.Begin(processes.size());

                while (clipper.Step())
                    for (u32 i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();

                        ImGui::Image((void*)(intptr_t)processes[i].iconTextureId, ImVec2(32, 32));
                        ImGui::TableNextColumn();
                        ImGui::Text("%lu", processes[i].pid);
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(processes[i].name.c_str());
                    }

                clipper.End();

                ImGui::EndTable();
            }

            if (ImGui::Button("Refresh")) {
                processes.clear();

                auto pids = hex::plugin::win::getProcessIDs();

                for (auto &pid : pids) {
                    if (pid == 0)
                        continue;

                    Process process;
                    auto icon = hex::plugin::win::getProcessIcon(pid);

                    process.pid = pid;
                    process.name = hex::plugin::win::getProcessName(pid);

                    glGenTextures(1, &process.iconTextureId);
                    glBindTexture(GL_TEXTURE_2D, process.iconTextureId);

                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, icon.width, icon.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, icon.data.data());

                    processes.push_back(process);
                }
            }

        }
    }

}


