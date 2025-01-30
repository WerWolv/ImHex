#pragma once

#include <hex/ui/banner.hpp>
#include <imgui.h>

namespace hex::ui {

    class BannerIcon : public Banner<BannerIcon> {
    public:
        BannerIcon(const char *icon, UnlocalizedString message, ImColor color)
                : Banner(color), m_icon(icon), m_message(std::move(message)) { }

        void drawContent() override {
            auto textHeight = std::max(ImGui::CalcTextSize(Lang(m_message)).y, ImGui::CalcTextSize(m_icon).y);
            auto textOffset = (ImGui::GetWindowHeight() - textHeight) / 2;
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + textOffset);
            ImGui::TextUnformatted(m_icon);
            ImGui::SameLine(0, 10_scaled);
            ImGui::TextUnformatted(Lang(m_message));
        }

    private:
        const char *m_icon;
        UnlocalizedString m_message;
    };

}