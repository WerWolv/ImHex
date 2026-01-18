#include <ui/pattern_value_editor.hpp>
#include <imgui.h>
#include <hex/helpers/utils.hpp>
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
                        pattern.setValue(enumValue.min);
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
                pattern.setValue(boolValue);
            }
        } else if (std::holds_alternative<i128>(value)) {
            if (ImGui::InputText("##Value", valueString, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue)) {
                if (pattern.getWriteFormatterFunction().empty()) {
                    wolv::math_eval::MathEvaluator<i128> mathEvaluator;

                    if (auto result = mathEvaluator.evaluate(valueString); result.has_value())
                        pattern.setValue(result.value());
                } else {
                    pattern.setValue(valueString);
                }

                m_onEditCallback();
            }
        } else if (std::holds_alternative<u128>(value)) {
            if (ImGui::InputText("##Value", valueString, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue)) {
                if (pattern.getWriteFormatterFunction().empty()) {
                    wolv::math_eval::MathEvaluator<u128> mathEvaluator;

                    if (auto result = mathEvaluator.evaluate(valueString); result.has_value())
                        pattern.setValue(result.value());
                } else {
                    pattern.setValue(valueString);
                }


                m_onEditCallback();
            }
        }
    }

    void PatternValueEditor::visit(pl::ptrn::PatternBitfieldArray& pattern) {
        std::ignore = pattern;
    }

    void PatternValueEditor::visit(pl::ptrn::PatternBoolean& pattern) {
        bool value = pattern.getValue().toBoolean();
        if (ImGui::Checkbox("##boolean", &value)) {
            pattern.setValue(value);
            m_onEditCallback();
        }
    }

    void PatternValueEditor::visit(pl::ptrn::PatternCharacter& pattern) {
        auto value = hex::encodeByteString(pattern.getBytes());
        if (ImGui::InputText("##Character", value.data(), value.size() + 1, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue)) {
            if (!value.empty()) {
                auto result = hex::decodeByteString(value);
                if (!result.empty())
                    pattern.setValue(char(result[0]));

                m_onEditCallback();
            }
        }
    }

    void PatternValueEditor::visit(pl::ptrn::PatternEnum& pattern) {
        if (ImGui::BeginCombo("##Enum", pattern.getFormattedValue().c_str())) {
            auto currValue = pattern.getValue().toUnsigned();
            for (auto &[name, enumValue] : pattern.getEnumValues()) {
                auto min = enumValue.min.toUnsigned();
                auto max = enumValue.max.toUnsigned();

                bool isSelected = min <= currValue && max >= currValue;
                if (ImGui::Selectable(fmt::format("{}::{}", pattern.getTypeName(), name, min, pattern.getSize() * 2).c_str(), isSelected)) {
                    pattern.setValue(enumValue.min);
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
        if (ImGui::InputText("##Value", value, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue)) {
            if (pattern.getWriteFormatterFunction().empty()) {
                wolv::math_eval::MathEvaluator<long double> mathEvaluator;

                if (auto result = mathEvaluator.evaluate(value); result.has_value())
                    pattern.setValue(double(result.value()));
            } else {
                pattern.setValue(value);
            }

            m_onEditCallback();
        }
    }

    void PatternValueEditor::visit(pl::ptrn::PatternPadding& pattern) {
        std::ignore = pattern;
    }

    void PatternValueEditor::visit(pl::ptrn::PatternPointer& pattern) {
        std::ignore = pattern;
    }

    void PatternValueEditor::visit(pl::ptrn::PatternSigned& pattern) {
        auto value = pattern.getFormattedValue();
        if (ImGui::InputText("##Value", value, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue)) {
            if (pattern.getWriteFormatterFunction().empty()) {
                wolv::math_eval::MathEvaluator<i128> mathEvaluator;

                if (auto result = mathEvaluator.evaluate(value); result.has_value())
                    pattern.setValue(result.value());
            } else {
                pattern.setValue(value);
            }

            m_onEditCallback();
        }
    }

    void PatternValueEditor::visit(pl::ptrn::PatternString& pattern) {
        auto value = pattern.toString();
        if (ImGui::InputText("##Value", value.data(), value.size() + 1, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue)) {
            pattern.setValue(value);
            m_onEditCallback();
        }
    }

    void PatternValueEditor::visit(pl::ptrn::PatternStruct& pattern) {
        auto value = pattern.toString();
        if (ImGui::InputText("##Value", value, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue)) {
            pattern.setValue(value);
            m_onEditCallback();
        }
    }

    void PatternValueEditor::visit(pl::ptrn::PatternUnion& pattern) {
        auto value = pattern.toString();
        if (ImGui::InputText("##Value", value, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue)) {
            pattern.setValue(value);
            m_onEditCallback();
        }
    }

    void PatternValueEditor::visit(pl::ptrn::PatternUnsigned& pattern) {
        auto value = pattern.toString();
        if (ImGui::InputText("##Value", value, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue)) {
            if (pattern.getWriteFormatterFunction().empty()) {
                wolv::math_eval::MathEvaluator<u128> mathEvaluator;

                if (auto result = mathEvaluator.evaluate(value); result.has_value())
                    pattern.setValue(result.value());
            } else {
                pattern.setValue(value);
            }

            m_onEditCallback();
        }
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