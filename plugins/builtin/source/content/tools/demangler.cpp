#include <hex/helpers/utils.hpp>
#include <hex/api/localization_manager.hpp>

#include <content/helpers/demangle.hpp>

#include <imgui.h>
#include <hex/ui/imgui_imhex_extensions.h>
#include <TextEditor.h>


namespace hex::plugin::builtin {

    void drawDemangler() {
        static std::string mangledName, demangledName, wrappedDemangledName;
        static TextEditor outputField = []{
            TextEditor editor;
            editor.SetReadOnly(true);
            editor.SetShowLineNumbers(false);
            editor.SetShowWhitespaces(false);
            editor.SetShowCursor(false);
            editor.SetImGuiChildIgnored(true);

            auto languageDef = TextEditor::LanguageDefinition::CPlusPlus();
            for (auto &[name, identifier] : languageDef.mIdentifiers)
                identifier.mDeclaration = "";

            editor.SetLanguageDefinition(languageDef);

            return editor;
        }();
        static float prevWindowWidth;

        if (ImGui::InputTextWithHint("hex.builtin.tools.demangler.mangled"_lang, "Itanium, MSVC, Dlang & Rust", mangledName)) {
            demangledName = hex::plugin::builtin::demangle(mangledName);

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

            outputField.SetText(wrappedDemangledName);
            prevWindowWidth = windowWidth;
        }

        ImGuiExt::Header("hex.builtin.tools.demangler.demangled"_lang);

        if (ImGui::BeginChild("Demangled", ImVec2(ImGui::GetContentRegionAvail().x, 150_scaled), true, ImGuiWindowFlags_NoMove)) {
            outputField.Render("Demangled", ImVec2(ImGui::GetContentRegionAvail().x, 150_scaled), true);
        }
        ImGui::EndChild();
    }

}
