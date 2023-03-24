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

        void drawNameColumn(const pl::ptrn::Pattern& pattern) {
            highlightWhenSelected(pattern, [&]{ ImGui::TextUnformatted(pattern.getDisplayName().c_str()); });
            ImGui::TableNextColumn();
        }

        void drawColorColumn(const pl::ptrn::Pattern& pattern) {
            ImGui::ColorButton("color", ImColor(pattern.getColor()), ImGuiColorEditFlags_NoTooltip, ImVec2(ImGui::GetColumnWidth(), ImGui::GetTextLineHeight()));
            ImGui::TableNextColumn();
        }

        void drawOffsetColumn(const pl::ptrn::Pattern& pattern) {
            ImGui::TextFormatted("0x{0:08X} : 0x{1:08X}", pattern.getOffset(), pattern.getOffset() + pattern.getSize() - (pattern.getSize() == 0 ? 0 : 1));
            ImGui::TableNextColumn();
        }

        void drawSizeColumn(const pl::ptrn::Pattern& pattern) {
            ImGui::TextFormatted("0x{0:04X}", pattern.getSize());
            ImGui::TableNextColumn();
        }

        void drawCommentTooltip(const pl::ptrn::Pattern &pattern) {
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) {
                if (auto comment = pattern.getComment(); !comment.empty()) {
                    ImGui::BeginTooltip();
                    ImGui::TextUnformatted(pattern.getComment().c_str());
                    ImGui::EndTooltip();
                }
            }
        }

    }

    bool PatternDrawer::isEditingPattern(const pl::ptrn::Pattern& pattern) const {
        return this->m_editingPattern == &pattern && this->m_editingPatternOffset == pattern.getOffset();
    }

    void PatternDrawer::resetEditing() {
        this->m_editingPattern = nullptr;
        this->m_editingPatternOffset = 0x00;
    }

    void PatternDrawer::drawVisualizer(const std::vector<pl::core::Token::Literal> &arguments, pl::ptrn::Pattern &pattern, pl::ptrn::Iteratable &iteratable, bool reset) {
        auto visualizerName = arguments.front().toString(true);

        const auto &visualizers = ContentRegistry::PatternLanguage::impl::getVisualizers();

        if (auto entry = visualizers.find(visualizerName); entry != visualizers.end()) {
            const auto &[name, visualizer] = *entry;
            if (visualizer.parameterCount != arguments.size() - 1) {
                ImGui::TextUnformatted("hex.builtin.pattern_drawer.visualizer.invalid_parameter_count"_lang);
            } else {
                try {
                    visualizer.callback(pattern, iteratable, reset, { arguments.begin() + 1, arguments.end() });
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

    void PatternDrawer::drawVisualizerButton(pl::ptrn::Pattern& pattern, pl::ptrn::Iteratable &iteratable) {
        if (const auto &arguments = pattern.getAttributeArguments("hex::visualize"); !arguments.empty()) {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
            if (ImGui::IconButton(ICON_VS_EYE, ImGui::GetStyleColorVec4(ImGuiCol_Text), ImVec2(ImGui::GetTextLineHeightWithSpacing(), ImGui::GetTextLineHeight()))) {
                this->m_currVisualizedPattern = &pattern;
                this->m_lastVisualizerError.clear();

                ImGui::OpenPopup("Visualizer");
            }
            ImGui::PopStyleVar();

            ImGui::SameLine();

            if (ImGui::BeginPopup("Visualizer")) {
                if (this->m_currVisualizedPattern == &pattern) {
                    drawVisualizer(arguments, pattern, iteratable, !this->m_visualizedPatterns.contains(&pattern));
                    this->m_visualizedPatterns.insert(&pattern);
                }

                ImGui::EndPopup();
            }
        }
    }


    void PatternDrawer::createLeafNode(const pl::ptrn::Pattern& pattern) {
        ImGui::TreeNodeEx(pattern.getDisplayName().c_str(), ImGuiTreeNodeFlags_Leaf |
                                                            ImGuiTreeNodeFlags_NoTreePushOnOpen |
                                                            ImGuiTreeNodeFlags_SpanFullWidth |
                                                            ImGuiTreeNodeFlags_AllowItemOverlap);
    }

    bool PatternDrawer::createTreeNode(const pl::ptrn::Pattern& pattern) {
        if (pattern.isSealed()) {
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
                        return ImGui::TreeNodeEx(pattern.getDisplayName().c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth);
                    case Flattened:
                        return ImGui::TreeNodeEx(pattern.getDisplayName().c_str(), ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanFullWidth);
                }
            });
        }
    }

    void PatternDrawer::makeSelectable(const pl::ptrn::Pattern &pattern) {
        ImGui::PushID(static_cast<int>(pattern.getOffset()));
        ImGui::PushID(pattern.getVariableName().c_str());

        if (ImGui::Selectable("##PatternLine", false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap)) {
            this->m_selectionCallback(Region { pattern.getOffset(), pattern.getSize() });
            this->resetEditing();
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
        createLeafNode(pattern);
        ImGui::TableNextColumn();
        makeSelectable(pattern);
        drawCommentTooltip(pattern);
        ImGui::SameLine();
        drawNameColumn(pattern);
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
        createLeafNode(pattern);
        ImGui::TableNextColumn();
        makeSelectable(pattern);
        drawCommentTooltip(pattern);
        ImGui::SameLine();
        drawNameColumn(pattern);
        drawColorColumn(pattern);

        auto firstBitIdx = pattern.getBitOffset();
        auto lastBitIdx = firstBitIdx + (pattern.getBitSize() - 1);
        if (firstBitIdx == lastBitIdx)
            ImGui::TextFormatted("bit {0}", firstBitIdx);
        else
            ImGui::TextFormatted("bits {0} - {1}", firstBitIdx, lastBitIdx);
        ImGui::TableNextColumn();
        if (pattern.getBitSize() == 1)
            ImGui::TextFormatted("{0} bit", pattern.getBitSize());
        else
            ImGui::TextFormatted("{0} bits", pattern.getBitSize());
        ImGui::TableNextColumn();
        ImGui::TextFormattedColored(ImColor(0xFF9BC64D), "bits");
        ImGui::TableNextColumn();

        ImGui::TextFormatted("{}", pattern.getFormattedValue());
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
            drawColorColumn(pattern);
            drawOffsetColumn(pattern);
            drawSizeColumn(pattern);
            drawTypenameColumn(pattern, "bitfield");

            drawVisualizerButton(pattern, static_cast<pl::ptrn::Iteratable&>(pattern));
            ImGui::TextFormatted("{}", pattern.getFormattedValue());
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

            bool value = hex::get_or(pattern.getValue(), true) != 0;
            if (ImGui::Checkbox(pattern.getFormattedValue().c_str(), &value)) {
                pattern.setValue(value);
                this->resetEditing();
            }

            ImGui::PopItemWidth();
            ImGui::PopStyleVar();
        } else {
            ImGui::TextFormatted("{}", pattern.getFormattedValue());
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
            ImGui::TextFormatted("{}", pattern.getFormattedValue());
        }
    }

    void PatternDrawer::visit(pl::ptrn::PatternEnum& pattern) {
        ImGui::TableNextRow();
        createLeafNode(pattern);
        drawCommentTooltip(pattern);
        ImGui::TableNextColumn();
        makeSelectable(pattern);
        ImGui::SameLine();
        drawNameColumn(pattern);
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
            ImGui::TextFormatted("{}", pattern.getFormattedValue());
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
            ImGui::TextFormatted("{}", pattern.getFormattedValue());
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
            ImGui::TextFormatted("{}", pattern.getFormattedValue());
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
            ImGui::TextFormatted("{}", pattern.getFormattedValue());
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
                ImGui::TextFormatted("{}", pattern.getFormattedValue());
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

            if (this->isEditingPattern(pattern)) {
                if (pattern.getWriteFormatterFunction().empty())
                    ImGui::TextFormatted("{}", pattern.getFormattedValue());
                else {
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
                    auto value = pattern.toString();
                    if (ImGui::InputText("##Value", value, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue)) {
                        pattern.setValue(value);
                        this->resetEditing();
                    }
                    ImGui::PopItemWidth();
                    ImGui::PopStyleVar();
                }
            } else {
                drawVisualizerButton(pattern, static_cast<pl::ptrn::Iteratable&>(pattern));
                ImGui::TextFormatted("{}", pattern.getFormattedValue());
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

            if (this->isEditingPattern(pattern)) {
                if (pattern.getWriteFormatterFunction().empty())
                    ImGui::TextFormatted("{}", pattern.getFormattedValue());
                else {
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
                    auto value = pattern.toString();
                    if (ImGui::InputText("##Value", value, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue)) {
                        pattern.setValue(value);
                        this->resetEditing();
                    }
                    ImGui::PopItemWidth();
                    ImGui::PopStyleVar();
                }
            } else {
                drawVisualizerButton(pattern, static_cast<pl::ptrn::Iteratable&>(pattern));
                ImGui::TextFormatted("{}", pattern.getFormattedValue());
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
            ImGui::TextFormatted("{}", pattern.getFormattedValue());
        }
    }

    void PatternDrawer::visit(pl::ptrn::PatternWideCharacter& pattern) {
        createDefaultEntry(pattern);
        ImGui::TextFormatted("{}", pattern.getFormattedValue());
    }

    void PatternDrawer::visit(pl::ptrn::PatternWideString& pattern) {
        if (pattern.getSize() > 0) {
            createDefaultEntry(pattern);
            ImGui::TextFormatted("{}", pattern.getFormattedValue());
        }
    }

    void PatternDrawer::draw(pl::ptrn::Pattern& pattern) {
        if (pattern.getVisibility() == pl::ptrn::Visibility::Hidden)
            return;

        pattern.accept(*this);
    }

    void PatternDrawer::drawArray(pl::ptrn::Pattern& pattern, pl::ptrn::Iteratable &iteratable, bool isInlined) {
        if (iteratable.getEntryCount() == 0)
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
            ImGui::TextFormattedColored(ImColor(0xFF00FF00), "{0}", iteratable.getEntryCount());
            ImGui::SameLine(0, 0);
            ImGui::TextUnformatted("]");

            ImGui::TableNextColumn();

            drawVisualizerButton(pattern, iteratable);
            ImGui::TextFormatted("{}", pattern.getFormattedValue());
        }

        if (open) {
            u64 chunkCount = 0;
            for (u64 i = 0; i < iteratable.getEntryCount(); i += ChunkSize) {
                chunkCount++;

                auto &displayEnd = this->getDisplayEnd(pattern);
                if (chunkCount > displayEnd) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    ImGui::Selectable(hex::format("... ({})", "hex.builtin.pattern_drawer.double_click"_lang).c_str(), false, ImGuiSelectableFlags_SpanAllColumns);
                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                        displayEnd += DisplayEndStep;
                    break;
                } else {
                    auto endIndex = std::min<u64>(iteratable.getEntryCount(), i + ChunkSize);

                    bool chunkOpen = true;
                    if (iteratable.getEntryCount() > ChunkSize) {
                        auto startOffset = iteratable.getEntry(i)->getOffset();
                        auto endOffset = iteratable.getEntry(endIndex - 1)->getOffset();
                        auto endSize = iteratable.getEntry(endIndex - 1)->getSize();

                        size_t chunkSize = (endOffset - startOffset) + endSize;

                        ImGui::TableNextRow();
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
                        iteratable.forEachEntry(i, endIndex, [&](u64, auto *entry){
                            ImGui::PushID(id);
                            this->draw(*entry);
                            ImGui::PopID();

                            id += 1;
                        });

                        if (iteratable.getEntryCount() > ChunkSize)
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
        } else if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("offset")) {
            if (sortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending)
                return left->getOffset() < right->getOffset();
            else
                return left->getOffset() > right->getOffset();
        } else if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("size")) {
            if (sortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending)
                return left->getSize() < right->getSize();
            else
                return left->getSize() > right->getSize();
        } else if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("value")) {
            if (sortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending)
                return left->getValue() < right->getValue();
            else
                return left->getValue() > right->getValue();
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
        if (ImGui::BeginTable("##Patterntable", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable | ImGuiTableFlags_Hideable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, height))) {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("hex.builtin.pattern_drawer.var_name"_lang, ImGuiTableColumnFlags_PreferSortAscending | ImGuiTableColumnFlags_NoHide, 0, ImGui::GetID("name"));
            ImGui::TableSetupColumn("hex.builtin.pattern_drawer.color"_lang,    ImGuiTableColumnFlags_PreferSortAscending, 0, ImGui::GetID("color"));
            ImGui::TableSetupColumn("hex.builtin.pattern_drawer.offset"_lang,   ImGuiTableColumnFlags_PreferSortAscending | ImGuiTableColumnFlags_DefaultSort, 0, ImGui::GetID("offset"));
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

    void PatternDrawer::draw(const std::vector<std::shared_ptr<pl::ptrn::Pattern>> &patterns, float height) {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered()) {
            this->resetEditing();
        }

        if (beginPatternTable(patterns, this->m_sortedPatterns, height)) {
            ImGui::TableHeadersRow();

            int id = 1;
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
    }
}
