#include <hex/helpers/utils.hpp>
#include <hex/api/localization.hpp>

#include <llvm/Demangle/Demangle.h>

#include <imgui.h>

#include <hex/ui/imgui_imhex_extensions.h>

namespace hex::plugin::builtin {
    namespace impl {
        void drawDemangler() {
            static std::string mangledName, demangledName;

            if (ImGui::InputTextWithHint("hex.builtin.tools.demangler.mangled"_lang, "Itanium, MSVC, Dlang & Rust", mangledName)) {
                demangledName = llvm::demangle(mangledName);

                if (demangledName == mangledName) {
                    demangledName = "???";
                }
            }

            ImGui::Header("hex.builtin.tools.demangler.demangled"_lang);
            if (ImGui::BeginChild("demangled", ImVec2(0, 200_scaled), true)) {
                ImGui::TextFormattedWrappedSelectable("{}", demangledName);
            }
            ImGui::EndChild();
        }
    }
}
