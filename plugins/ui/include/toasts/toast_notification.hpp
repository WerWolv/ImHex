#pragma once

#include <hex/ui/imgui_imhex_extensions.h>
#include <hex/ui/toast.hpp>

#include <fonts/vscode_icons.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/helpers/logger.hpp>

#include <popups/popup_notification.hpp>

namespace hex::ui {

    namespace impl {

        template<typename T>
        struct ToastNotification : Toast<T> {
            ToastNotification(ImColor color, const char *icon, UnlocalizedString unlocalizedTitle, std::string message)
                : Toast<T>(color), m_icon(icon), m_unlocalizedTitle(std::move(unlocalizedTitle)), m_message(std::move(message)) {}

            void drawContent() final {
                if (ImGui::IsWindowHovered()) {
                    if (ImGui::BeginTooltip()) {
                        ImGuiExt::Header(Lang(m_unlocalizedTitle), true);
                        ImGui::PushTextWrapPos(300_scaled);
                        ImGui::TextUnformatted(m_message.c_str());
                        ImGui::PopTextWrapPos();
                        ImGui::EndTooltip();
                    }
                }

                ImGuiExt::TextFormattedColored(this->getColor(), "{}", m_icon);
                ImGui::SameLine();

                ImGuiExt::TextFormatted("{}", hex::limitStringLength(Lang(m_unlocalizedTitle).get(), 30));

                ImGui::Separator();

                ImGuiExt::TextFormattedWrapped("{}", hex::limitStringLength(m_message, 60));
            }

        private:
            const char *m_icon;
            UnlocalizedString m_unlocalizedTitle;
            std::string m_message;
        };

    }

    struct ToastInfo : impl::ToastNotification<ToastInfo> {
        explicit ToastInfo(std::string message)
            : ToastNotification(ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_LoggerInfo), ICON_VS_INFO, "hex.ui.common.info", message) {
            log::info("{}", message);
        }
    };

    struct ToastWarning : impl::ToastNotification<ToastWarning> {
        explicit ToastWarning(std::string message)
            : ToastNotification(ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_LoggerWarning), ICON_VS_WARNING, "hex.ui.common.warning", message) {
            log::warn("{}", message);
        }
    };

    struct ToastError : impl::ToastNotification<ToastError> {
        explicit ToastError(std::string message)
            : ToastNotification(ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_LoggerError), ICON_VS_ERROR, "hex.ui.common.error", message) {
            log::error("{}", message);
        }
    };

}
