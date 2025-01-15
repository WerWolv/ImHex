#pragma once

#include <hex/ui/banner.hpp>

#include <imgui.h>
#include <hex/ui/imgui_imhex_extensions.h>

namespace hex::ui {

    class BannerButton : public Banner<BannerButton> {
    public:
        BannerButton(const char *icon, UnlocalizedString message, ImColor color, UnlocalizedString buttonText, std::function<void()> buttonCallback)
            : Banner(color), m_icon(icon), m_message(std::move(message)), m_buttonText(std::move(buttonText)), m_buttonCallback(std::move(buttonCallback)) { }

        void drawContent() override {
            const std::string buttonText = Lang(m_buttonText);
            const auto buttonSize = ImGui::CalcTextSize(buttonText.c_str());

            ImGui::TextUnformatted(m_icon);
            ImGui::SameLine(0, 10_scaled);

            const std::string message = Lang(m_message);
            const auto messageSize = ImGui::CalcTextSize(message.c_str());
            ImGuiExt::TextFormatted("{}", limitStringLength(message, message.size() * ((ImGui::GetContentRegionAvail().x - buttonSize.x - 40_scaled) / messageSize.x)));
            if (ImGui::IsItemHovered()) {
                ImGui::SetNextWindowSize(ImVec2(400_scaled, 0));
                if (ImGui::BeginTooltip()) {
                    ImGuiExt::TextFormattedWrapped("{}", message);
                    ImGui::EndTooltip();
                }
            }

            ImGui::SameLine();

            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - buttonSize.x - 20_scaled);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2_scaled);
            if (ImGui::SmallButton(buttonText.c_str())) {
                m_buttonCallback();
                this->close();
            }
            ImGui::PopStyleVar(1);
        }

    private:
        const char *m_icon;
        UnlocalizedString m_message;
        UnlocalizedString m_buttonText;
        std::function<void()> m_buttonCallback;
    };

}
