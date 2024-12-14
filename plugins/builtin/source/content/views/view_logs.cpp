#include "content/views/view_logs.hpp"

#include <hex/api/content_registry.hpp>
#include <hex/helpers/logger.hpp>

#include <fonts/vscode_icons.hpp>

namespace hex::plugin::builtin {

    ViewLogs::ViewLogs() : View::Floating("hex.builtin.view.logs.name") {
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.extras", "hex.builtin.view.logs.name" }, ICON_VS_BRACKET_ERROR, 2500, Shortcut::None, [&, this] {
            this->getWindowOpenState() = true;
        });
    }

    static ImColor getColor(std::string_view level) {
        if (level.contains("DEBUG"))
            return ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_LoggerDebug);
        else if (level.contains("INFO"))
            return ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_LoggerInfo);
        else if (level.contains("WARN"))
            return ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_LoggerWarning);
        else if (level.contains("ERROR"))
            return ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_LoggerError);
        else if (level.contains("FATAL"))
            return ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_LoggerFatal);
        return ImGui::GetStyleColorVec4(ImGuiCol_Text);
    }

    static bool shouldDisplay(std::string_view messageLevel, int currLevel) {
        if (messageLevel.contains("[DEBUG]"))
            return currLevel <= 0;
        else if (messageLevel.contains("[INFO]"))
            return currLevel <= 1;
        else if (messageLevel.contains("[WARN]"))
            return currLevel <= 2;
        else if (messageLevel.contains("[ERROR]"))
            return currLevel <= 3;
        else if (messageLevel.contains("[FATAL]"))
            return currLevel <= 4;
        else
            return false;
    }

    void ViewLogs::drawContent() {
        ImGui::Combo("hex.builtin.view.logs.log_level"_lang, &m_logLevel, "DEBUG\0INFO\0WARNING\0ERROR\0FATAL\0");

        if (ImGui::BeginTable("##logs", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY)) {
            ImGui::TableSetupColumn("hex.builtin.view.logs.component"_lang, ImGuiTableColumnFlags_WidthFixed, 100_scaled);
            ImGui::TableSetupColumn("hex.builtin.view.logs.message"_lang);
            ImGui::TableSetupScrollFreeze(0, 1);

            ImGui::TableHeadersRow();

            const auto &logs = log::impl::getLogEntries();
            for (const auto &log : logs | std::views::reverse) {
                if (!shouldDisplay(log.level, m_logLevel)) {
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

            ImGui::EndTable();
        }
    }

}