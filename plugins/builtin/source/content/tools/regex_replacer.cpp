#include <hex/helpers/http_requests.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/api/localization_manager.hpp>

#include <boost/regex.hpp>

#include <imgui.h>
#include <hex/ui/imgui_imhex_extensions.h>

#include <fonts/vscode_icons.hpp>

namespace hex::plugin::builtin {

    void drawRegexReplacer() {
        static std::string inputString;
        static std::string regexPattern;
        static std::string replacePattern;
        static std::string outputString;

        ImGui::PushItemWidth(-150_scaled);
        bool changed1 = ImGuiExt::InputTextIcon("hex.builtin.tools.regex_replacer.pattern"_lang, ICON_VS_REGEX, regexPattern);
        bool changed2 = ImGuiExt::InputTextIcon("hex.builtin.tools.regex_replacer.replace"_lang, ICON_VS_REGEX, replacePattern);
        bool changed3 = ImGui::InputTextMultiline("hex.builtin.tools.regex_replacer.input"_lang, inputString, ImVec2(0, 0));

        if (changed1 || changed2 || changed3) {
            try {
                const auto regex = boost::regex(regexPattern);
                outputString = boost::regex_replace(inputString, regex, replacePattern);
            } catch (boost::regex_error &) { }
        }

        ImGui::InputTextMultiline("hex.builtin.tools.regex_replacer.output"_lang, outputString.data(), outputString.size(), ImVec2(0, 0), ImGuiInputTextFlags_ReadOnly);

        ImGui::PopItemWidth();
    }

}
