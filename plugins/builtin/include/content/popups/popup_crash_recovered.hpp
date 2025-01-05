#pragma once

#include <hex/ui/popup.hpp>

#include <hex/api/localization_manager.hpp>

#include <llvm/Demangle/Demangle.h>

#include <string>

namespace hex::plugin::builtin {

    class PopupCrashRecovered : public Popup<PopupCrashRecovered> {
    public:
        PopupCrashRecovered(const std::exception &e)
            : hex::Popup<PopupCrashRecovered>("hex.builtin.popup.crash_recover.title", false),
              m_errorType(typeid(e).name()),
              m_errorMessage(e.what()) { }

        void drawContent() override {
            ImGuiExt::TextFormattedWrapped("hex.builtin.popup.crash_recover.message"_lang);

            ImGuiExt::TextFormattedWrapped(hex::format("Error: {}: {}", llvm::demangle(this->m_errorType), this->m_errorMessage));

            if (ImGui::Button("hex.ui.common.okay"_lang)) {
                this->close();
            }
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
        std::string m_errorType, m_errorMessage;
    };

}