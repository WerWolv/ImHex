#pragma once

#include <hex/api/imhex_api.hpp>
#include <hex/ui/imgui_imhex_extensions.h>
#include <hex/ui/toast.hpp>

#include <fonts/codicons_font.h>
#include <hex/helpers/utils.hpp>

namespace hex::plugin::builtin::ui {

    namespace impl {

        template<typename T>
        struct ToastNotification : Toast<T> {
            ToastNotification(ImColor color, const char *icon, UnlocalizedString title, UnlocalizedString message)
                : Toast<T>(color), m_icon(icon), m_title(std::move(title)), m_message(std::move(message)) {}

            void drawContent() final {
                ImGuiExt::TextFormattedColored(this->getColor(), "{}", m_icon);
                ImGui::SameLine();
                ImGui::PushFont(ImHexApi::Fonts::Bold());
                {
                    ImGuiExt::TextFormatted("{}", hex::limitStringLength(Lang(m_title).get(), 30));
                }
                ImGui::PopFont();

                ImGui::Separator();

                ImGuiExt::TextFormattedWrapped("{}", hex::limitStringLength(Lang(m_message).get(), 60));
            }

        private:
            const char *m_icon;
            UnlocalizedString m_title, m_message;
        };

    }

    struct ToastInfo : impl::ToastNotification<ToastInfo> {
        ToastInfo(UnlocalizedString title, UnlocalizedString message)
            : ToastNotification(ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_LoggerInfo), ICON_VS_INFO, std::move(title), std::move(message)) {}
    };

    struct ToastWarn : impl::ToastNotification<ToastWarn> {
        ToastWarn(UnlocalizedString title, UnlocalizedString message)
            : ToastNotification(ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_LoggerWarning), ICON_VS_WARNING, std::move(title), std::move(message)) {}
    };

    struct ToastError : impl::ToastNotification<ToastError> {
        ToastError(UnlocalizedString title, UnlocalizedString message)
            : ToastNotification(ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_LoggerError), ICON_VS_ERROR, std::move(title), std::move(message)) {}
    };

}
