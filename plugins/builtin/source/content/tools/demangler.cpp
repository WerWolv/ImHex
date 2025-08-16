#include <hex/api/localization_manager.hpp>
#include <hex/helpers/scaling.hpp>
#include <hex/trace/stacktrace.hpp>

#include <imgui.h>
#include <ui/text_editor.hpp>
#include <hex/ui/imgui_imhex_extensions.h>

namespace hex::plugin::builtin {

    void drawDemangler() {
        static std::string mangledName, demangledName, wrappedDemangledName;
        static ui::TextEditor outputField = []{
            ui::TextEditor editor;
            editor.setReadOnly(true);
            editor.setShowLineNumbers(false);
            editor.setShowWhitespaces(false);
            editor.setShowCursor(false);
            editor.setImGuiChildIgnored(true);

            auto languageDef = ui::TextEditor::LanguageDefinition::CPlusPlus();
            for (auto &[name, identifier] : languageDef.m_identifiers)
                identifier.m_declaration = "";

            editor.setLanguageDefinition(languageDef);

            return editor;
        }();
        static float prevWindowWidth;

        if (ImGui::InputTextWithHint("hex.builtin.tools.demangler.mangled"_lang, "Itanium, MSVC, Dlang & Rust", mangledName)) {
            demangledName = trace::demangle(mangledName);

            if (demangledName == mangledName) {
                demangledName = "???";
            }

            prevWindowWidth = 0;
        }

        const auto windowWidth = ImGui::GetContentRegionAvail().x;
        if (prevWindowWidth != windowWidth) {
            wrappedDemangledName = wolv::util::wrapMonospacedString(
                demangledName,
                ImGui::CalcTextSize("M").x,
                ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ScrollbarSize - ImGui::GetStyle().FrameBorderSize
            );

            outputField.setText(wrappedDemangledName);
            prevWindowWidth = windowWidth;
        }

        ImGuiExt::Header("hex.builtin.tools.demangler.demangled"_lang);

        if (ImGui::BeginChild("Demangled", ImVec2(ImGui::GetContentRegionAvail().x, 150_scaled), true, ImGuiWindowFlags_NoMove)) {
            outputField.render("Demangled", ImVec2(ImGui::GetContentRegionAvail().x, 150_scaled), true);
        }
        ImGui::EndChild();
    }

}
