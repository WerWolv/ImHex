#include <ui/pattern_value_editor.hpp>
#include <ui/control_byte_picture.hpp>
#include <imgui.h>
#include <hex/helpers/utils.hpp>
#include <hex/helpers/logger.hpp>
#include <hex/helpers/string_codec.hpp>
#include <hex/ui/imgui_imhex_extensions.h>

#include <wolv/math_eval/math_evaluator.hpp>

#include <pl/patterns/pattern_array_dynamic.hpp>
#include <pl/patterns/pattern_array_static.hpp>
#include <pl/patterns/pattern_bitfield.hpp>
#include <pl/patterns/pattern_boolean.hpp>
#include <pl/patterns/pattern_character.hpp>
#include <pl/patterns/pattern_enum.hpp>
#include <pl/patterns/pattern_float.hpp>
#include <pl/patterns/pattern_pointer.hpp>
#include <pl/patterns/pattern_signed.hpp>
#include <pl/patterns/pattern_string.hpp>
#include <pl/patterns/pattern_struct.hpp>
#include <pl/patterns/pattern_union.hpp>
#include <pl/patterns/pattern_unsigned.hpp>
#include <pl/patterns/pattern_wide_character.hpp>
#include <pl/patterns/pattern_wide_string.hpp>

namespace hex::ui {

    /**
     * @brief Sets a pattern's value, catching an exception that would otherwise crash the application
     *
     * A write formatter can reject input, or a string can fail to encode a
     * character its encoding has no byte for. An uncaught exception here
     * would cross a BeginTable()/EndTable() pair and corrupt ImGui's state.
     *
     * @param pattern The pattern to set
     * @param value The value to set it to
     */
    static void trySetValue(pl::ptrn::Pattern &pattern, const pl::core::Token::Literal &value) {
        try {
            pattern.setValue(value);
        } catch (const std::exception &e) {
            log::error("Failed to set pattern value: {}", e.what());
        }
    }

    /**
     * @brief Force-writes a string pattern's value, substituting anything its encoding cannot represent
     * @param pattern The string pattern to set
     * @param value The value to set it to
     */
    static void trySetValueLossy(pl::ptrn::PatternString &pattern, const std::string &value) {
        try {
            pattern.setValueLossy(value);
        } catch (const std::exception &e) {
            log::error("Failed to set pattern value: {}", e.what());
        }
    }

    void PatternValueEditor::resetEditing() {
        m_editingValuePattern = nullptr;
        m_editingValue.clear();
        m_hasUnencodableChar = false;
    }

    void PatternValueEditor::cancelIfDeactivated(bool submitted) {
        if (!submitted && ImGui::IsItemDeactivated())
            m_onEditCallback();
    }

    void PatternValueEditor::visit(pl::ptrn::PatternArrayDynamic& pattern) {
        std::ignore = pattern;
    }

    void PatternValueEditor::visit(pl::ptrn::PatternArrayStatic& pattern) {
        std::ignore = pattern;
    }

    void PatternValueEditor::visit(pl::ptrn::PatternBitfield& pattern) {
        std::ignore = pattern;
    }

    void PatternValueEditor::visit(pl::ptrn::PatternBitfieldField& pattern) {
        auto value = pattern.getValue();
        auto valueString = pattern.toString();

        if (const auto *enumPattern = dynamic_cast<pl::ptrn::PatternBitfieldFieldEnum*>(&pattern); enumPattern != nullptr) {
            if (ImGui::BeginCombo("##Enum", pattern.getFormattedValue().c_str())) {
                auto currValue = pattern.getValue().toUnsigned();
                for (auto &[name, enumValue] : enumPattern->getEnumValues()) {
                    auto min = enumValue.min.toUnsigned();
                    auto max = enumValue.max.toUnsigned();

                    bool isSelected = min <= currValue && max >= currValue;
                    if (ImGui::Selectable(fmt::format("{}::{}", pattern.getTypeName(), name, min, pattern.getSize() * 2).c_str(), isSelected)) {
                        trySetValue(pattern, enumValue.min);
                        m_onEditCallback();
                    }
                    if (isSelected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        } else if (dynamic_cast<pl::ptrn::PatternBitfieldFieldBoolean*>(&pattern) != nullptr) {
            bool boolValue = value.toBoolean();
            if (ImGui::Checkbox("##boolean", &boolValue)) {
                trySetValue(pattern, boolValue);
            }
        } else if (std::holds_alternative<i128>(value)) {
            const bool submitted = ImGui::InputText("##Value", valueString, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue);
            if (submitted) {
                if (pattern.getWriteFormatterFunction().empty()) {
                    wolv::math_eval::MathEvaluator<i128> mathEvaluator;

                    if (auto result = mathEvaluator.evaluate(valueString); result.has_value())
                        trySetValue(pattern, result.value());
                } else {
                    trySetValue(pattern, valueString);
                }

                m_onEditCallback();
            }
            cancelIfDeactivated(submitted);
        } else if (std::holds_alternative<u128>(value)) {
            const bool submitted = ImGui::InputText("##Value", valueString, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue);
            if (submitted) {
                if (pattern.getWriteFormatterFunction().empty()) {
                    wolv::math_eval::MathEvaluator<u128> mathEvaluator;

                    if (auto result = mathEvaluator.evaluate(valueString); result.has_value())
                        trySetValue(pattern, result.value());
                } else {
                    trySetValue(pattern, valueString);
                }


                m_onEditCallback();
            }
            cancelIfDeactivated(submitted);
        }
    }

    void PatternValueEditor::visit(pl::ptrn::PatternBitfieldArray& pattern) {
        std::ignore = pattern;
    }

    void PatternValueEditor::visit(pl::ptrn::PatternBoolean& pattern) {
        bool value = pattern.getValue().toBoolean();
        if (ImGui::Checkbox("##boolean", &value)) {
            trySetValue(pattern, value);
            m_onEditCallback();
        }
    }

    void PatternValueEditor::visit(pl::ptrn::PatternCharacter& pattern) {
        auto value = hex::encodeByteString(pattern.getBytes());
        const bool submitted = ImGui::InputText("##Character", value.data(), value.size() + 1, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue);
        if (submitted) {
            if (!value.empty()) {
                auto result = hex::decodeByteString(value);
                if (result.has_value() && !result->empty())
                    trySetValue(pattern, char((*result)[0]));

                m_onEditCallback();
            }
        }
        cancelIfDeactivated(submitted);
    }

    void PatternValueEditor::visit(pl::ptrn::PatternEnum& pattern) {
        if (ImGui::BeginCombo("##Enum", pattern.getFormattedValue().c_str())) {
            auto currValue = pattern.getValue().toUnsigned();
            for (auto &[name, enumValue] : pattern.getEnumValues()) {
                auto min = enumValue.min.toUnsigned();
                auto max = enumValue.max.toUnsigned();

                bool isSelected = min <= currValue && max >= currValue;
                if (ImGui::Selectable(fmt::format("{}::{}", pattern.getTypeName(), name, min, pattern.getSize() * 2).c_str(), isSelected)) {
                    trySetValue(pattern, enumValue.min);
                    m_onEditCallback();
                }
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }

    void PatternValueEditor::visit(pl::ptrn::PatternFloat& pattern) {
        auto value = pattern.toString();
        const bool submitted = ImGui::InputText("##Value", value, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue);
        if (submitted) {
            if (pattern.getWriteFormatterFunction().empty()) {
                wolv::math_eval::MathEvaluator<long double> mathEvaluator;

                if (auto result = mathEvaluator.evaluate(value); result.has_value())
                    trySetValue(pattern, double(result.value()));
            } else {
                trySetValue(pattern, value);
            }

            m_onEditCallback();
        }
        cancelIfDeactivated(submitted);
    }

    void PatternValueEditor::visit(pl::ptrn::PatternPadding& pattern) {
        std::ignore = pattern;
    }

    void PatternValueEditor::visit(pl::ptrn::PatternPointer& pattern) {
        std::ignore = pattern;
    }

    void PatternValueEditor::visit(pl::ptrn::PatternSigned& pattern) {
        auto value = pattern.getFormattedValue();
        const bool submitted = ImGui::InputText("##Value", value, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue);
        if (submitted) {
            if (pattern.getWriteFormatterFunction().empty()) {
                wolv::math_eval::MathEvaluator<i128> mathEvaluator;

                if (auto result = mathEvaluator.evaluate(value); result.has_value())
                    trySetValue(pattern, result.value());
            } else {
                trySetValue(pattern, value);
            }

            m_onEditCallback();
        }
        cancelIfDeactivated(submitted);
    }

    void PatternValueEditor::visit(pl::ptrn::PatternString& pattern) {
        // Re-decode only when editing starts on a different field, so a rejected edit stays in place.
        if (m_editingValuePattern != &pattern) {
            m_editingValuePattern = &pattern;
            m_hasUnencodableChar = false;
            m_editingValue.clear();
            try {
                // toString() can throw on bytes not valid under the field's encoding.
                m_editingValue = nulToPicture(pattern.toString());
            } catch (const std::exception &e) {
                log::error("Failed to decode pattern value: {}", e.what());
            }
        }
        std::string &value = m_editingValue;

        // Sized for UTF-8 bytes of the typed text, not the target encoding; getBytesOf() caps the actual write.
        const auto bufferSize = std::max(value.size(), pattern.getSize() * 4) + 1;
        value.resize(bufferSize - 1, '\0');

        const auto encodingName = pattern.getEncodingName();

        struct CallbackData {
            std::string encodingName;
            bool *hasUnencodableChar;
        } callbackData { encodingName, &m_hasUnencodableChar };

        // Captured before InputText's callback below can change m_hasUnencodableChar mid-call.
        const bool borderPushed = m_hasUnencodableChar;
        if (borderPushed) {
            ImGui::PushStyleColor(ImGuiCol_Border, ImGuiExt::GetCustomColorU32(ImGuiCustomCol_LoggerError));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1_scaled);
        }

        const bool submitted = ImGui::InputText("##Value", value.data(), bufferSize,
            ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackEdit,
            [](ImGuiInputTextCallbackData *data) -> int {
                auto &callbackData = *static_cast<CallbackData*>(data->UserData);
                auto text = pictureToNul(std::string_view(data->Buf, size_t(data->BufTextLen)));
                auto encoded = PatternLanguageStringCodec().encode(text, callbackData.encodingName);
                *callbackData.hasUnencodableChar = !encoded.has_value();
                return 0;
            }, &callbackData);

        if (borderPushed) {
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
        }

        // Shift+Enter force-writes, substituting for anything the encoding cannot represent.
        if (submitted) {
            value = pictureToNul(value.c_str());

            if (ImGui::GetIO().KeyShift) {
                trySetValueLossy(pattern, value);
                m_hasUnencodableChar = false;
                m_editingValuePattern = nullptr;
                m_onEditCallback();
            } else if (auto encoded = PatternLanguageStringCodec().encode(value, encodingName); encoded.has_value()) {
                trySetValue(pattern, value);
                m_hasUnencodableChar = false;
                m_editingValuePattern = nullptr;
                m_onEditCallback();
            } else {
                m_hasUnencodableChar = true;

                // Reclaim focus, since Enter deactivates the field even on a rejected commit.
                ImGui::SetKeyboardFocusHere(-1);
            }
        } else if (ImGui::IsItemDeactivated()) {
            m_hasUnencodableChar = false;
            m_editingValuePattern = nullptr;
            m_onEditCallback();
        }
    }

    void PatternValueEditor::visit(pl::ptrn::PatternStruct& pattern) {
        // A nested PatternString member can throw here too; see visit(PatternString&) above.
        std::string value;
        try {
            value = pattern.toString();
        } catch (const std::exception &e) {
            log::error("Failed to decode pattern value: {}", e.what());
        }
        const bool submitted = ImGui::InputText("##Value", value, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue);
        if (submitted) {
            trySetValue(pattern, value);
            m_onEditCallback();
        }
        cancelIfDeactivated(submitted);
    }

    void PatternValueEditor::visit(pl::ptrn::PatternUnion& pattern) {
        // See the note in visit(PatternStruct&) above.
        std::string value;
        try {
            value = pattern.toString();
        } catch (const std::exception &e) {
            log::error("Failed to decode pattern value: {}", e.what());
        }
        const bool submitted = ImGui::InputText("##Value", value, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue);
        if (submitted) {
            trySetValue(pattern, value);
            m_onEditCallback();
        }
        cancelIfDeactivated(submitted);
    }

    void PatternValueEditor::visit(pl::ptrn::PatternUnsigned& pattern) {
        auto value = pattern.toString();
        const bool submitted = ImGui::InputText("##Value", value, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue);
        if (submitted) {
            if (pattern.getWriteFormatterFunction().empty()) {
                wolv::math_eval::MathEvaluator<u128> mathEvaluator;

                if (auto result = mathEvaluator.evaluate(value); result.has_value())
                    trySetValue(pattern, result.value());
            } else {
                trySetValue(pattern, value);
            }

            m_onEditCallback();
        }
        cancelIfDeactivated(submitted);
    }

    void PatternValueEditor::visit(pl::ptrn::PatternWideCharacter& pattern) {
        std::ignore = pattern;
    }

    void PatternValueEditor::visit(pl::ptrn::PatternWideString& pattern) {
        std::ignore = pattern;
    }

    void PatternValueEditor::visit(pl::ptrn::PatternError& pattern) {
        std::ignore = pattern;
    }

    void PatternValueEditor::visit(pl::ptrn::Pattern& pattern) {
        std::ignore = pattern;
    }


}