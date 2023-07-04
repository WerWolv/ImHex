#include <ui/pattern_drawer.hpp>

#include <pl/patterns/pattern_array_dynamic.hpp>
#include <pl/patterns/pattern_array_static.hpp>
#include <pl/patterns/pattern_bitfield.hpp>
#include <pl/patterns/pattern_boolean.hpp>
#include <pl/patterns/pattern_character.hpp>
#include <pl/patterns/pattern_enum.hpp>
#include <pl/patterns/pattern_float.hpp>
#include <pl/patterns/pattern_padding.hpp>
#include <pl/patterns/pattern_pointer.hpp>
#include <pl/patterns/pattern_signed.hpp>
#include <pl/patterns/pattern_string.hpp>
#include <pl/patterns/pattern_struct.hpp>
#include <pl/patterns/pattern_union.hpp>
#include <pl/patterns/pattern_unsigned.hpp>
#include <pl/patterns/pattern_wide_character.hpp>
#include <pl/patterns/pattern_wide_string.hpp>

#include <string>

#include <hex/api/imhex_api.hpp>
#include <hex/api/content_registry.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/api/localization.hpp>
#include <content/helpers/math_evaluator.hpp>

#include <imgui.h>
#include <hex/ui/imgui_imhex_extensions.h>
#include <fonts/codicons_font.h>
#include <fonts/fontawesome_font.h>


namespace hex::plugin::builtin::ui {

    namespace {

        constexpr auto DisplayEndDefault = 50U;

        using namespace ::std::literals::string_literals;

        bool isPatternSelected(u64 address, u64 size) {
            auto currSelection = ImHexApi::HexEditor::getSelection();
            if (!currSelection.has_value())
                return false;

            return Region{ address, size }.overlaps(*currSelection);
        }

        template<typename T>
        auto highlightWhenSelected(u64 address, u64 size, const T &callback) {
            constexpr bool HasReturn = !requires(T t) { { t() } -> std::same_as<void>; };

            auto selected = isPatternSelected(address, size);

            if (selected)
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));

            if constexpr (HasReturn) {
                auto result = callback();

                if (selected)
                    ImGui::PopStyleColor();

                return result;
            } else {
                callback();

                if (selected)
                    ImGui::PopStyleColor();
            }
        }

        template<typename T>
        auto highlightWhenSelected(const pl::ptrn::Pattern& pattern, const T &callback) {
            return highlightWhenSelected(pattern.getOffset(), pattern.getSize(), callback);
        }

        void drawTypenameColumn(const pl::ptrn::Pattern& pattern, const std::string& pattern_name) {
            ImGui::TextFormattedColored(ImColor(0xFFD69C56), pattern_name);
            ImGui::SameLine();
            ImGui::TextUnformatted(pattern.getTypeName().c_str());
            ImGui::TableNextColumn();
        }

        void drawColorColumn(const pl::ptrn::Pattern& pattern) {
            if (pattern.getVisibility() == pl::ptrn::Visibility::Visible)
                ImGui::ColorButton("color", ImColor(pattern.getColor()), ImGuiColorEditFlags_NoTooltip, ImVec2(ImGui::GetColumnWidth(), ImGui::GetTextLineHeight()));

            ImGui::TableNextColumn();
        }

        inline void drawOffsetColumnForBitfieldMember(const pl::ptrn::PatternBitfieldMember &pattern) {
            if (pattern.isPatternLocal()) {
                ImGui::TextFormatted("[{}]", "hex.builtin.pattern_drawer.local"_lang);
                ImGui::TableNextColumn();
                ImGui::TextFormatted("[{}]", "hex.builtin.pattern_drawer.local"_lang);
                ImGui::TableNextColumn();
            } else {
                ImGui::TextFormatted("0x{0:08X}, bit {1}", pattern.getOffset(), pattern.getBitOffsetForDisplay());
                ImGui::TableNextColumn();
                ImGui::TextFormatted("0x{0:08X}, bit {1}", pattern.getOffset() + pattern.getSize(), pattern.getBitOffsetForDisplay() + pattern.getBitSize() - (pattern.getSize() == 0 ? 0 : 1));
                ImGui::TableNextColumn();
            }
        }

        void drawOffsetColumn(const pl::ptrn::Pattern& pattern) {
            if (auto *bitfieldMember = dynamic_cast<pl::ptrn::PatternBitfieldMember const*>(&pattern); bitfieldMember != nullptr && bitfieldMember->getParentBitfield() != nullptr)
                drawOffsetColumnForBitfieldMember(*bitfieldMember);
            else {
                if (pattern.isPatternLocal()) {
                    ImGui::TextFormatted("[{}]", "hex.builtin.pattern_drawer.local"_lang);
                } else {
                    ImGui::TextFormatted("0x{0:08X}", pattern.getOffset());
                }

                ImGui::TableNextColumn();

                if (pattern.isPatternLocal()) {
                    ImGui::TextFormatted("[{}]", "hex.builtin.pattern_drawer.local"_lang);
                } else {
                    ImGui::TextFormatted("0x{0:08X}", pattern.getOffset() + pattern.getSize() - (pattern.getSize() == 0 ? 0 : 1));
                }

                ImGui::TableNextColumn();
            }
        }

        inline void drawSizeColumnForBitfieldMember(const pl::ptrn::PatternBitfieldMember &pattern) {
            if (pattern.getBitSize() == 1)
                ImGui::TextFormatted("1 bit");
            else
                ImGui::TextFormatted("{0} bits", pattern.getBitSize());
        }

        void drawSizeColumn(const pl::ptrn::Pattern& pattern) {
            if (auto *bitfieldMember = dynamic_cast<pl::ptrn::PatternBitfieldMember const*>(&pattern); bitfieldMember != nullptr && bitfieldMember->getParentBitfield() != nullptr)
                drawSizeColumnForBitfieldMember(*bitfieldMember);
            else
                ImGui::TextFormatted("0x{0:04X}", pattern.getSize());

            ImGui::TableNextColumn();
        }

        void drawCommentTooltip(const pl::ptrn::Pattern &pattern) {
            if (auto comment = pattern.getComment(); !comment.empty()) {
                ImGui::InfoTooltip(comment.c_str());
            }
        }

        std::vector<std::string> parseRValueFilter(const std::string &filter) {
            std::vector<std::string> result;

            if (!filter.empty()) {
                result.emplace_back();
                for (char c : filter) {
                    if (c == '.')
                        result.emplace_back();
                    else if (c == '[') {
                        result.emplace_back();
                        result.back() += c;
                    } else {
                        result.back() += c;
                    }
                }
            }

            return result;
        }

    }

    bool PatternDrawer::isEditingPattern(const pl::ptrn::Pattern& pattern) const {
        return this->m_editingPattern == &pattern && this->m_editingPatternOffset == pattern.getOffset();
    }

    void PatternDrawer::resetEditing() {
        this->m_editingPattern = nullptr;
        this->m_editingPatternOffset = 0x00;
    }

    bool PatternDrawer::matchesFilter(const std::vector<std::string> &filterPath, const std::vector<std::string> &patternPath, bool fullMatch) {
        if (fullMatch) {
            if (patternPath.size() != filterPath.size())
                return false;
        }

        if (patternPath.size() <= filterPath.size()) {
            for (ssize_t i = patternPath.size() - 1; i >= 0; i--) {
                const auto &filter = filterPath[i];

                if (patternPath[i] != filter && !filter.empty() && filter != "*") {
                    return false;
                }
            }
        }

        return true;
    }

    void PatternDrawer::drawFavoriteColumn(const pl::ptrn::Pattern& pattern) {
        if (this->m_showFavoriteStars) {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

            if (this->m_favorites.contains(this->m_currPatternPath)) {
                if (ImGui::DimmedIconButton(ICON_VS_STAR_DELETE, ImGui::GetStyleColorVec4(ImGuiCol_PlotHistogram))) {
                    this->m_favorites.erase(this->m_currPatternPath);
                }
            }
            else {
                if (ImGui::DimmedIconButton(ICON_VS_STAR_ADD, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled))) {
                    this->m_favorites.insert({ this->m_currPatternPath, pattern.clone() });
                }
            }

            ImGui::PopStyleVar();
        }

        ImGui::TableNextColumn();
    }

    void PatternDrawer::drawVisualizer(const std::map<std::string, ContentRegistry::PatternLanguage::impl::Visualizer> &visualizers, const std::vector<pl::core::Token::Literal> &arguments, pl::ptrn::Pattern &pattern, pl::ptrn::IIterable &iterable, bool reset) {
        auto visualizerName = arguments.front().toString(true);

        if (auto entry = visualizers.find(visualizerName); entry != visualizers.end()) {
            const auto &[name, visualizer] = *entry;
            if (visualizer.parameterCount != arguments.size() - 1) {
                ImGui::TextUnformatted("hex.builtin.pattern_drawer.visualizer.invalid_parameter_count"_lang);
            } else {
                try {
                    visualizer.callback(pattern, iterable, reset, { arguments.begin() + 1, arguments.end() });
                } catch (std::exception &e) {
                    this->m_lastVisualizerError = e.what();
                }

            }
        } else {
            ImGui::TextUnformatted("hex.builtin.pattern_drawer.visualizer.unknown"_lang);
        }

        if (!this->m_lastVisualizerError.empty())
            ImGui::TextUnformatted(this->m_lastVisualizerError.c_str());
    }

    void PatternDrawer::drawValueColumn(pl::ptrn::Pattern& pattern) {
        const auto value = pattern.getFormattedValue();

        const auto width = ImGui::GetColumnWidth();
        if (const auto &visualizeArgs = pattern.getAttributeArguments("hex::visualize"); !visualizeArgs.empty()) {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0, 0.5F));

            bool shouldReset = false;
            if (ImGui::Button(hex::format(" {}  {}", ICON_VS_EYE_WATCH, value).c_str(), ImVec2(width, ImGui::GetTextLineHeight()))) {
                auto previousPattern = this->m_currVisualizedPattern;

                this->m_currVisualizedPattern = &pattern;
                this->m_lastVisualizerError.clear();

                if (this->m_currVisualizedPattern != previousPattern)
                    shouldReset = true;

                ImGui::OpenPopup("Visualizer");
            }
            ImGui::PopStyleVar(2);

            ImGui::SameLine();

            if (ImGui::BeginPopup("Visualizer")) {
                if (this->m_currVisualizedPattern == &pattern) {
                    drawVisualizer(ContentRegistry::PatternLanguage::impl::getVisualizers(), visualizeArgs, pattern, dynamic_cast<pl::ptrn::IIterable&>(pattern), !this->m_visualizedPatterns.contains(&pattern) || shouldReset);
                    this->m_visualizedPatterns.insert(&pattern);
                }

                ImGui::EndPopup();
            }
        } else if (const auto &inlineVisualizeArgs = pattern.getAttributeArguments("hex::inline_visualize"); !inlineVisualizeArgs.empty()) {
            drawVisualizer(ContentRegistry::PatternLanguage::impl::getInlineVisualizers(), inlineVisualizeArgs, pattern, dynamic_cast<pl::ptrn::IIterable&>(pattern), true);
        } else {
            ImGui::TextFormatted("{}", value);
        }

        if (ImGui::CalcTextSize(value.c_str()).x > width) {
            ImGui::InfoTooltip(value.c_str());
        }
    }

    bool PatternDrawer::createTreeNode(const pl::ptrn::Pattern& pattern, bool leaf) {
        drawFavoriteColumn(pattern);

        if (pattern.isSealed() || leaf) {
            ImGui::Indent();
            highlightWhenSelected(pattern, [&]{ ImGui::TextUnformatted(pattern.getDisplayName().c_str()); });
            ImGui::Unindent();
            return false;
        }
        else {
            return highlightWhenSelected(pattern, [&]{
                switch (this->m_treeStyle) {
                    using enum TreeStyle;
                    default:
                    case Default:
                        return ImGui::TreeNodeEx(pattern.getDisplayName().c_str(), ImGuiTreeNodeFlags_SpanFullWidth);
                    case AutoExpanded:
                        return ImGui::TreeNodeEx(pattern.getDisplayName().c_str(), ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_DefaultOpen);
                    case Flattened:
                        return ImGui::TreeNodeEx(pattern.getDisplayName().c_str(), ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen);
                }
            });
        }
    }

    void PatternDrawer::makeSelectable(const pl::ptrn::Pattern &pattern) {
        ImGui::PushID(static_cast<int>(pattern.getOffset()));
        ImGui::PushID(pattern.getVariableName().c_str());

        if (ImGui::Selectable("##PatternLine", false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap)) {
            this->m_selectionCallback(Region { pattern.getOffset(), pattern.getSize() });

            if (this->m_editingPattern != &pattern) {
                this->resetEditing();
            }
        }

        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            this->m_editingPattern = &pattern;
            this->m_editingPatternOffset = pattern.getOffset();
        }

        ImGui::SameLine(0, 0);

        ImGui::PopID();
        ImGui::PopID();
    }

    void PatternDrawer::createDefaultEntry(pl::ptrn::Pattern &pattern) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        createTreeNode(pattern, true);
        ImGui::SameLine(0, 0);
        makeSelectable(pattern);
        drawCommentTooltip(pattern);
        ImGui::TableNextColumn();
        drawColorColumn(pattern);
        drawOffsetColumn(pattern);
        drawSizeColumn(pattern);
        ImGui::TextFormattedColored(ImColor(0xFF9BC64D), "{}", pattern.getFormattedName().empty() ? pattern.getTypeName() : pattern.getFormattedName());
        ImGui::TableNextColumn();
    }

    void PatternDrawer::closeTreeNode(bool inlined) {
        if (!inlined && this->m_treeStyle != TreeStyle::Flattened)
            ImGui::TreePop();
    }


    void PatternDrawer::visit(pl::ptrn::PatternArrayDynamic& pattern) {
        drawArray(pattern, pattern, pattern.isInlined());
    }

    void PatternDrawer::visit(pl::ptrn::PatternArrayStatic& pattern) {
        drawArray(pattern, pattern, pattern.isInlined());
    }

    void PatternDrawer::visit(pl::ptrn::PatternBitfieldField& pattern) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        createTreeNode(pattern, true);
        ImGui::SameLine(0, 0);
        makeSelectable(pattern);
        drawCommentTooltip(pattern);
        ImGui::TableNextColumn();
        drawColorColumn(pattern);
        drawOffsetColumnForBitfieldMember(pattern);
        drawSizeColumnForBitfieldMember(pattern);
        ImGui::TableNextColumn();
        ImGui::TextFormattedColored(ImColor(0xFF9BC64D), "bits");
        ImGui::TableNextColumn();

        if (this->isEditingPattern(pattern)) {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);

            auto value = pattern.getValue();
            auto valueString = pattern.toString();

            if (pattern.getBitSize() == 1) {
                bool boolValue = value.toBoolean();
                if (ImGui::Checkbox("##boolean", &boolValue)) {
                    pattern.setValue(boolValue);
                }
            } else if (std::holds_alternative<i128>(value)) {
                if (ImGui::InputText("##Value", valueString, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue)) {
                    MathEvaluator<i128> mathEvaluator;

                    if (auto result = mathEvaluator.evaluate(valueString); result.has_value())
                        pattern.setValue(result.value());

                    this->resetEditing();
                }
            } else if (std::holds_alternative<u128>(value)) {
                if (ImGui::InputText("##Value", valueString, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue)) {
                    MathEvaluator<u128> mathEvaluator;

                    if (auto result = mathEvaluator.evaluate(valueString); result.has_value())
                        pattern.setValue(result.value());

                    this->resetEditing();
                }
            }

            ImGui::PopItemWidth();
            ImGui::PopStyleVar();
        } else {
            drawValueColumn(pattern);
        }
    }

    void PatternDrawer::visit(pl::ptrn::PatternBitfieldArray& pattern) {
        drawArray(pattern, pattern, pattern.isInlined());
    }

    void PatternDrawer::visit(pl::ptrn::PatternBitfield& pattern) {
        bool open = true;
        if (!pattern.isInlined() && this->m_treeStyle != TreeStyle::Flattened) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            open = createTreeNode(pattern);
            ImGui::SameLine(0, 0);
            makeSelectable(pattern);
            drawCommentTooltip(pattern);
            ImGui::TableNextColumn();

            if (pattern.isSealed())
                drawColorColumn(pattern);
            else
                ImGui::TableNextColumn();

            drawOffsetColumn(pattern);
            drawSizeColumn(pattern);
            drawTypenameColumn(pattern, "bitfield");

            drawValueColumn(pattern);
        }

        if (open) {
            int id = 1;
            pattern.forEachEntry(0, pattern.getEntryCount(), [&] (u64, auto *field) {
                ImGui::PushID(id);
                this->draw(*field);
                ImGui::PopID();

                id += 1;
            });

            closeTreeNode(pattern.isInlined());
        }
    }

    void PatternDrawer::visit(pl::ptrn::PatternBoolean& pattern) {
        createDefaultEntry(pattern);

        if (this->isEditingPattern(pattern)) {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);

            bool value = pattern.getValue().toBoolean();
            if (ImGui::Checkbox("##boolean", &value)) {
                pattern.setValue(value);
            }

            ImGui::PopItemWidth();
            ImGui::PopStyleVar();
        } else {
            drawValueColumn(pattern);
        }
    }

    void PatternDrawer::visit(pl::ptrn::PatternCharacter& pattern) {
        createDefaultEntry(pattern);

        if (this->isEditingPattern(pattern)) {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
            auto value = hex::encodeByteString(pattern.getBytes());
            if (ImGui::InputText("##Character", value.data(), value.size() + 1, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue)) {
                if (!value.empty()) {
                    auto result = hex::decodeByteString(value);
                    if (!result.empty())
                        pattern.setValue(char(result[0]));

                    this->resetEditing();
                }
            }
            ImGui::PopItemWidth();
            ImGui::PopStyleVar();
        } else {
            drawValueColumn(pattern);
        }
    }

    void PatternDrawer::visit(pl::ptrn::PatternEnum& pattern) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        createTreeNode(pattern, true);
        ImGui::SameLine(0, 0);
        makeSelectable(pattern);
        drawCommentTooltip(pattern);
        ImGui::TableNextColumn();
        drawColorColumn(pattern);
        drawOffsetColumn(pattern);
        drawSizeColumn(pattern);
        drawTypenameColumn(pattern, "enum");

        if (this->isEditingPattern(pattern)) {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);

            if (ImGui::BeginCombo("##Enum", pattern.getFormattedValue().c_str())) {
                auto currValue = pattern.getValue().toUnsigned();
                for (auto &value : pattern.getEnumValues()) {
                    auto min = value.min.toUnsigned();
                    auto max = value.max.toUnsigned();

                    bool isSelected = min <= currValue && max >= currValue;
                    if (ImGui::Selectable(fmt::format("{}::{} (0x{:0{}X})", pattern.getTypeName(), value.name, min, pattern.getSize() * 2).c_str(), isSelected)) {
                        pattern.setValue(value.min);
                        this->resetEditing();
                    }
                    if (isSelected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            ImGui::PopItemWidth();
            ImGui::PopStyleVar();
        } else {
            drawValueColumn(pattern);
        }
    }

    void PatternDrawer::visit(pl::ptrn::PatternFloat& pattern) {
        createDefaultEntry(pattern);

        if (this->isEditingPattern(pattern)) {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);

            auto value = pattern.toString();
            if (ImGui::InputText("##Value", value, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue)) {
                MathEvaluator<long double> mathEvaluator;

                if (auto result = mathEvaluator.evaluate(value); result.has_value())
                    pattern.setValue(double(result.value()));

                this->resetEditing();
            }

            ImGui::PopItemWidth();
            ImGui::PopStyleVar();
        } else {
            drawValueColumn(pattern);
        }
    }

    void PatternDrawer::visit(pl::ptrn::PatternPadding& pattern) {
        // Do nothing
        hex::unused(pattern);
    }

    void PatternDrawer::visit(pl::ptrn::PatternPointer& pattern) {
        bool open = true;

        if (!pattern.isInlined() && this->m_treeStyle != TreeStyle::Flattened) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            open = createTreeNode(pattern);
            ImGui::SameLine(0, 0);
            makeSelectable(pattern);
            drawCommentTooltip(pattern);
            ImGui::TableNextColumn();
            drawColorColumn(pattern);
            drawOffsetColumn(pattern);
            drawSizeColumn(pattern);
            ImGui::TextFormattedColored(ImColor(0xFF9BC64D), "{}", pattern.getFormattedName());
            ImGui::TableNextColumn();
            drawValueColumn(pattern);
        }

        if (open) {
            pattern.getPointedAtPattern()->accept(*this);

            closeTreeNode(pattern.isInlined());
        }
    }

    void PatternDrawer::visit(pl::ptrn::PatternSigned& pattern) {
        createDefaultEntry(pattern);

        if (this->isEditingPattern(pattern)) {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);

            auto value = pattern.getFormattedValue();
            if (ImGui::InputText("##Value", value, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue)) {
                MathEvaluator<i128> mathEvaluator;

                if (auto result = mathEvaluator.evaluate(value); result.has_value())
                    pattern.setValue(result.value());

                this->resetEditing();
            }

            ImGui::PopItemWidth();
            ImGui::PopStyleVar();
        } else {
            drawValueColumn(pattern);
        }
    }

    void PatternDrawer::visit(pl::ptrn::PatternString& pattern) {
        if (pattern.getSize() > 0) {
            createDefaultEntry(pattern);

            if (this->isEditingPattern(pattern)) {
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);

                auto value = pattern.toString();
                if (ImGui::InputText("##Value", value.data(), value.size() + 1, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue)) {
                    pattern.setValue(value);
                    this->resetEditing();
                }

                ImGui::PopItemWidth();
                ImGui::PopStyleVar();
            } else {
                drawValueColumn(pattern);
            }
        }
    }

    void PatternDrawer::visit(pl::ptrn::PatternStruct& pattern) {
        bool open = true;

        if (!pattern.isInlined() && this->m_treeStyle != TreeStyle::Flattened) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            open = createTreeNode(pattern);
            ImGui::SameLine(0, 0);
            makeSelectable(pattern);
            drawCommentTooltip(pattern);
            ImGui::TableNextColumn();
            if (pattern.isSealed())
                drawColorColumn(pattern);
            else
                ImGui::TableNextColumn();
            drawOffsetColumn(pattern);
            drawSizeColumn(pattern);
            drawTypenameColumn(pattern, "struct");

            if (this->isEditingPattern(pattern) && !pattern.getWriteFormatterFunction().empty()) {
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
                auto value = pattern.toString();
                if (ImGui::InputText("##Value", value, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue)) {
                    pattern.setValue(value);
                    this->resetEditing();
                }
                ImGui::PopItemWidth();
                ImGui::PopStyleVar();
            } else {
                drawValueColumn(pattern);
            }

        }

        if (open) {
            int id = 1;
            pattern.forEachEntry(0, pattern.getEntryCount(), [&](u64, auto *member){
                ImGui::PushID(id);
                this->draw(*member);
                ImGui::PopID();
                id += 1;
            });

            closeTreeNode(pattern.isInlined());
        }
    }

    void PatternDrawer::visit(pl::ptrn::PatternUnion& pattern) {
        bool open = true;

        if (!pattern.isInlined() && this->m_treeStyle != TreeStyle::Flattened) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            open = createTreeNode(pattern);
            ImGui::SameLine(0, 0);
            makeSelectable(pattern);
            drawCommentTooltip(pattern);
            ImGui::TableNextColumn();
            if (pattern.isSealed())
                drawColorColumn(pattern);
            else
                ImGui::TableNextColumn();
            drawOffsetColumn(pattern);
            drawSizeColumn(pattern);
            drawTypenameColumn(pattern, "union");

            if (this->isEditingPattern(pattern) && !pattern.getWriteFormatterFunction().empty()) {
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
                auto value = pattern.toString();
                if (ImGui::InputText("##Value", value, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue)) {
                    pattern.setValue(value);
                    this->resetEditing();
                }
                ImGui::PopItemWidth();
                ImGui::PopStyleVar();
            } else {
                drawValueColumn(pattern);
            }
        }

        if (open) {
            int id = 1;
            pattern.forEachEntry(0, pattern.getEntryCount(), [&](u64, auto *member) {
                ImGui::PushID(id);
                this->draw(*member);
                ImGui::PopID();

                id += 1;
            });

            closeTreeNode(pattern.isInlined());
        }
    }

    void PatternDrawer::visit(pl::ptrn::PatternUnsigned& pattern) {
        createDefaultEntry(pattern);

        if (this->isEditingPattern(pattern)) {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
            auto value = pattern.toString();
            if (ImGui::InputText("##Value", value, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue)) {
                MathEvaluator<u128> mathEvaluator;

                if (auto result = mathEvaluator.evaluate(value); result.has_value())
                    pattern.setValue(result.value());

                this->resetEditing();
            }
            ImGui::PopItemWidth();
            ImGui::PopStyleVar();
        } else {
            drawValueColumn(pattern);
        }
    }

    void PatternDrawer::visit(pl::ptrn::PatternWideCharacter& pattern) {
        createDefaultEntry(pattern);
        drawValueColumn(pattern);
    }

    void PatternDrawer::visit(pl::ptrn::PatternWideString& pattern) {
        if (pattern.getSize() > 0) {
            createDefaultEntry(pattern);
            drawValueColumn(pattern);
        }
    }

    void PatternDrawer::draw(pl::ptrn::Pattern& pattern) {
        if (pattern.getVisibility() == pl::ptrn::Visibility::Hidden)
            return;

        this->m_currPatternPath.push_back(pattern.getVariableName());
        ON_SCOPE_EXIT { this->m_currPatternPath.pop_back(); };

        if (matchesFilter(this->m_filter, this->m_currPatternPath, false))
            pattern.accept(*this);
    }

    void PatternDrawer::drawArray(pl::ptrn::Pattern& pattern, pl::ptrn::IIterable &iterable, bool isInlined) {
        if (iterable.getEntryCount() == 0)
            return;

        bool open = true;
        if (!isInlined && this->m_treeStyle != TreeStyle::Flattened) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            open = createTreeNode(pattern);
            ImGui::SameLine(0, 0);
            makeSelectable(pattern);
            drawCommentTooltip(pattern);
            ImGui::TableNextColumn();

            if (pattern.isSealed())
                drawColorColumn(pattern);
            else
                ImGui::TableNextColumn();
            drawOffsetColumn(pattern);
            drawSizeColumn(pattern);
            ImGui::TextFormattedColored(ImColor(0xFF9BC64D), "{0}", pattern.getTypeName());
            ImGui::SameLine(0, 0);

            ImGui::TextUnformatted("[");
            ImGui::SameLine(0, 0);
            ImGui::TextFormattedColored(ImColor(0xFF00FF00), "{0}", iterable.getEntryCount());
            ImGui::SameLine(0, 0);
            ImGui::TextUnformatted("]");

            ImGui::TableNextColumn();

            drawValueColumn(pattern);
        }

        if (open) {
            u64 chunkCount = 0;
            for (u64 i = 0; i < iterable.getEntryCount(); i += ChunkSize) {
                chunkCount++;

                auto &displayEnd = this->getDisplayEnd(pattern);
                if (chunkCount > displayEnd) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TableNextColumn();

                    ImGui::Selectable(hex::format("... ({})", "hex.builtin.pattern_drawer.double_click"_lang).c_str(), false, ImGuiSelectableFlags_SpanAllColumns);
                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                        displayEnd += DisplayEndStep;
                    break;
                } else {
                    auto endIndex = std::min<u64>(iterable.getEntryCount(), i + ChunkSize);

                    bool chunkOpen = true;
                    if (iterable.getEntryCount() > ChunkSize) {
                        auto startOffset = iterable.getEntry(i)->getOffset();
                        auto endOffset = iterable.getEntry(endIndex - 1)->getOffset();
                        auto endSize = iterable.getEntry(endIndex - 1)->getSize();

                        size_t chunkSize = (endOffset - startOffset) + endSize;

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TableNextColumn();

                        chunkOpen = highlightWhenSelected(startOffset, ((endOffset + endSize) - startOffset) - 1, [&]{
                            return ImGui::TreeNodeEx(hex::format("{0}[{1} ... {2}]", this->m_treeStyle == TreeStyle::Flattened ? pattern.getDisplayName().c_str() : "", i, endIndex - 1).c_str(), ImGuiTreeNodeFlags_SpanFullWidth);
                        });

                        ImGui::TableNextColumn();
                        drawColorColumn(pattern);
                        ImGui::TextFormatted("0x{0:08X} : 0x{1:08X}", startOffset, startOffset + chunkSize - (pattern.getSize() == 0 ? 0 : 1));
                        ImGui::TableNextColumn();
                        ImGui::TextFormatted("0x{0:04X}", chunkSize);
                        ImGui::TableNextColumn();
                        ImGui::TextFormattedColored(ImColor(0xFF9BC64D), "{0}", pattern.getTypeName());
                        ImGui::SameLine(0, 0);

                        ImGui::TextUnformatted("[");
                        ImGui::SameLine(0, 0);
                        ImGui::TextFormattedColored(ImColor(0xFF00FF00), "{0}", endIndex - i);
                        ImGui::SameLine(0, 0);
                        ImGui::TextUnformatted("]");

                        ImGui::TableNextColumn();
                        ImGui::TextFormatted("[ ... ]");
                    }


                    if (chunkOpen) {
                        int id = 1;
                        iterable.forEachEntry(i, endIndex, [&](u64, auto *entry){
                            ImGui::PushID(id);
                            this->draw(*entry);
                            ImGui::PopID();

                            id += 1;
                        });

                        if (iterable.getEntryCount() > ChunkSize)
                            ImGui::TreePop();
                    }
                }
            }

            closeTreeNode(isInlined);
        }
    }

    u64& PatternDrawer::getDisplayEnd(const pl::ptrn::Pattern& pattern) {
        auto it = this->m_displayEnd.find(&pattern);
        if (it != this->m_displayEnd.end()) {
            return it->second;
        }

        auto [value, success] = this->m_displayEnd.emplace(&pattern, DisplayEndDefault);
        return value->second;
    }

    static bool sortPatterns(const ImGuiTableSortSpecs* sortSpecs, const pl::ptrn::Pattern * left, const pl::ptrn::Pattern * right) {
        if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("name")) {
            if (sortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending)
                return left->getDisplayName() < right->getDisplayName();
            else
                return left->getDisplayName() > right->getDisplayName();
        } else if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("start")) {
            if (sortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending)
                return left->getOffsetForSorting() < right->getOffsetForSorting();
            else
                return left->getOffsetForSorting() > right->getOffsetForSorting();
        } else if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("end")) {
            if (sortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending)
                return left->getOffsetForSorting() + left->getSize() < right->getOffsetForSorting() + right->getSize();
            else
                return left->getOffsetForSorting() + left->getSize() > right->getOffsetForSorting() + right->getSize();
        } else if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("size")) {
            if (sortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending)
                return left->getSizeForSorting() < right->getSizeForSorting();
            else
                return left->getSizeForSorting() > right->getSizeForSorting();
        } else if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("value")) {
            if (sortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending)
                return left->getValue().toString(true) < right->getValue().toString(true);
            else
                return left->getValue().toString(true) > right->getValue().toString(true);
        } else if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("type")) {
            if (sortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending)
                return left->getTypeName() < right->getTypeName();
            else
                return left->getTypeName() > right->getTypeName();
        } else if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("color")) {
            if (sortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending)
                return left->getColor() < right->getColor();
            else
                return left->getColor() > right->getColor();
        }

        return false;
    }

    static bool beginPatternTable(const std::vector<std::shared_ptr<pl::ptrn::Pattern>> &patterns, std::vector<pl::ptrn::Pattern*> &sortedPatterns, float height) {
        if (ImGui::BeginTable("##Patterntable", 8, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable | ImGuiTableFlags_Hideable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, height))) {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("##favorite",                               ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_NoReorder | ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_IndentDisable, ImGui::GetTextLineHeight(), ImGui::GetID("favorite"));
            ImGui::TableSetupColumn("hex.builtin.pattern_drawer.var_name"_lang, ImGuiTableColumnFlags_PreferSortAscending | ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_IndentEnable, 0, ImGui::GetID("name"));
            ImGui::TableSetupColumn("hex.builtin.pattern_drawer.color"_lang,    ImGuiTableColumnFlags_PreferSortAscending, 0, ImGui::GetID("color"));
            ImGui::TableSetupColumn("hex.builtin.pattern_drawer.start"_lang,    ImGuiTableColumnFlags_PreferSortAscending | ImGuiTableColumnFlags_DefaultSort, 0, ImGui::GetID("start"));
            ImGui::TableSetupColumn("hex.builtin.pattern_drawer.end"_lang,      ImGuiTableColumnFlags_PreferSortAscending | ImGuiTableColumnFlags_DefaultSort, 0, ImGui::GetID("end"));
            ImGui::TableSetupColumn("hex.builtin.pattern_drawer.size"_lang,     ImGuiTableColumnFlags_PreferSortAscending, 0, ImGui::GetID("size"));
            ImGui::TableSetupColumn("hex.builtin.pattern_drawer.type"_lang,     ImGuiTableColumnFlags_PreferSortAscending, 0, ImGui::GetID("type"));
            ImGui::TableSetupColumn("hex.builtin.pattern_drawer.value"_lang,    ImGuiTableColumnFlags_PreferSortAscending, 0, ImGui::GetID("value"));

            auto sortSpecs = ImGui::TableGetSortSpecs();

            if (patterns.empty())
                sortedPatterns.clear();

            if (!patterns.empty() && (sortSpecs->SpecsDirty || sortedPatterns.empty())) {
                sortedPatterns.clear();
                std::transform(patterns.begin(), patterns.end(), std::back_inserter(sortedPatterns), [](const std::shared_ptr<pl::ptrn::Pattern> &pattern) {
                    return pattern.get();
                });

                std::sort(sortedPatterns.begin(), sortedPatterns.end(), [&sortSpecs](pl::ptrn::Pattern *left, pl::ptrn::Pattern *right) -> bool {
                    return sortPatterns(sortSpecs, left, right);
                });

                for (auto &pattern : sortedPatterns)
                    pattern->sort([&sortSpecs](const pl::ptrn::Pattern *left, const pl::ptrn::Pattern *right){
                        return sortPatterns(sortSpecs, left, right);
                    });

                sortSpecs->SpecsDirty = false;
            }

            return true;
        }

        return false;
    }

    void PatternDrawer::traversePatternTree(pl::ptrn::Pattern &pattern, std::vector<std::string> &patternPath, const std::function<void(pl::ptrn::Pattern&)> &callback) {
        patternPath.push_back(pattern.getVariableName());
        ON_SCOPE_EXIT { patternPath.pop_back(); };

        callback(pattern);
        if (auto iterable = dynamic_cast<pl::ptrn::IIterable*>(&pattern); iterable != nullptr) {
            iterable->forEachEntry(0, iterable->getEntryCount(), [&](u64, pl::ptrn::Pattern *entry) {
                traversePatternTree(*entry, patternPath, callback);
            });
        }
    }

    void PatternDrawer::draw(const std::vector<std::shared_ptr<pl::ptrn::Pattern>> &patterns, pl::PatternLanguage *runtime, float height) {
        const auto treeStyleButton = [this](auto icon, TreeStyle style, const char *tooltip) {
            bool pushed = false;

            if (this->m_treeStyle == style) {
                ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
                pushed = true;
            }

            if (ImGui::DimmedIconButton(icon, ImGui::GetStyleColorVec4(ImGuiCol_Text)))
                this->m_treeStyle = style;

            if (pushed)
                ImGui::PopStyleColor();

            ImGui::InfoTooltip(tooltip);
        };

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered()) {
            this->resetEditing();
        }

        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - ImGui::GetTextLineHeightWithSpacing() * 7.5);
        if (ImGui::InputTextIcon("##Search", ICON_VS_FILTER, this->m_filterText)) {
            this->m_filter = parseRValueFilter(this->m_filterText);
        }
        ImGui::PopItemWidth();

        ImGui::SameLine();

        treeStyleButton(ICON_VS_SYMBOL_KEYWORD, TreeStyle::Default,         "hex.builtin.pattern_drawer.tree_style.tree"_lang);
        ImGui::SameLine(0, 0);
        treeStyleButton(ICON_VS_LIST_TREE,      TreeStyle::AutoExpanded,    "hex.builtin.pattern_drawer.tree_style.auto_expanded"_lang);
        ImGui::SameLine(0, 0);
        treeStyleButton(ICON_VS_LIST_FLAT,      TreeStyle::Flattened,       "hex.builtin.pattern_drawer.tree_style.flattened"_lang);

        ImGui::SameLine(0, 15_scaled);

        const auto startPos = ImGui::GetCursorPos();

        ImGui::BeginDisabled(runtime == nullptr);
        if (ImGui::DimmedIconButton(ICON_VS_EXPORT, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
            ImGui::OpenPopup("ExportPatterns");
        }
        ImGui::EndDisabled();

        ImGui::InfoTooltip("hex.builtin.pattern_drawer.export"_lang);

        ImGui::SetNextWindowPos(ImGui::GetWindowPos() + ImVec2(startPos.x, ImGui::GetCursorPosY()));
        if (ImGui::BeginPopup("ExportPatterns")) {
            for (const auto &formatter : this->m_formatters) {
                const auto name = [&]{
                    auto name = formatter->getName();
                    std::transform(name.begin(), name.end(), name.begin(), [](char c){ return char(std::toupper(c)); });

                    return name;
                }();

                const auto &extension = formatter->getFileExtension();

                if (ImGui::MenuItem(name.c_str())) {
                    fs::openFileBrowser(fs::DialogMode::Save, { { name.c_str(), extension.c_str() } }, [&](const std::fs::path &path) {
                        auto result = formatter->format(*runtime);

                        wolv::io::File output(path, wolv::io::File::Mode::Create);
                        output.writeVector(result);
                    });
                }
            }
            ImGui::EndPopup();
        }

        if (!this->m_favoritesUpdated) {
            this->m_favoritesUpdated = true;

            if (!this->m_favorites.empty() && !patterns.empty() && !this->m_favoritesUpdateTask.isRunning()) {
                this->m_favoritesUpdateTask = TaskManager::createTask("hex.builtin.pattern_drawer.updating"_lang, TaskManager::NoProgress, [this, patterns](auto &task) {
                    size_t updatedFavorites = 0;
                    for (auto &pattern : patterns) {
                        if (updatedFavorites == this->m_favorites.size())
                            task.interrupt();
                        task.update();

                        std::vector<std::string> patternPath;
                        traversePatternTree(*pattern, patternPath, [&, this](pl::ptrn::Pattern &pattern) {
                            for (auto &[path, favoritePattern] : this->m_favorites) {
                                if (updatedFavorites == this->m_favorites.size())
                                    task.interrupt();
                                task.update();

                                if (this->matchesFilter(patternPath, path, true)) {
                                    favoritePattern = pattern.clone();
                                    updatedFavorites += 1;

                                    break;
                                }
                            }
                        });
                    }

                    std::erase_if(this->m_favorites, [](const auto &entry) {
                        const auto &[path, favoritePattern] = entry;

                        return favoritePattern == nullptr;
                    });
                });
            }

        }

        if (beginPatternTable(patterns, this->m_sortedPatterns, height)) {
            ImGui::TableHeadersRow();

            this->m_showFavoriteStars = false;
            if (!this->m_favorites.empty()) {
                ImGui::TableNextColumn();
                ImGui::TableNextColumn();
                ImGui::PushID(1);
                if (ImGui::TreeNodeEx("hex.builtin.pattern_drawer.favorites"_lang, ImGuiTreeNodeFlags_SpanFullWidth)) {
                    if (!this->m_favoritesUpdateTask.isRunning()) {
                        for (auto &[path, pattern] : this->m_favorites) {
                            if (pattern == nullptr)
                                continue;

                            ImGui::PushID(pattern->getDisplayName().c_str());
                            this->draw(*pattern);
                            ImGui::PopID();
                        }
                    } else {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TableNextColumn();
                        ImGui::TextSpinner("hex.builtin.pattern_drawer.updating"_lang);
                    }

                    ImGui::TreePop();
                }
                ImGui::PopID();
            }

            this->m_showFavoriteStars = true;

            int id = 2;
            for (auto &pattern : this->m_sortedPatterns) {
                ImGui::PushID(id);
                this->draw(*pattern);
                ImGui::PopID();

                id += 1;
            }

            ImGui::EndTable();
        }
    }

    void PatternDrawer::reset() {
        this->resetEditing();
        this->m_displayEnd.clear();
        this->m_visualizedPatterns.clear();
        this->m_currVisualizedPattern = nullptr;
        this->m_sortedPatterns.clear();
        this->m_lastVisualizerError.clear();
        this->m_currPatternPath.clear();

        for (auto &[path, pattern] : this->m_favorites)
            pattern = nullptr;
        this->m_favoritesUpdated = false;
    }
}
