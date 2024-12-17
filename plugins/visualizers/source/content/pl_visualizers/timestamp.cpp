#include <hex/helpers/utils.hpp>

#include <content/visualizer_helpers.hpp>

#include <numbers>

#include <imgui.h>
#include <hex/api/imhex_api.hpp>

#include <hex/ui/imgui_imhex_extensions.h>

#include <chrono>
#include <fmt/chrono.h>

namespace hex::plugin::visualizers {

    void drawTimestampVisualizer(pl::ptrn::Pattern &, bool, std::span<const pl::core::Token::Literal> arguments) {
        time_t timestamp = arguments[0].toUnsigned();
        auto tm = fmt::gmtime(timestamp);
        auto date = std::chrono::year_month_day(std::chrono::year(tm.tm_year + 1900), std::chrono::month(tm.tm_mon + 1), std::chrono::day(tm.tm_mday));

        auto lastMonthDay = std::chrono::year_month_day_last(date.year(), date.month() / std::chrono::last);
        auto firstWeekDay = std::chrono::weekday(std::chrono::year_month_day(date.year(), date.month(), std::chrono::day(1)));

        const auto scale = 1_scaled * (ImHexApi::Fonts::getFontSize() / ImHexApi::Fonts::DefaultFontSize);

        // Draw calendar
        if (ImGui::BeginTable("##month_table", 2)) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            // Draw centered month name and year
            ImGuiExt::TextFormattedCenteredHorizontal("{:%B %Y}", tm);

            if (ImGui::BeginTable("##days_table", 7, ImGuiTableFlags_Borders | ImGuiTableFlags_NoHostExtendX, ImVec2(160, 120) * scale)) {
                constexpr static auto ColumnFlags = ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_NoReorder | ImGuiTableColumnFlags_NoHide;
                ImGui::TableSetupColumn("M", ColumnFlags);
                ImGui::TableSetupColumn("T", ColumnFlags);
                ImGui::TableSetupColumn("W", ColumnFlags);
                ImGui::TableSetupColumn("T", ColumnFlags);
                ImGui::TableSetupColumn("F", ColumnFlags);
                ImGui::TableSetupColumn("S", ColumnFlags);
                ImGui::TableSetupColumn("S", ColumnFlags);

                ImGui::TableHeadersRow();

                ImGui::TableNextRow();

                // Skip days before the first day of the month
                for (u32 i = 0; i < firstWeekDay.c_encoding() - 1; ++i)
                    ImGui::TableNextColumn();

                // Draw days
                for (u32 i = 1; i <= u32(lastMonthDay.day()); ++i) {
                    ImGui::TableNextColumn();
                    ImGuiExt::TextFormatted("{:02}", i);

                    if (std::chrono::day(i) == date.day())
                        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, ImGuiExt::GetCustomColorU32(ImGuiCustomCol_ToolbarRed));

                    if (std::chrono::weekday(std::chrono::year_month_day(date.year(), date.month(), std::chrono::day(i))) == std::chrono::Sunday)
                        ImGui::TableNextRow();
                }

                ImGui::EndTable();
            }

            ImGui::TableNextColumn();

            // Draw analog clock
            const auto size = ImVec2(120, 120) * scale;
            if (ImGui::BeginChild("##clock", size + ImVec2(0, ImGui::GetTextLineHeightWithSpacing()))) {
                // Draw centered digital hour, minute and seconds
                ImGuiExt::TextFormattedCenteredHorizontal("{:%H:%M:%S}", tm);
                auto drawList = ImGui::GetWindowDrawList();
                const auto center = ImGui::GetWindowPos() + ImVec2(0, ImGui::GetTextLineHeightWithSpacing()) + size / 2;

                // Draw clock face
                drawList->AddCircle(center, size.x / 2, ImGui::GetColorU32(ImGuiCol_TextDisabled), 0);

                auto sectionPos = [](float i) {
                    return ImVec2(std::sin(-i * 30.0F * std::numbers::pi / 180.0F + std::numbers::pi / 2), std::cos(-i * 30.0F * std::numbers::pi / 180.0F + std::numbers::pi / 2));
                };

                // Draw clock sections and numbers
                for (u8 i = 0; i < 12; ++i) {
                    auto text = hex::format("{}", (((i + 2) % 12) + 1));
                    drawList->AddLine(center + sectionPos(i) * size / 2.2, center + sectionPos(i) * size / 2, ImGui::GetColorU32(ImGuiCol_TextDisabled), 1_scaled);
                    drawList->AddText(center + sectionPos(i) * size / 3 - ImGui::CalcTextSize(text.c_str()) / 2, ImGui::GetColorU32(ImGuiCol_Text), text.c_str());
                }

                // Draw hour hand
                drawList->AddLine(center, center + sectionPos((tm.tm_hour + 9) % 12 + float(tm.tm_min) / 60.0) * size / 3.5, ImGui::GetColorU32(ImGuiCol_TextDisabled), 3_scaled);

                // Draw minute hand
                drawList->AddLine(center, center + sectionPos((float(tm.tm_min) / 5.0F) - 3)  * size / 2.5, ImGui::GetColorU32(ImGuiCol_TextDisabled), 3_scaled);

                // Draw second hand
                drawList->AddLine(center, center + sectionPos((float(tm.tm_sec) / 5.0F) - 3)  * size / 2.5, ImGuiExt::GetCustomColorU32(ImGuiCustomCol_ToolbarRed), 2_scaled);
            }
            ImGui::EndChild();

            ImGui::EndTable();
        }
    }

}