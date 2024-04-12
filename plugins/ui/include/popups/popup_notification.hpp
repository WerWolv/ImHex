#pragma once

#include <hex/ui/popup.hpp>

#include <hex/api/localization_manager.hpp>
#include <hex/api/imhex_api.hpp>

#include <functional>
#include <string>

namespace hex::ui {

    namespace impl {

        template<typename T>
        class PopupNotification : public Popup<T> {
        public:
            PopupNotification(UnlocalizedString unlocalizedName, std::string message, std::function<void()> function)
                : hex::Popup<T>(std::move(unlocalizedName), false),
                  m_message(std::move(message)), m_function(std::move(function)) { }

            void drawContent() override {
                ImGuiExt::TextFormattedWrapped("{}", m_message.c_str());
                ImGui::NewLine();
                ImGui::Separator();
                if (ImGui::Button("hex.ui.common.okay"_lang) || ImGui::IsKeyDown(ImGuiKey_Escape))
                    m_function();

                ImGui::SetWindowPos((ImHexApi::System::getMainWindowSize() - ImGui::GetWindowSize()) / 2, ImGuiCond_Appearing);

                if (ImGui::IsKeyPressed(ImGuiKey_Escape))
                    this->close();
            }

            [[nodiscard]] ImGuiWindowFlags getFlags() const override {
                return ImGuiWindowFlags_AlwaysAutoResize;
            }

            [[nodiscard]] ImVec2 getMinSize() const override {
                return scaled({ 400, 100 });
            }

            [[nodiscard]] ImVec2 getMaxSize() const override {
                return scaled({ 600, 300 });
            }

        private:
            std::string m_message;
            std::function<void()> m_function;
        };

    }

    class PopupInfo : public impl::PopupNotification<PopupInfo> {
    public:
        explicit PopupInfo(std::string message)
        : PopupNotification("hex.ui.common.info", std::move(message), [this] {
            Popup::close();
        }) { }
    };

    class PopupWarning : public impl::PopupNotification<PopupWarning> {
    public:
        explicit PopupWarning(std::string message)
        : PopupNotification("hex.ui.common.warning", std::move(message), [this] {
            Popup::close();
        }) { }
    };

    class PopupError : public impl::PopupNotification<PopupError> {
    public:
        explicit PopupError(std::string message)
        : PopupNotification("hex.ui.common.error", std::move(message), [this] {
            Popup::close();
        }) { }
    };

    class PopupFatal : public impl::PopupNotification<PopupFatal> {
    public:
        explicit PopupFatal(std::string message)
        : PopupNotification("hex.ui.common.fatal", std::move(message), [this] {
            ImHexApi::System::closeImHex();
            Popup::close();
        }) { }
    };


}