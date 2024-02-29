#pragma once

#include <hex/ui/popup.hpp>

#include <hex/api/localization_manager.hpp>

#include <string>

namespace hex::plugin::builtin {

    class PopupCrashRecovered : public Popup<PopupCrashRecovered> {
    public:
        PopupCrashRecovered(const std::exception &e)
            : hex::Popup<PopupCrashRecovered>("hex.builtin.popup.crash_recover.title", false), m_error(e) { }

        void drawContent() override {
            if (ImGui::Button("hex.ui.common.okay"_lang)) {
                this->close();
            }
        }

        [[nodiscard]] ImGuiWindowFlags getFlags() const override {
            return ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove;
        }
    private:
        std::exception m_error;
    };

}