#include <hex/helpers/http_requests.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/api/localization.hpp>

#include <hex/ui/view.hpp>

#include <regex>

#include <content/tools_entries.hpp>

#include <imgui.h>

#include <hex/ui/imgui_imhex_extensions.h>

namespace hex::plugin::builtin {
    namespace impl {
        void drawRegexReplacer() {
            static auto regexInput     = [] { std::string s; s.reserve(0xFFF); return s; }();
            static auto regexPattern   = [] { std::string s; s.reserve(0xFFF); return s; }();
            static auto replacePattern = [] { std::string s; s.reserve(0xFFF); return s; }();
            static auto regexOutput    = [] { std::string s; s.reserve(0xFFF); return s; }();

            ImGui::PushItemWidth(-150_scaled);
            bool changed1 = ImGuiExt::InputTextIcon("hex.builtin.tools.regex_replacer.pattern"_lang, ICON_VS_REGEX, regexPattern);
            bool changed2 = ImGuiExt::InputTextIcon("hex.builtin.tools.regex_replacer.replace"_lang, ICON_VS_REGEX, replacePattern);
            bool changed3 = ImGui::InputTextMultiline("hex.builtin.tools.regex_replacer.input"_lang, regexInput, ImVec2(0, 0));

            if (changed1 || changed2 || changed3) {
                try {
                    regexOutput = std::regex_replace(regexInput.data(), std::regex(regexPattern.data()), replacePattern.data());
                } catch (std::regex_error &) { }
            }

            ImGui::InputTextMultiline("hex.builtin.tools.regex_replacer.output"_lang, regexOutput.data(), regexOutput.size(), ImVec2(0, 0), ImGuiInputTextFlags_ReadOnly);

            ImGui::PopItemWidth();
        }
    }
}
