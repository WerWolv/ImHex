#include <hex/api/content_registry.hpp>
#include <hex/api/localization_manager.hpp>
#include <hex/helpers/utils.hpp>

#include <imgui.h>
#include <hex/ui/imgui_imhex_extensions.h>

namespace hex::plugin::builtin {

    // Tool for converting between different number formats.
    // There are three places where input can be changed; the bit checkboxes, the hex input, and the decimal input.
    // The bit checkboxes and the hex input are directly related and can be converted between each other easily.
    // The decimal input is a bit more complicated. IEEE 754 floating point numbers are represented as a sign bit,
    // an exponent and a mantissa. For details see https://en.wikipedia.org/wiki/IEEE_754.
    // Workflow is as follows:
    // From the bit checkboxes determine the integer hex value. This is straightforward.
    // From the hex value determine the binary floating point value by extracting the sign, exponent, and mantissa.
    // From the binary floating point value determine the decimal floating point value using a third party library.
    // From the decimal floating point we reconstruct the binary floating point value using internal hardware.
    // If the format is non-standard, the reconstruction is done using properties of the format.
    void drawIEEE754Decoder() {

        constexpr static auto flags = ImGuiInputTextFlags_EnterReturnsTrue;

        class IEEE754STATICS {
        public:
            IEEE754STATICS() : value(0), exponentBitCount(8), mantissaBitCount(23), resultFloat(0) {}

            u128 value;
            i32 exponentBitCount;
            i32 mantissaBitCount;
            long double resultFloat;
        };

        static IEEE754STATICS ieee754statics;


        enum class NumberType {
            Normal,
            Zero,
            Denormal,
            Infinity,
            NaN,
        };

        enum class InputType {
            Infinity,
            NotANumber,
            QuietNotANumber,
            SignalingNotANumber,
            Regular,
            Invalid
        };

        enum class ValueType {
            Regular,
            SignalingNaN,
            QuietNaN,
            NegativeInfinity,
            PositiveInfinity,
        };

        class IEEE754 {
        public:
            ValueType valueType;
            NumberType numberType;
            i64 exponentBias;
            long double signValue;
            long double exponentValue;
            long double mantissaValue;
            i64 signBits;
            i64 exponentBits;
            i64 mantissaBits;
            i64 precision;
        } ieee754 = {};

        std::string specialNumbers[] = {
                "inf" , "Inf", "INF" , "nan" , "Nan" , "NAN",
                "qnan","Qnan", "QNAN", "snan", "Snan", "SNAN"
        };


        const auto totalBitCount = ieee754statics.exponentBitCount + ieee754statics.mantissaBitCount;
        const auto signBitPosition = totalBitCount - 0;
        const auto exponentBitPosition = totalBitCount - 1;
        const auto mantissaBitPosition = totalBitCount - 1 - ieee754statics.exponentBitCount;

        const static auto ExtractBits = [](u32 startBit, u32 count) {
            return hex::extract(startBit, startBit - (count - 1), ieee754statics.value);
        };

        ieee754.signBits = ExtractBits(signBitPosition, 1);
        ieee754.exponentBits = ExtractBits(exponentBitPosition, ieee754statics.exponentBitCount);
        ieee754.mantissaBits = ExtractBits(mantissaBitPosition, ieee754statics.mantissaBitCount);


        static i64 inputFieldWidth = 0;
        ImGuiExt::TextFormattedWrapped("{}", "hex.builtin.tools.ieee754.description"_lang);
        ImGui::NewLine();

        static i64 displayMode = ContentRegistry::Settings::read<int>("hex.builtin.tools.ieee754.settings", "display_mode", 0);
        i64 displayModeTemp = displayMode;
        ImGui::RadioButton("hex.builtin.tools.ieee754.settings.display_mode.detailed"_lang, reinterpret_cast<int *>(&displayMode), 0);
        ImGui::SameLine();

        ImGui::RadioButton("hex.builtin.tools.ieee754.settings.display_mode.simplified"_lang, reinterpret_cast<int *>(&displayMode), 1);
        if (displayModeTemp != displayMode) {
            ContentRegistry::Settings::write<int>("hex.builtin.tools.ieee754.settings", "display_mode", displayMode);
            displayModeTemp = displayMode;
        }

        auto tableFlags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoKeepColumnsVisible |
                                 ImGuiTableFlags_ScrollX | ImGuiTableFlags_NoPadInnerX;

        const static auto IndentBoxOrLabel = [](u32 startBit, u32 bitIndex, u32 count, bool isLabel) {
            auto checkBoxWidth = ImGui::CalcTextSize("0").x + ImGui::GetStyle().FramePadding.x * 2.0F;
            auto columnWidth = ImGui::GetColumnWidth();
            float boxesPerColumn=columnWidth/checkBoxWidth;
            float result;
            if (isLabel) {
                std::string labelString = fmt::format("{}", bitIndex);
                auto labelWidth = ImGui::CalcTextSize(labelString.c_str()).x;
                auto leadingBoxes = (boxesPerColumn-count) / 2.0F;
                if (leadingBoxes < 0.0F)
                   leadingBoxes = 0.0F;
                result = checkBoxWidth*(leadingBoxes + startBit - bitIndex + 0.5F) - labelWidth / 2.0F;
            } else {
                if (count < boxesPerColumn)
                    result = (columnWidth - count * checkBoxWidth) / 2.0F;
                else
                    result = 0.0F;
            }
            if (result <= 0.0F)
                result = 0.05F;
            return result;
        };

        const static auto DisplayBitLabels = [](int startBit, int count) {
            static i32 lastLabelAdded = -1;
            i32 labelIndex;
            if (lastLabelAdded == -1 || count < 4)
                labelIndex = startBit - (count >> 1);
            else
                labelIndex = lastLabelAdded - 4;

            while (labelIndex + count > startBit) {
                auto indentSize = IndentBoxOrLabel(startBit, labelIndex, count, true);
                ImGui::Indent(indentSize );
                ImGuiExt::TextFormatted("{}", labelIndex);
                lastLabelAdded = labelIndex;
                ImGui::Unindent(indentSize );
                labelIndex -= 4;
                ImGui::SameLine();
            }
        };

        const static auto FormatBitLabels = [](i32 totalBitCount, i32 exponentBitPosition, i32 mantissaBitPosition) {
            // Row for bit labels. Due to font size constrains each bit cannot have its own label.
            // Instead, we label each 4 bits and then use the bit position to determine the bit label.
            // Result.
            ImGui::TableNextColumn();
            // Equals.
            ImGui::TableNextColumn();
            // Sign bit label is always shown.
            ImGui::TableNextColumn();

            DisplayBitLabels(totalBitCount + 1, 1);
            // Times.
            ImGui::TableNextColumn();
            // Exponent.
            ImGui::TableNextColumn();

            DisplayBitLabels(exponentBitPosition + 1, ieee754statics.exponentBitCount);

            // Times.
            ImGui::TableNextColumn();
            // Mantissa.
            ImGui::TableNextColumn();

            DisplayBitLabels(mantissaBitPosition + 1, ieee754statics.mantissaBitCount);
        };



        const static auto BitCheckbox = [](u8 bit) {
            bool checkbox = false;
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0F);
            checkbox = (ieee754statics.value & (u128(1) << bit)) != 0;
            ImGuiExt::BitCheckbox("##checkbox", &checkbox);
            ieee754statics.value = (ieee754statics.value & ~(u128(1) << bit)) | (u128(checkbox) << bit);
            ImGui::PopStyleVar();
        };


        const static auto BitCheckboxes = [](u32 startBit, u32 count) {
            for (u32 i = 0; i < count; i++) {
                ImGui::PushID(startBit - i);
                BitCheckbox(startBit - i);
                ImGui::SameLine(0, 0);
                ImGui::PopID();
            }
        };

        const static auto FormatBits = [](i32 signBitPosition, i32 exponentBitPosition, i32 mantissaBitPosition) {

            // Sign.
            ImGui::TableNextColumn();

            ImVec4 signColor = ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_IEEEToolSign);
            ImVec4 expColor = ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_IEEEToolExp);
            ImVec4 mantColor = ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_IEEEToolMantissa);
            ImVec4 black = ImVec4(0.0, 0.0, 0.0, 1.0);

            float indent = IndentBoxOrLabel(signBitPosition,signBitPosition, 1, false);
            ImGui::Indent(indent);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, signColor);
            ImGui::PushStyleColor(ImGuiCol_Border, black);

            BitCheckboxes(signBitPosition, 1);

            ImGui::PopStyleColor();
            ImGui::PopStyleColor();
            ImGui::Unindent(indent);

            // Times.
            ImGui::TableNextColumn();
            // Exponent.
            ImGui::TableNextColumn();
            indent = IndentBoxOrLabel(exponentBitPosition,exponentBitPosition, ieee754statics.exponentBitCount, false);
            ImGui::Indent(indent);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, expColor);
            ImGui::PushStyleColor(ImGuiCol_Border, black);

            BitCheckboxes(exponentBitPosition, ieee754statics.exponentBitCount);

            ImGui::PopStyleColor();
            ImGui::PopStyleColor();
            ImGui::Unindent(indent);
            // Times.
            ImGui::TableNextColumn();
            // Mantissa.
            ImGui::TableNextColumn();

            indent = IndentBoxOrLabel(mantissaBitPosition,mantissaBitPosition, ieee754statics.mantissaBitCount, false);
            ImGui::Indent(indent);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, mantColor);
            ImGui::PushStyleColor(ImGuiCol_Border, black);

            BitCheckboxes(mantissaBitPosition, ieee754statics.mantissaBitCount);

            ImGui::PopStyleColor();
            ImGui::PopStyleColor();
            ImGui::Unindent(indent);
        };

        const static auto BitsToFloat = [](IEEE754 &ieee754) {
            // Zero or denormal.
            if (ieee754.exponentBits == 0) {
                // Result doesn't fit in 128 bits.
                if ((ieee754.exponentBias - 1) > 128) {
                    ieee754.exponentValue = std::pow(2.0L, static_cast<long double>(-ieee754.exponentBias + 1));
                } else {
                    if (ieee754.exponentBias == 0) {
                        // Exponent is zero.
                        if (ieee754.mantissaBits == 0)
                            ieee754.exponentValue = 1.0;
                        else
                            // Exponent is one.
                            ieee754.exponentValue = 2.0;
                    } else {
                        ieee754.exponentValue = 1.0 / static_cast<long double>(u128(1) << (ieee754.exponentBias - 1));
                    }
                }
            }
                // Normal.
            else {
                // Result doesn't fit in 128 bits.
                if (std::abs(ieee754.exponentBits - ieee754.exponentBias) > 128) {
                    ieee754.exponentValue = std::pow(2.0L, static_cast<long double>(ieee754.exponentBits - ieee754.exponentBias));
                }
                //Result fits in 128 bits.
                else {
                    // Exponent is positive.
                    if (ieee754.exponentBits > ieee754.exponentBias)
                        ieee754.exponentValue = static_cast<long double>(u128(1) << (ieee754.exponentBits - ieee754.exponentBias));
                        // Exponent is negative.
                    else if (ieee754.exponentBits < ieee754.exponentBias)
                        ieee754.exponentValue = 1.0 / static_cast<long double>(u128(1) << (ieee754.exponentBias - ieee754.exponentBits));
                        // Exponent is zero.
                    else ieee754.exponentValue = 1.0;
                }
            }

            ieee754.mantissaValue = static_cast<long double>(ieee754.mantissaBits) / static_cast<long double>(u128(1) << (ieee754statics.mantissaBitCount));
            if (ieee754.exponentBits != 0)
                ieee754.mantissaValue += 1.0;

            // Check if all exponent bits are set.
            if (std::popcount(static_cast<u64>(ieee754.exponentBits)) == static_cast<i64>(ieee754statics.exponentBitCount)) {
                // If fraction is zero number is infinity,
                if (ieee754.mantissaBits == 0) {
                    if (ieee754.signBits == 0) {

                        ieee754.valueType = ValueType::PositiveInfinity;
                        ieee754statics.resultFloat = std::numeric_limits<long double>::infinity();

                    } else {

                        ieee754.valueType = ValueType::NegativeInfinity;
                        ieee754statics.resultFloat = -std::numeric_limits<long double>::infinity();

                    }
                    ieee754.numberType = NumberType::Infinity;

                    // otherwise number is NaN.
                } else {
                    if (ieee754.mantissaBits & (u128(1) << (ieee754statics.mantissaBitCount - 1))) {

                        ieee754.valueType = ValueType::QuietNaN;
                        ieee754statics.resultFloat = std::numeric_limits<long double>::quiet_NaN();

                    } else {

                        ieee754.valueType = ValueType::SignalingNaN;
                        ieee754statics.resultFloat = std::numeric_limits<long double>::signaling_NaN();

                    }
                    ieee754.numberType = NumberType::NaN;
                }
                // If all exponent bits are zero, but we have a non-zero fraction
                // then the number is denormal which are smaller than regular numbers
                // but not as precise.
            } else if (ieee754.exponentBits == 0 && ieee754.mantissaBits != 0) {

                ieee754.numberType = NumberType::Denormal;
                ieee754.valueType = ValueType::Regular;
                ieee754statics.resultFloat = ieee754.signValue * ieee754.exponentValue * ieee754.mantissaValue;

            } else {

                ieee754.numberType = NumberType::Normal;
                ieee754.valueType = ValueType::Regular;
                ieee754statics.resultFloat = ieee754.signValue * ieee754.exponentValue *  ieee754.mantissaValue;

            }
        };

        const static auto FloatToBits = [&specialNumbers](IEEE754 &ieee754, std::string decimalFloatingPointNumberString, int totalBitCount) {

            // Always obtain sign first.
            if (decimalFloatingPointNumberString[0] == '-') {
                // And remove it from the string.
                ieee754.signBits = 1;
                decimalFloatingPointNumberString.erase(0, 1);
            } else {
                // Important to switch from - to +.
                ieee754.signBits = 0;
            }

            InputType inputType = InputType::Regular;
            bool matchFound = false;

            // Detect and use special numbers.
            for (u32 i = 0; i < 12; i++) {
                if (decimalFloatingPointNumberString == specialNumbers[i]) {
                    inputType = InputType(i/3);
                    matchFound = true;
                    break;
                }
            }

            if (!matchFound)
                inputType = InputType::Regular;

            if (inputType == InputType::Regular) {
                try {
                    ieee754statics.resultFloat = stod(decimalFloatingPointNumberString);
                } catch(const std::invalid_argument& _) {
                    inputType = InputType::Invalid;
                }
            } else if (inputType == InputType::Infinity) {
                ieee754statics.resultFloat = std::numeric_limits<long double>::infinity();
                ieee754statics.resultFloat *= (ieee754.signBits == 1 ? -1 : 1);

            } else if (inputType == InputType::NotANumber) {
                ieee754statics.resultFloat = std::numeric_limits<long double>::quiet_NaN();
            } else if (inputType == InputType::QuietNotANumber) {
                ieee754statics.resultFloat = std::numeric_limits<long double>::quiet_NaN();
            } else if (inputType == InputType::SignalingNotANumber) {
                ieee754statics.resultFloat = std::numeric_limits<long double>::signaling_NaN();
            }


            if (inputType != InputType::Invalid) {
                // Deal with zero first so we can use log2.
                if (ieee754statics.resultFloat == 0.0) {
                    if (ieee754.signBits == 1)
                        ieee754statics.resultFloat = -0.0;
                    else
                        ieee754statics.resultFloat = 0.0;
                    ieee754.numberType = NumberType::Zero;
                    ieee754.valueType = ValueType::Regular;
                    ieee754.exponentBits = 0;
                    ieee754.mantissaBits = 0;

                } else {
                    long double log2Result = std::log2(ieee754statics.resultFloat);
                    // 2^(bias+1)-2^(bias-prec) is the largest number that can be represented.
                    // If the number entered is larger than this then the input is set to infinity.
                    if (ieee754statics.resultFloat > (std::pow(2.0L, ieee754.exponentBias + 1) - std::pow(2.0L, ieee754.exponentBias - ieee754statics.mantissaBitCount)) || inputType == InputType::Infinity ) {

                        ieee754statics.resultFloat = std::numeric_limits<long double>::infinity();
                        ieee754.numberType = NumberType::Infinity;
                        ieee754.valueType = ieee754.signBits == 1 ? ValueType::NegativeInfinity : ValueType::PositiveInfinity;
                        ieee754.exponentBits = (u128(1) << ieee754statics.exponentBitCount) - 1;
                        ieee754.mantissaBits = 0;

                    } else if (-std::rint(log2Result) > ieee754.exponentBias + ieee754statics.mantissaBitCount - 1) {

                        // 1/2^(bias-1+prec) is the smallest number that can be represented.
                        // If the number entered is smaller than this then the input is set to zero.
                        if (ieee754.signBits == 1)
                            ieee754statics.resultFloat = -0.0;
                        else
                            ieee754statics.resultFloat = 0.0;

                        ieee754.numberType = NumberType::Zero;
                        ieee754.valueType = ValueType::Regular;
                        ieee754.exponentBits = 0;
                        ieee754.mantissaBits = 0;

                    } else if (inputType == InputType::SignalingNotANumber) {

                        ieee754statics.resultFloat = std::numeric_limits<long double>::signaling_NaN();
                        ieee754.valueType = ValueType::SignalingNaN;
                        ieee754.numberType = NumberType::NaN;
                        ieee754.exponentBits = (u128(1) << ieee754statics.exponentBitCount) - 1;
                        ieee754.mantissaBits = 1;

                    } else if (inputType == InputType::QuietNotANumber || inputType == InputType::NotANumber ) {

                        ieee754statics.resultFloat = std::numeric_limits<long double>::quiet_NaN();
                        ieee754.valueType = ValueType::QuietNaN;
                        ieee754.numberType = NumberType::NaN;
                        ieee754.exponentBits = (u128(1) << ieee754statics.exponentBitCount) - 1;
                        ieee754.mantissaBits = (u128(1) << (ieee754statics.mantissaBitCount - 1));

                    } else if (static_cast<i64>(std::floor(log2Result)) + ieee754.exponentBias <= 0) {

                        ieee754.numberType = NumberType::Denormal;
                        ieee754.valueType = ValueType::Regular;
                        ieee754.exponentBits = 0;
                        auto mantissaExp = log2Result + ieee754.exponentBias + ieee754statics.mantissaBitCount - 1;
                        ieee754.mantissaBits = static_cast<i64>(std::round(std::pow(2.0L, mantissaExp)));

                    } else {

                        ieee754.valueType = ValueType::Regular;
                        ieee754.numberType = NumberType::Normal;
                        i64 unBiasedExponent = static_cast<i64>(std::floor(log2Result));
                        ieee754.exponentBits = unBiasedExponent + ieee754.exponentBias;
                        ieee754.mantissaValue = ieee754statics.resultFloat * std::pow(2.0L, -unBiasedExponent) - 1;
                        ieee754.mantissaBits = static_cast<i64>(std::round( static_cast<long double>(u128(1) << (ieee754statics.mantissaBitCount)) * ieee754.mantissaValue));

                    }
                }
                // Put the bits together.
                ieee754statics.value = (ieee754.signBits << (totalBitCount)) | (ieee754.exponentBits << (totalBitCount - ieee754statics.exponentBitCount)) | ieee754.mantissaBits;
            }
        };

        const static auto DisplayDecimal  = [](IEEE754 &ieee754) {

            unsigned signColorU32 = ImGuiExt::GetCustomColorU32(ImGuiCustomCol_IEEEToolSign);
            unsigned expColorU32 = ImGuiExt::GetCustomColorU32(ImGuiCustomCol_IEEEToolExp);
            unsigned mantColorU32 = ImGuiExt::GetCustomColorU32(ImGuiCustomCol_IEEEToolMantissa);

            ImGui::TableNextColumn();

            ImGui::TextUnformatted("=");

            // Sign.
            ImGui::TableNextColumn();

            // This has the effect of dimming the color of the numbers so user doesn't try
            // to interact with them.
            ImVec4 textColor =  ImGui::GetStyleColorVec4(ImGuiCol_Text);
            ImGui::BeginDisabled();
            ImGui::PushStyleColor(ImGuiCol_Text, textColor);

            ImGui::Indent(10_scaled);
            ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, signColorU32);
            if (ieee754.signBits == 1)
                ImGui::TextUnformatted("-1");
            else
                ImGui::TextUnformatted("+1");
            ImGui::Unindent(10_scaled);

            // Times.
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("x");
            ImGui::TableNextColumn();

            // Exponent.
            ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, expColorU32);

            ImGui::Indent(20_scaled);
            if (ieee754.numberType == NumberType::NaN) {
                if (ieee754.valueType == ValueType::QuietNaN)
                    ImGui::TextUnformatted("qNaN");
                else
                    ImGui::TextUnformatted("sNaN");
            } else if (ieee754.numberType == NumberType::Infinity) {
                ImGui::TextUnformatted("Inf");
            } else if (ieee754.numberType == NumberType::Zero) {
                ImGui::TextUnformatted("0");
            } else if (ieee754.numberType == NumberType::Denormal) {
                ImGuiExt::TextFormatted("2^{0}", 1 - ieee754.exponentBias);
            } else {
                ImGuiExt::TextFormatted("2^{0}", ieee754.exponentBits - ieee754.exponentBias);
            }

            ImGui::Unindent(20_scaled);

            // Times.
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("x");
            ImGui::TableNextColumn();

            // Mantissa.
            ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, mantColorU32);
            ImGui::Indent(20_scaled);
            ImGuiExt::TextFormatted("{:.{}}", ieee754.mantissaValue,ieee754.precision);
            ImGui::Unindent(20_scaled);

            ImGui::PopStyleColor();
            ImGui::EndDisabled();
        };


        const static auto ToolMenu = [](i64 &inputFieldWidth) {
            // If precision and exponent match one of the IEEE 754 formats the format is highlighted
            // and remains highlighted until user changes to a different format. Matching formats occur when
            // the user clicks on one of the selections or if the slider values match the format in question.
            // When a new format is selected, it may have a smaller number of digits than
            // the previous selection. Since the largest of the hexadecimal and the decimal
            // representation widths set both field widths to the same value, we need to
            // reset it here when a new choice is set.

            auto exponentBitCount = ieee754statics.exponentBitCount;
            auto mantissaBitCount = ieee754statics.mantissaBitCount;
            if (ImGui::SliderInt("hex.builtin.tools.ieee754.exponent_size"_lang, &exponentBitCount, 1, 63 - mantissaBitCount)) {
                inputFieldWidth = 0;
                ieee754statics.exponentBitCount = exponentBitCount;
            }
            if (ImGui::SliderInt("hex.builtin.tools.ieee754.mantissa_size"_lang, &mantissaBitCount, 1, 63 - exponentBitCount)) {
                inputFieldWidth = 0;
                ieee754statics.mantissaBitCount = mantissaBitCount;
            }
            ImGui::Separator();

            auto color = ImGui::GetColorU32(ImGuiCol_ButtonActive);

            bool needsPop = false;
            if (ieee754statics.exponentBitCount == 3 && ieee754statics.mantissaBitCount == 4) {
                ImGui::PushStyleColor(ImGuiCol_Button, color);
                needsPop = true;
            }
            if (ImGui::Button("hex.builtin.tools.ieee754.quarter_precision"_lang)) {
                ieee754statics.exponentBitCount = 3;
                ieee754statics.mantissaBitCount = 4;
                inputFieldWidth = 0;
            }
            if (needsPop) ImGui::PopStyleColor();

            ImGui::SameLine();
            needsPop = false;
            if (ieee754statics.exponentBitCount == 5 && ieee754statics.mantissaBitCount == 10) {
                ImGui::PushStyleColor(ImGuiCol_Button, color);
                needsPop = true;
            }
            if (ImGui::Button("hex.builtin.tools.ieee754.half_precision"_lang)) {
                ieee754statics.exponentBitCount = 5;
                ieee754statics.mantissaBitCount = 10;
                inputFieldWidth = 0;
            }
            if (needsPop) ImGui::PopStyleColor();

            ImGui::SameLine();

            needsPop = false;
            if (ieee754statics.exponentBitCount == 8 && ieee754statics.mantissaBitCount == 23) {
                ImGui::PushStyleColor(ImGuiCol_Button, color);
                needsPop = true;
            }
            if (ImGui::Button("hex.builtin.tools.ieee754.single_precision"_lang)) {
                ieee754statics.exponentBitCount = 8;
                ieee754statics.mantissaBitCount = 23;
                inputFieldWidth = 0;
            }
            if (needsPop) ImGui::PopStyleColor();

            ImGui::SameLine();

            needsPop = false;
            if (ieee754statics.exponentBitCount == 11 && ieee754statics.mantissaBitCount == 52) {
                ImGui::PushStyleColor(ImGuiCol_Button, color);
                needsPop = true;
            }
            if (ImGui::Button("hex.builtin.tools.ieee754.double_precision"_lang)) {
                ieee754statics.exponentBitCount = 11;
                ieee754statics.mantissaBitCount = 52;
                inputFieldWidth = 0;
            }
            if (needsPop) ImGui::PopStyleColor();

            ImGui::SameLine();

            needsPop = false;
            if (ImGui::Button("hex.builtin.tools.ieee754.clear"_lang))
                // This will reset all interactive widgets to zero.
                ieee754statics.value = 0;

            ImGui::Separator();

            ImGui::NewLine();
        };

        if (ImGui::BeginTable("##outer", 7, tableFlags, ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 5.5 ))) {
            ImGui::TableSetupColumn("hex.builtin.tools.ieee754.result.title"_lang);
            ImGui::TableSetupColumn("##equals");
            ImGui::TableSetupColumn("hex.builtin.tools.ieee754.sign"_lang);
            ImGui::TableSetupColumn("##times");
            ImGui::TableSetupColumn("hex.builtin.tools.ieee754.exponent"_lang);
            ImGui::TableSetupColumn("##times");
            ImGui::TableSetupColumn("hex.builtin.tools.ieee754.mantissa"_lang);
            ImGui::TableHeadersRow();
            ImGui::TableNextRow();

            FormatBitLabels(totalBitCount, exponentBitPosition, mantissaBitPosition);


            ImGui::TableNextRow();
            // Row for bit checkboxes
            // Result.
            ImGui::TableNextColumn();

            u64 mask = hex::bitmask(totalBitCount+1);
            std::string maskString = hex::format("0x{:X}  ", mask);

            auto style = ImGui::GetStyle();
            inputFieldWidth = std::fmax(inputFieldWidth,
                                        ImGui::CalcTextSize(maskString.c_str()).x + style.FramePadding.x * 2.0F);
            ImGui::PushItemWidth(inputFieldWidth);

            u64 newValue = ieee754statics.value & mask;
            if (ImGuiExt::InputHexadecimal("##hex", &newValue, flags))
                ieee754statics.value = newValue;
            ImGui::PopItemWidth();

            // Equals.
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("=");

            FormatBits(signBitPosition, exponentBitPosition, mantissaBitPosition);


            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            ieee754.exponentBias = (u128(1) << (ieee754statics.exponentBitCount - 1)) - 1;

            ieee754.signValue = ieee754.signBits == 0 ? 1.0 : -1.0;

            BitsToFloat(ieee754);

            if (ieee754.numberType == NumberType::Denormal)
                ieee754.precision = std::ceil(1+ieee754statics.mantissaBitCount * std::log10(2.0L));
            else
                ieee754.precision = std::ceil(1+(ieee754statics.mantissaBitCount + 1) * std::log10(2.0L));

            // For C++ from_chars is better than strtold.
            // The main problem is that from_chars will not process special numbers
            // like inf and nan, so we handle them manually.
            static std::string decimalFloatingPointNumberString;

            // Use qnan for quiet NaN and snan for signaling NaN.
            if (ieee754.numberType == NumberType::NaN) {
                if (ieee754.valueType == ValueType::QuietNaN)
                    decimalFloatingPointNumberString = "qnan";
                else
                    decimalFloatingPointNumberString = "snan";
            } else {
                decimalFloatingPointNumberString = fmt::format("{:.{}}", ieee754statics.resultFloat, ieee754.precision);
            }

            auto style1 = ImGui::GetStyle();
            inputFieldWidth = std::fmax(inputFieldWidth, ImGui::CalcTextSize(decimalFloatingPointNumberString.c_str()).x + 2 * style1.FramePadding.x);
            ImGui::PushItemWidth(inputFieldWidth);


            // We allow any input in order to accept infinities and NaNs, all invalid entries
            // are detected catching exceptions. You can also enter -0 or -inf.
            if (ImGui::InputText("##resultFloat", decimalFloatingPointNumberString, flags)) {
                FloatToBits(ieee754, decimalFloatingPointNumberString, totalBitCount);
            }
            ImGui::PopItemWidth();

            if (displayMode == 0)
                DisplayDecimal(ieee754);

            ImGui::EndTable();

        }

        ToolMenu(inputFieldWidth);
    }

}
