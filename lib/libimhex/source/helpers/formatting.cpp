#include <string>

#include <hex/helpers/formatting.hpp>
#include <wolv/utils/string.hpp>

#include <hex/api/content_registry/settings.hpp>

namespace hex {
    namespace {
        ContentRegistry::Settings::SettingsVariable<bool, "hex.builtin.setting.pattern_editor", "hex.builtin.setting.pattern_editor.save_tabs"> m_formattingSaveTabs = false;
        ContentRegistry::Settings::SettingsVariable<bool, "hex.builtin.setting.pattern_editor", "hex.builtin.setting.pattern_editor.trim_whitespace"> m_formattingTrimWhitespace = false;
        ContentRegistry::Settings::SettingsVariable<bool, "hex.builtin.setting.pattern_editor", "hex.builtin.setting.pattern_editor.final_newline"> m_formattingFinalNewline = false;
    }

    [[nodiscard]] std::string formatPattern(const std::string &code, const u32 tabSize) {
        const auto shouldConvertToTabs = m_formattingSaveTabs.get();
        const auto trimWhitespace = m_formattingTrimWhitespace.get();
        const auto insertFinalNewline = m_formattingFinalNewline.get();
        if (!shouldConvertToTabs && !trimWhitespace && !insertFinalNewline) {
            return code;
        }

        auto formattedCode = code;

        if (shouldConvertToTabs) { // spaces -> tabs
            formattedCode = wolv::util::replaceSpacesWithTabs(code, tabSize, trimWhitespace);
        }

        if (insertFinalNewline && !formattedCode.ends_with('\n')) {
            formattedCode += '\n';
        }

        return formattedCode;
    }

    [[nodiscard]] std::string preprocessPattern(const std::string &code, const u32 tabSize) {
        // todo: trim trailing whitespace
        return wolv::util::preprocessText(code, tabSize);
    }
}