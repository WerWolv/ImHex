#include <hex/ui/popup.hpp>

#include <hex/api/localization.hpp>

#include <functional>
#include <string>

namespace hex::plugin::builtin {

    namespace impl {

        template<typename T>
        class PopupNotification : public Popup<T> {
        public:
            PopupNotification(std::string unlocalizedName, std::string message, std::function<void()> function)
                : hex::Popup<T>(std::move(unlocalizedName), false),
                  m_message(std::move(message)), m_function(std::move(function)) { }

            void drawContent() override {
                ImGui::TextFormattedWrapped("{}", this->m_message.c_str());
                ImGui::NewLine();
                ImGui::Separator();
                if (ImGui::Button("hex.builtin.common.okay"_lang) || ImGui::IsKeyDown(ImGuiKey_Escape))
                    this->m_function();

                ImGui::SetWindowPos((ImHexApi::System::getMainWindowSize() - ImGui::GetWindowSize()) / 2, ImGuiCond_Appearing);
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
        : PopupNotification("hex.builtin.common.info", std::move(message), [this]() {
            Popup::close();
        }) { }
    };

    class PopupError : public impl::PopupNotification<PopupError> {
    public:
        explicit PopupError(std::string message)
        : PopupNotification("hex.builtin.common.error", std::move(message), [this]() {
            Popup::close();
        }) { }
    };

    class PopupFatal : public impl::PopupNotification<PopupFatal> {
    public:
        explicit PopupFatal(std::string message)
        : PopupNotification("hex.builtin.common.fatal", std::move(message), [this]() {
            ImHexApi::System::closeImHex();
            Popup::close();
        }) { }
    };


}