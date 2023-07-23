#include "content/views/view_logs.hpp"

#include <hex/helpers/logger.hpp>

namespace hex::plugin::builtin {

    ViewLogs::ViewLogs() : View("hex.builtin.view.logs.name") {
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.extras", "hex.builtin.view.logs.name" }, 2500, Shortcut::None, [&, this] {
            this->m_viewOpen = true;
            this->getWindowOpenState() = true;
        });
    }

    static ImColor getColor(std::string_view level) {
        if (level.contains("DEBUG"))
            return ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarGreen);
        else if (level.contains("INFO"))
            return ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarBlue);
        else if (level.contains("WARN"))
            return ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarYellow);
        else if (level.contains("ERROR"))
            return ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarRed);
        else if (level.contains("FATAL"))
            return ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarPurple);
        return ImGui::GetStyleColorVec4(ImGuiCol_Text);
    }

    static bool shouldDisplay(std::string_view messageLevel, int currLevel) {
        if (currLevel == 0)
            return true;
        else if (currLevel == 1 && messageLevel.contains("INFO"))
            return true;
        else if (currLevel == 2 && messageLevel.contains("WARN"))
            return true;
        else if (currLevel == 3 && messageLevel.contains("ERROR"))
            return true;
        else if (currLevel == 4 && messageLevel.contains("FATAL"))
            return true;
        return false;
    }

    void ViewLogs::drawContent() {
        if (ImGui::Begin(View::toWindowName("hex.builtin.view.logs.name").c_str(), &this->m_viewOpen, ImGuiWindowFlags_NoCollapse)) {

            ImGui::Combo("hex.builtin.view.logs.log_level"_lang, &this->m_logLevel, "DEBUG\0INFO\0WARNING\0ERROR\0FATAL\0");

            if (ImGui::BeginTable("##logs", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY)) {
                ImGui::TableSetupColumn("hex.builtin.view.logs.component"_lang, ImGuiTableColumnFlags_WidthFixed, 100_scaled);
                ImGui::TableSetupColumn("hex.builtin.view.logs.message"_lang);
                ImGui::TableSetupScrollFreeze(0, 1);

                ImGui::TableHeadersRow();

                const auto &logs = log::impl::getLogEntries();
                ImGuiListClipper clipper;
                clipper.Begin(logs.size());

                while (clipper.Step()) {
                    auto end = 0;
                    for (size_t i = clipper.DisplayStart; i < std::min<size_t>(clipper.DisplayEnd + end, logs.size()); i++) {
                        const auto &log = logs[i];

                        if (!shouldDisplay(log.level, this->m_logLevel)) {
                            end++;
                            clipper.ItemsCount--;
                            continue;
                        }

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(log.project.c_str());
                        ImGui::TableNextColumn();
                        ImGui::PushStyleColor(ImGuiCol_Text, getColor(log.level).Value);
                        ImGui::TextUnformatted(log.message.c_str());
                        ImGui::PopStyleColor();
                    }
                }

                ImGui::EndTable();
            }
        }
        ImGui::End();

        this->getWindowOpenState() = this->m_viewOpen;
    }

}