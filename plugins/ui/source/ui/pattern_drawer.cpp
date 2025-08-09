#include <ui/pattern_drawer.hpp>

#include <pl/core/lexer.hpp>

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

#include <string>

#include <hex/api/imhex_api.hpp>
#include <hex/api/content_registry.hpp>
#include <hex/api/achievement_manager.hpp>
#include <hex/api/localization_manager.hpp>

#include <hex/helpers/utils.hpp>
#include <wolv/math_eval/math_evaluator.hpp>
#include <TextEditor.h>

#include <imgui.h>
#include <hex/ui/imgui_imhex_extensions.h>
#include <fonts/vscode_icons.hpp>

#include <wolv/io/file.hpp>

namespace hex::ui {

    namespace {

        std::mutex s_resetDrawMutex;

        constexpr auto DisplayEndDefault = 50U;

        using namespace ::std::literals::string_literals;

        bool isPatternOverlapSelected(u64 address, u64 size) {
            auto currSelection = ImHexApi::HexEditor::getSelection();
            if (!currSelection.has_value())
                return false;

            return Region(address, size).overlaps(*currSelection);
        }

        bool isPatternFullySelected(u64 address, u64 size) {
            auto currSelection = ImHexApi::HexEditor::getSelection();
            if (!currSelection.has_value())
                return false;

            return currSelection->address == address && currSelection->size == size;
        }

        template<typename T>
        auto highlightWhenSelected(u64 address, u64 size, const T &callback) {
            constexpr static bool HasReturn = !requires(T t) { { t() } -> std::same_as<void>; };

            const auto overlapSelected = isPatternOverlapSelected(address, size);
            const auto fullySelected = isPatternFullySelected(address, size);

            const u32 selectionColor = ImColor(ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_PatternSelected));
            if (overlapSelected)
                ImGui::PushStyleColor(ImGuiCol_Text, selectionColor);
            if (fullySelected)
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, (selectionColor & 0x00'FF'FF'FF) | 0x30'00'00'00);

            if constexpr (HasReturn) {
                auto result = callback();

                if (overlapSelected)
                    ImGui::PopStyleColor();

                return result;
            } else {
                callback();

                if (overlapSelected)
                    ImGui::PopStyleColor();
            }
        }

        template<typename T>
        auto highlightWhenSelected(const pl::ptrn::Pattern& pattern, const T &callback) {
            return highlightWhenSelected(pattern.getOffset(), pattern.getSize(), callback);
        }

        void drawTypeNameColumn(const pl::ptrn::Pattern& pattern, const std::string& structureTypeName) {
            ImGui::TableNextColumn();
            ImGuiExt::TextFormattedColored(TextEditor::getPalette()[u32(TextEditor::PaletteIndex::Keyword)], structureTypeName);
            ImGui::SameLine();
            ImGui::TextUnformatted(pattern.getTypeName().c_str());
        }

        void drawOffsetColumnForBitfieldMember(const pl::ptrn::PatternBitfieldMember &pattern) {
            if (pattern.isPatternLocal()) {
                ImGui::TableNextColumn();
                ImGuiExt::TextFormatted("[{}]", "hex.ui.pattern_drawer.local"_lang);
                ImGui::TableNextColumn();
                ImGuiExt::TextFormatted("[{}]", "hex.ui.pattern_drawer.local"_lang);
            } else {
                ImGui::TableNextColumn();
                ImGuiExt::TextFormatted("0x{0:08X}.{1}", pattern.getOffset(), pattern.getBitOffsetForDisplay());
                ImGui::TableNextColumn();

                const auto bitSize = (pattern.getBitOffsetForDisplay() + pattern.getBitSize() - (pattern.getSize() == 0 ? 0 : 1));
                ImGuiExt::TextFormatted("0x{0:08X}.{1}", pattern.getOffset() + (bitSize / 8), bitSize % 8);
            }
        }

        void drawOffsetColumns(const pl::ptrn::Pattern& pattern) {
            auto *bitfieldMember = dynamic_cast<const pl::ptrn::PatternBitfieldMember*>(&pattern);
            if (bitfieldMember != nullptr && bitfieldMember->getParent() != nullptr) {
                drawOffsetColumnForBitfieldMember(*bitfieldMember);
                return;
            }

            ImGui::TableNextColumn();

            if (pattern.isPatternLocal()) {
                ImGuiExt::TextFormatted("[{}]", "hex.ui.pattern_drawer.local"_lang);
            } else {
                ImGuiExt::TextFormatted("0x{0:08X}", pattern.getOffset());
            }

            ImGui::TableNextColumn();

            if (pattern.isPatternLocal()) {
                ImGuiExt::TextFormatted("[{}]", "hex.ui.pattern_drawer.local"_lang);
            } else {
                ImGuiExt::TextFormatted("0x{0:08X}", pattern.getOffset() + pattern.getSize() - (pattern.getSize() == 0 ? 0 : 1));
            }
        }

        void drawSizeColumnForBitfieldMember(const pl::ptrn::PatternBitfieldMember &pattern) {
            ImGui::TableNextColumn();

            auto bits = pattern.getBitSize();
            auto bytes = bits / 8;
            bits = bits % 8;

            std::string text;
            if (bytes != 0) {
                if (bytes == 1)
                    text += fmt::format("{0} byte", bytes);
                else
                    text += fmt::format("{0} bytes", bytes);

                if (bits != 0)
                    text += ", ";
            }

            if (bits != 0) {
                if (bits == 1)
                    text += fmt::format("{0} bit", bits);
                else
                    text += fmt::format("{0} bits", bits);
            }

            if (bytes == 0 && bits == 0) {
                text = "0 bytes";
            }

            ImGui::TextUnformatted(text.c_str());
        }

        void drawSizeColumn(const pl::ptrn::Pattern& pattern) {
            if (pattern.isPatternLocal()) {
                ImGui::TableNextColumn();
                return;
            }

            if (auto *bitfieldMember = dynamic_cast<const pl::ptrn::PatternBitfieldMember*>(&pattern); bitfieldMember != nullptr && bitfieldMember->getParent() != nullptr)
                drawSizeColumnForBitfieldMember(*bitfieldMember);
            else {
                ImGui::TableNextColumn();

                auto size = pattern.getSize();
                ImGuiExt::TextFormatted("{0:d} {1}", size, size == 1 ? "byte" : "bytes");
            }
        }

        void drawCommentTooltip(const pl::ptrn::Pattern &pattern) {
            if (auto comment = pattern.getComment(); !comment.empty()) {
                ImGuiExt::InfoTooltip(comment.c_str());
            }
        }

    }

    std::optional<PatternDrawer::Filter> PatternDrawer::parseRValueFilter(const std::string &filter) {
        Filter result;

        if (filter.empty()) {
            return result;
        }

        result.path.emplace_back();
        for (size_t i = 0; i < filter.size(); i += 1) {
            char c = filter[i];

            if (i < filter.size() - 1 && c == '=' && filter[i + 1] == '=') {
                pl::core::Lexer lexer;

                pl::api::Source source(filter.substr(i + 2));
                auto tokens = lexer.lex(&source);

                if (!tokens.isOk() || tokens.unwrap().size() != 2)
                    return std::nullopt;

                auto literal = std::get_if<pl::core::Token::Literal>(&tokens.unwrap().front().value);
                if (literal == nullptr)
                    return std::nullopt;
                result.value = *literal;

                break;
            } else if (c == '.') {
                result.path.emplace_back();
            } else if (c == '[') {
                result.path.emplace_back();
                result.path.back() += c;
            } else if (c == ' ') {
                // Skip whitespace
            } else {
                result.path.back() += c;
            }
        }

        return result;
    }

    void PatternDrawer::updateFilter() {
        m_filteredPatterns.clear();

        if (m_filter.path.empty()) {
            m_filteredPatterns = m_sortedPatterns;
            return;
        }

        std::vector<std::string> treePath;
        for (auto &pattern : m_sortedPatterns) {
            if (m_filteredPatterns.size() > m_maxFilterDisplayItems)
                break;

            traversePatternTree(*pattern, treePath, [this, &treePath](auto &pattern) {
                if (m_filteredPatterns.size() > m_maxFilterDisplayItems)
                    return;

                if (matchesFilter(m_filter.path, treePath, false)) {
                    if (!m_filter.value.has_value() || pattern.getValue() == m_filter.value)
                        m_filteredPatterns.push_back(&pattern);
                }
            });
        }
    }

    bool PatternDrawer::isEditingPattern(const pl::ptrn::Pattern& pattern) const {
        return m_editingPattern == &pattern && m_editingPatternOffset == pattern.getOffset();
    }

    void PatternDrawer::resetEditing() {
        m_editingPattern = nullptr;
        m_editingPatternOffset = 0x00;
    }

    bool PatternDrawer::matchesFilter(const std::vector<std::string> &filterPath, const std::vector<std::string> &patternPath, bool fullMatch) {
        if (fullMatch) {
            if (patternPath.size() != filterPath.size())
                return false;
        }

        if (filterPath.size() > patternPath.size())
            return false;

        auto commonSize = std::min(filterPath.size(), patternPath.size());
        for (size_t i = patternPath.size() - commonSize; i < patternPath.size(); i += 1) {
            const auto &filter = filterPath[i - (patternPath.size() - commonSize)];
            if (filter.empty())
                return false;

            if (filter != "*") {
                if (i == (patternPath.size() - 1)) {
                    if (!patternPath[i].starts_with(filter))
                        return false;
                } else {
                    if (patternPath[i] != filter)
                        return false;
                }
            }
        }

        return true;
    }

    void PatternDrawer::drawFavoriteColumn(const pl::ptrn::Pattern& pattern) {
        ImGui::TableNextColumn();
        if (!m_showFavoriteStars) {
            return;
        }

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

        if (m_favorites.contains(m_currPatternPath)) {
            if (ImGuiExt::DimmedIconButton(ICON_VS_STAR_FULL, ImGui::GetStyleColorVec4(ImGuiCol_PlotHistogram), {}, { 1_scaled, 0 })) {
                m_favorites.erase(m_currPatternPath);
            }
        }
        else {
            if (ImGuiExt::DimmedIconButton(ICON_VS_STAR_EMPTY, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled), {}, { 1_scaled, 0 })) {
                m_favorites.insert({ m_currPatternPath, pattern.clone() });
            }
        }

        ImGui::PopStyleVar();
    }

    bool PatternDrawer::drawNameColumn(const pl::ptrn::Pattern &pattern, bool leaf) {
        bool open = createTreeNode(pattern, leaf);
        ImGui::SameLine(0, 0);
        makeSelectable(pattern);
        drawCommentTooltip(pattern);

        return open;
    }

    void PatternDrawer::drawColorColumn(const pl::ptrn::Pattern& pattern) {
        ImGui::TableNextColumn();
        if (pattern.getVisibility() != pl::ptrn::Visibility::HighlightHidden) {
            ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, (pattern.getColor() & 0x00'FF'FF'FF) | 0xC0'00'00'00);

            if (m_rowColoring)
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, (pattern.getColor() & 0x00'FF'FF'FF) | 0x30'00'00'00);
        }
    }

    void PatternDrawer::drawCommentColumn(const pl::ptrn::Pattern& pattern) {
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(pattern.getComment().c_str());
    }

    void PatternDrawer::drawValueColumn(pl::ptrn::Pattern& pattern) {
        ImGui::TableNextColumn();

        const auto value = pattern.getFormattedValue();
        const bool valueValid = pattern.hasValidFormattedValue();
        const auto width = ImGui::GetColumnWidth();

        if (const auto &visualizeArgs = pattern.getAttributeArguments("hex::visualize"); !visualizeArgs.empty()) {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0, 0.5F));

            bool shouldReset = false;
            if (ImGui::Button(fmt::format(" {}  {}", ICON_VS_EYE, value).c_str(), ImVec2(width, ImGui::GetTextLineHeight()))) {
                const auto *previousPattern = m_currVisualizedPattern;
                m_currVisualizedPattern = &pattern;
                auto lastVisualizerError = m_visualizerDrawer.getLastVisualizerError();
                if (!lastVisualizerError.empty() || m_currVisualizedPattern != previousPattern)
                    shouldReset = true;

                m_visualizerDrawer.clearLastVisualizerError();

                ImGui::OpenPopup("Visualizer");
            }
            ImGui::PopStyleVar(2);

            ImGui::SameLine();

            if (ImGui::BeginPopupEx(ImGui::GetCurrentWindowRead()->GetID("Visualizer"), ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
                if (m_currVisualizedPattern == &pattern) {
                    m_visualizerDrawer.drawVisualizer(ContentRegistry::PatternLanguage::impl::getVisualizers(), visualizeArgs, pattern, !m_visualizedPatterns.contains(&pattern) || shouldReset);
                    m_visualizedPatterns.insert(&pattern);
                }

                ImGui::EndPopup();
            }
        } else if (const auto &inlineVisualizeArgs = pattern.getAttributeArguments("hex::inline_visualize"); !inlineVisualizeArgs.empty()) {
            m_visualizerDrawer.drawVisualizer(ContentRegistry::PatternLanguage::impl::getInlineVisualizers(), inlineVisualizeArgs, pattern, true);
        } else {
            if (!valueValid) ImGui::PushStyleColor(ImGuiCol_Text, ImGuiExt::GetCustomColorU32(ImGuiCustomCol_LoggerError));
            ImGuiExt::TextFormatted("{}", value);
            if (!valueValid) ImGui::PopStyleColor();
        }

        if (ImGui::CalcTextSize(value.c_str()).x > width) {
            ImGuiExt::InfoTooltip(value.c_str());
        }
    }

    std::string PatternDrawer::getDisplayName(const pl::ptrn::Pattern& pattern) const {
        if (m_showSpecName && pattern.hasAttribute("hex::spec_name"))
            return pattern.getAttributeArguments("hex::spec_name")[0].toString(true);
        else
            return pattern.getDisplayName();
    }

    [[nodiscard]] std::vector<std::string> PatternDrawer::getPatternPath(const pl::ptrn::Pattern *pattern) const {
        std::vector<std::string> result;

        while (pattern != nullptr) {
            result.emplace_back(pattern->getVariableName());
            pattern = pattern->getParent();
        }

        std::reverse(result.begin(), result.end());

        return result;
    }

    bool PatternDrawer::createTreeNode(const pl::ptrn::Pattern& pattern, bool leaf) {
        ImGui::TableNextRow();

        drawFavoriteColumn(pattern);

        bool shouldOpen = false;
        if (m_jumpToPattern != nullptr) {
            if (m_jumpToPattern == &pattern) {
                ImGui::SetScrollHereY();
                m_jumpToPattern = nullptr;
            }
            else {
                auto parent = m_jumpToPattern->getParent();
                while (parent != nullptr) {
                    if (&pattern == parent) {
                        ImGui::SetScrollHereY();
                        shouldOpen = true;
                        break;
                    }

                    parent = parent->getParent();
                }
            }
        }

        ImGui::TableNextColumn();

        if (pattern.isSealed() || leaf) {
            const float indent = ImGui::GetCurrentContext()->FontSize + ImGui::GetStyle().FramePadding.x * 2;
            ImGui::Indent(indent);
            highlightWhenSelected(pattern, [&] {
                ImGui::TextUnformatted(this->getDisplayName(pattern).c_str());
            });
            ImGui::Unindent(indent);
            return false;
        }

        return highlightWhenSelected(pattern, [&]{
            if (shouldOpen)
                ImGui::SetNextItemOpen(true, ImGuiCond_Always);

            ImGui::PushStyleVarX(ImGuiStyleVar_FramePadding, 0.0F);
            bool retVal = false;
            switch (m_treeStyle) {
                using enum TreeStyle;
                default:
                case Default:
                    retVal = ImGui::TreeNodeEx("##TreeNode", ImGuiTreeNodeFlags_DrawLinesToNodes | ImGuiTreeNodeFlags_SpanLabelWidth | ImGuiTreeNodeFlags_OpenOnArrow);
                    break;
                case AutoExpanded:
                    retVal = ImGui::TreeNodeEx("##TreeNode", ImGuiTreeNodeFlags_DrawLinesToNodes | ImGuiTreeNodeFlags_SpanLabelWidth | ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow);
                    break;
                case Flattened:
                    retVal = ImGui::TreeNodeEx("##TreeNode", ImGuiTreeNodeFlags_DrawLinesToNodes | ImGuiTreeNodeFlags_SpanLabelWidth | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen);
                    break;
            }
            ImGui::PopStyleVar();

            ImGui::SameLine();
            ImGui::TextUnformatted(this->getDisplayName(pattern).c_str());

            return retVal;
        });
    }

    void PatternDrawer::makeSelectable(const pl::ptrn::Pattern &pattern) {
        ImGui::PushID(static_cast<int>(pattern.getOffset()));
        ImGui::PushID(pattern.getVariableName().c_str());

        if (ImGui::Selectable("##PatternLine", false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap)) {
            m_selectionCallback(&pattern);

            if (m_editingPattern != nullptr && m_editingPattern != &pattern) {
                this->resetEditing();
            }
        }

        if (ImGui::IsItemHovered()) {
            m_hoverCallback(&pattern);

            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && m_editingEnabled) {
                m_editingPattern = &pattern;
                m_editingPatternOffset = pattern.getOffset();
                AchievementManager::unlockAchievement("hex.builtin.achievement.patterns", "hex.builtin.achievement.patterns.modify_data.name");
            }
        }

        ImGui::SameLine(0, 0);

        ImGui::PopID();
        ImGui::PopID();
    }

    void PatternDrawer::createDefaultEntry(const pl::ptrn::Pattern &pattern) {
        // Draw Name column
        drawNameColumn(pattern, true);

        // Draw Color column
        drawColorColumn(pattern);

        // Draw Start / End columns
        drawOffsetColumns(pattern);

        // Draw Size column
        drawSizeColumn(pattern);

        // Draw type column
        ImGui::TableNextColumn();
        ImGuiExt::TextFormattedColored(TextEditor::getPalette()[u32(TextEditor::PaletteIndex::BuiltInType)], "{}", pattern.getFormattedName().empty() ? pattern.getTypeName() : pattern.getFormattedName());
    }

    void PatternDrawer::closeTreeNode(bool inlined) const {
        if (!inlined && m_treeStyle != TreeStyle::Flattened)
            ImGui::TreePop();
    }


    void PatternDrawer::visit(pl::ptrn::PatternArrayDynamic& pattern) {
        drawArray(pattern, pattern, pattern.isInlined());
    }

    void PatternDrawer::visit(pl::ptrn::PatternArrayStatic& pattern) {
        drawArray(pattern, pattern, pattern.isInlined());
    }

    void PatternDrawer::visit(pl::ptrn::PatternBitfieldField& pattern) {
        drawNameColumn(pattern, true);
        drawColorColumn(pattern);
        drawOffsetColumnForBitfieldMember(pattern);
        drawSizeColumnForBitfieldMember(pattern);
        ImGui::TableNextColumn();

        if (dynamic_cast<pl::ptrn::PatternBitfieldFieldSigned*>(&pattern) != nullptr) {
            ImGuiExt::TextFormattedColored(TextEditor::getPalette()[u32(TextEditor::PaletteIndex::Keyword)], "signed");
            ImGui::SameLine();
            ImGuiExt::TextFormattedColored(TextEditor::getPalette()[u32(TextEditor::PaletteIndex::BuiltInType)], pattern.getBitSize() == 1 ? "bit" : "bits");
        } else if (dynamic_cast<pl::ptrn::PatternBitfieldFieldEnum*>(&pattern) != nullptr) {
            ImGuiExt::TextFormattedColored(TextEditor::getPalette()[u32(TextEditor::PaletteIndex::Keyword)], "enum");
            ImGui::SameLine();
            ImGui::TextUnformatted(pattern.getTypeName().c_str());
        } else if (dynamic_cast<pl::ptrn::PatternBitfieldFieldBoolean*>(&pattern) != nullptr) {
            ImGuiExt::TextFormattedColored(TextEditor::getPalette()[u32(TextEditor::PaletteIndex::BuiltInType)], "bool");
            ImGui::SameLine();
            ImGuiExt::TextFormattedColored(TextEditor::getPalette()[u32(TextEditor::PaletteIndex::BuiltInType)], "bit");
        } else {
            ImGuiExt::TextFormattedColored(TextEditor::getPalette()[u32(TextEditor::PaletteIndex::Keyword)], "unsigned");
            ImGui::SameLine();
            ImGuiExt::TextFormattedColored(TextEditor::getPalette()[u32(TextEditor::PaletteIndex::BuiltInType)], pattern.getBitSize() == 1 ? "bit" : "bits");
        }

        if (!this->isEditingPattern(pattern)) {
            drawValueColumn(pattern);
        } else {
            ImGui::TableNextColumn();
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);

            m_valueEditor.visit(pattern);

            ImGui::PopItemWidth();
            ImGui::PopStyleVar();
        }

        drawCommentColumn(pattern);
    }

    void PatternDrawer::visit(pl::ptrn::PatternBitfieldArray& pattern) {
        drawArray(pattern, pattern, pattern.isInlined());
    }

    void PatternDrawer::visit(pl::ptrn::PatternBitfield& pattern) {
        bool open = true;
        if (!pattern.isInlined() && m_treeStyle != TreeStyle::Flattened) {
            open = drawNameColumn(pattern);

            if (pattern.isSealed())
                drawColorColumn(pattern);
            else
                ImGui::TableNextColumn();

            drawOffsetColumns(pattern);
            drawSizeColumn(pattern);
            drawTypeNameColumn(pattern, "bitfield");

            drawValueColumn(pattern);
            drawCommentColumn(pattern);
        }

        if (!open) {
            return;
        }

        int id = 1;
        pattern.forEachEntry(0, pattern.getEntryCount(), [&] (u64, auto *field) {
            ImGui::PushID(id);
            this->draw(*field);
            ImGui::PopID();

            id += 1;
        });

        closeTreeNode(pattern.isInlined());
    }

    void PatternDrawer::visit(pl::ptrn::PatternBoolean& pattern) {
        createDefaultEntry(pattern);

        if (!this->isEditingPattern(pattern)) {
            drawValueColumn(pattern);
        } else {
            ImGui::TableNextColumn();
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);

            m_valueEditor.visit(pattern);

            ImGui::PopItemWidth();
            ImGui::PopStyleVar();
        }

        drawCommentColumn(pattern);
    }

    void PatternDrawer::visit(pl::ptrn::PatternCharacter& pattern) {
        createDefaultEntry(pattern);

        if (!this->isEditingPattern(pattern)) {
            drawValueColumn(pattern);
        } else {
            ImGui::TableNextColumn();
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
            ImGui::SetKeyboardFocusHere();

            m_valueEditor.visit(pattern);

            ImGui::PopItemWidth();
            ImGui::PopStyleVar();
        }

        drawCommentColumn(pattern);
    }

    void PatternDrawer::visit(pl::ptrn::PatternEnum& pattern) {
        drawNameColumn(pattern, true);
        drawColorColumn(pattern);
        drawOffsetColumns(pattern);
        drawSizeColumn(pattern);
        drawTypeNameColumn(pattern, "enum");

        if (!this->isEditingPattern(pattern)) {
            drawValueColumn(pattern);
        } else {
            ImGui::TableNextColumn();
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);

            m_valueEditor.visit(pattern);

            ImGui::PopItemWidth();
            ImGui::PopStyleVar();
        }

        drawCommentColumn(pattern);
    }

    void PatternDrawer::visit(pl::ptrn::PatternFloat& pattern) {
        createDefaultEntry(pattern);

        if (!this->isEditingPattern(pattern)) {
            drawValueColumn(pattern);
        } else {
            ImGui::TableNextColumn();
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
            ImGui::SetKeyboardFocusHere();

            m_valueEditor.visit(pattern);

            ImGui::PopItemWidth();
            ImGui::PopStyleVar();
        }

        drawCommentColumn(pattern);
    }

    void PatternDrawer::visit(pl::ptrn::PatternPadding& pattern) {
        // Do nothing
        std::ignore = pattern;
    }

    void PatternDrawer::visit(pl::ptrn::PatternPointer& pattern) {
        bool open = true;

        if (!pattern.isInlined() && m_treeStyle != TreeStyle::Flattened) {
            open = drawNameColumn(pattern);
            drawColorColumn(pattern);
            drawOffsetColumns(pattern);
            drawSizeColumn(pattern);
            ImGui::TableNextColumn();
            ImGuiExt::TextFormattedColored(TextEditor::getPalette()[u32(TextEditor::PaletteIndex::BuiltInType)], "{}", pattern.getFormattedName());
            drawValueColumn(pattern);
            drawCommentColumn(pattern);
        }

        if (open) {
            pattern.getPointedAtPattern()->accept(*this);

            closeTreeNode(pattern.isInlined());
        }
    }

    void PatternDrawer::visit(pl::ptrn::PatternSigned& pattern) {
        createDefaultEntry(pattern);

        if (!this->isEditingPattern(pattern)) {
            drawValueColumn(pattern);
        } else {
            ImGui::TableNextColumn();
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
            ImGui::SetKeyboardFocusHere();

            m_valueEditor.visit(pattern);

            ImGui::PopItemWidth();
            ImGui::PopStyleVar();
        }

        drawCommentColumn(pattern);
    }

    void PatternDrawer::visit(pl::ptrn::PatternString& pattern) {
        if (pattern.getSize() > 0) {
            createDefaultEntry(pattern);

            if (!this->isEditingPattern(pattern)) {
                drawValueColumn(pattern);
            } else {
                ImGui::TableNextColumn();
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::SetKeyboardFocusHere();

                m_valueEditor.visit(pattern);

                ImGui::PopItemWidth();
                ImGui::PopStyleVar();
            }

            drawCommentColumn(pattern);
        }
    }

    void PatternDrawer::visit(pl::ptrn::PatternStruct& pattern) {
        bool open = true;

        if (!pattern.isInlined() && m_treeStyle != TreeStyle::Flattened) {
            open = drawNameColumn(pattern);
            if (pattern.isSealed())
                drawColorColumn(pattern);
            else
                ImGui::TableNextColumn();
            drawOffsetColumns(pattern);
            drawSizeColumn(pattern);
            drawTypeNameColumn(pattern, "struct");

            if (this->isEditingPattern(pattern) && !pattern.getWriteFormatterFunction().empty()) {
                ImGui::TableNextColumn();
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::SetKeyboardFocusHere();

                m_valueEditor.visit(pattern);

                ImGui::PopItemWidth();
                ImGui::PopStyleVar();
            } else {
                drawValueColumn(pattern);
            }

            drawCommentColumn(pattern);
        }

        if (!open) {
            return;
        }

        int id = 1;
        pattern.forEachEntry(0, pattern.getEntryCount(), [&](u64, auto *member){
            ImGui::PushID(id);
            this->draw(*member);
            ImGui::PopID();
            id += 1;
        });

        closeTreeNode(pattern.isInlined());
    }

    void PatternDrawer::visit(pl::ptrn::PatternUnion& pattern) {
        bool open = true;

        if (!pattern.isInlined() && m_treeStyle != TreeStyle::Flattened) {
            open = drawNameColumn(pattern);
            if (pattern.isSealed())
                drawColorColumn(pattern);
            else
                ImGui::TableNextColumn();
            drawOffsetColumns(pattern);
            drawSizeColumn(pattern);
            drawTypeNameColumn(pattern, "union");

            if (this->isEditingPattern(pattern) && !pattern.getWriteFormatterFunction().empty()) {
                ImGui::TableNextColumn();
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::SetKeyboardFocusHere();

                m_valueEditor.visit(pattern);

                ImGui::PopItemWidth();
                ImGui::PopStyleVar();
            } else {
                drawValueColumn(pattern);
            }

            drawCommentColumn(pattern);
        }

        if (!open) {
            return;
        }

        int id = 1;
        pattern.forEachEntry(0, pattern.getEntryCount(), [&](u64, auto *member) {
            ImGui::PushID(id);
            this->draw(*member);
            ImGui::PopID();

            id += 1;
        });

        closeTreeNode(pattern.isInlined());
    }

    void PatternDrawer::visit(pl::ptrn::PatternUnsigned& pattern) {
        createDefaultEntry(pattern);

        if (!this->isEditingPattern(pattern)) {
            drawValueColumn(pattern);
        } else {
            ImGui::TableNextColumn();
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
            ImGui::SetKeyboardFocusHere();

            m_valueEditor.visit(pattern);

            ImGui::PopItemWidth();
            ImGui::PopStyleVar();
        }

        drawCommentColumn(pattern);
    }

    void PatternDrawer::visit(pl::ptrn::PatternWideCharacter& pattern) {
        createDefaultEntry(pattern);
        drawValueColumn(pattern);
        drawCommentColumn(pattern);
    }

    void PatternDrawer::visit(pl::ptrn::PatternWideString& pattern) {
        if (pattern.getSize() > 0) {
            createDefaultEntry(pattern);
            drawValueColumn(pattern);
            drawCommentColumn(pattern);
        }
    }

    void PatternDrawer::visit(pl::ptrn::PatternError& pattern) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_LoggerError));
        createDefaultEntry(pattern);
        drawValueColumn(pattern);
        drawCommentColumn(pattern);
        ImGui::PopStyleColor();
    }


    void PatternDrawer::visit(pl::ptrn::Pattern& pattern) {
        createDefaultEntry(pattern);
        drawValueColumn(pattern);
        drawCommentColumn(pattern);
    }

    void PatternDrawer::draw(pl::ptrn::Pattern& pattern) {
        if (pattern.getVisibility() == pl::ptrn::Visibility::Hidden)
            return;
        if (pattern.getVisibility() == pl::ptrn::Visibility::TreeHidden)
            return;

        m_currPatternPath.push_back(pattern.getVariableName());
        ON_SCOPE_EXIT { m_currPatternPath.pop_back(); };

        pattern.accept(*this);
    }

    void PatternDrawer::drawArray(pl::ptrn::Pattern& pattern, pl::ptrn::IIterable &iterable, bool isInlined) {
        if (iterable.getEntryCount() == 0)
            return;

        bool open = true;
        if (!isInlined && m_treeStyle != TreeStyle::Flattened) {
            open = drawNameColumn(pattern);
            if (pattern.isSealed())
                drawColorColumn(pattern);
            else
                ImGui::TableNextColumn();
            drawOffsetColumns(pattern);
            drawSizeColumn(pattern);

            ImGui::TableNextColumn();
            ImGuiExt::TextFormattedColored(TextEditor::getPalette()[u32(TextEditor::PaletteIndex::BuiltInType)], "{0}", pattern.getTypeName());
            ImGui::SameLine(0, 0);

            ImGui::TextUnformatted("[");
            ImGui::SameLine(0, 0);
            ImGuiExt::TextFormattedColored(TextEditor::getPalette()[u32(TextEditor::PaletteIndex::NumericLiteral)], "{0}", iterable.getEntryCount());
            ImGui::SameLine(0, 0);
            ImGui::TextUnformatted("]");

            drawValueColumn(pattern);
            drawCommentColumn(pattern);
        }

        if (!open) {
            return;
        }

        u64 chunkCount = 0;
        for (u64 i = 0; i < iterable.getEntryCount(); i += ChunkSize) {
            chunkCount++;

            auto &displayEnd = this->getDisplayEnd(pattern);
            if (chunkCount > displayEnd) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TableNextColumn();

                ImGui::Selectable(fmt::format("... ({})", "hex.ui.pattern_drawer.double_click"_lang).c_str(), false, ImGuiSelectableFlags_SpanAllColumns);
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    displayEnd += DisplayEndStep;
                break;
            }

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
                    ImGui::PushStyleVarX(ImGuiStyleVar_FramePadding, 0.0F);
                    const auto result = ImGui::TreeNodeEx(fmt::format("##TreeNode_{:X}", endOffset).c_str(), ImGuiTreeNodeFlags_DrawLinesToNodes | ImGuiTreeNodeFlags_SpanLabelWidth | ImGuiTreeNodeFlags_OpenOnArrow);
                    ImGui::PopStyleVar();
                    ImGui::SameLine();
                    ImGui::TextUnformatted(fmt::format("{0}[{1} ... {2}]", m_treeStyle == TreeStyle::Flattened ? this->getDisplayName(pattern).c_str() : "", i, endIndex - 1).c_str());
                    return result;
                });

                ImGui::TableNextColumn();

                if (!pattern.isLocal()) {
                    ImGui::TableNextColumn();
                    ImGuiExt::TextFormatted("0x{0:08X}", startOffset);
                    ImGui::TableNextColumn();
                    ImGuiExt::TextFormatted("0x{0:08X}", endOffset + endSize - (endSize == 0 ? 0 : 1));
                } else {
                    ImGui::TableNextColumn();
                    ImGuiExt::TextFormatted("[{}]", "hex.ui.pattern_drawer.local"_lang);
                    ImGui::TableNextColumn();
                    ImGuiExt::TextFormatted("[{}]", "hex.ui.pattern_drawer.local"_lang);
                }

                ImGui::TableNextColumn();
                ImGuiExt::TextFormatted("{0} {1}", chunkSize, chunkSize == 1 ? "byte" : "bytes");
                ImGui::TableNextColumn();
                ImGuiExt::TextFormattedColored(TextEditor::getPalette()[u32(TextEditor::PaletteIndex::BuiltInType)], "{0}", pattern.getTypeName());
                ImGui::SameLine(0, 0);

                ImGui::TextUnformatted("[");
                ImGui::SameLine(0, 0);
                ImGuiExt::TextFormattedColored(TextEditor::getPalette()[u32(TextEditor::PaletteIndex::NumericLiteral)], "{0}", endIndex - i);
                ImGui::SameLine(0, 0);
                ImGui::TextUnformatted("]");

                ImGui::TableNextColumn();
                ImGuiExt::TextFormatted("[ ... ]");
            }


            if (!chunkOpen) {
                continue;
            }

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

        closeTreeNode(isInlined);
    }

    u64& PatternDrawer::getDisplayEnd(const pl::ptrn::Pattern& pattern) {
        auto it = m_displayEnd.find(&pattern);
        if (it != m_displayEnd.end()) {
            return it->second;
        }

        auto [value, success] = m_displayEnd.emplace(&pattern, DisplayEndDefault);
        return value->second;
    }

    bool PatternDrawer::sortPatterns(const ImGuiTableSortSpecs* sortSpecs, const pl::ptrn::Pattern * left, const pl::ptrn::Pattern * right) const {
        auto result = std::strong_ordering::equal;

        if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("name")) {
            result = this->getDisplayName(*left) <=> this->getDisplayName(*right);
        } else if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("start")) {
            result = left->getOffsetForSorting() <=> right->getOffsetForSorting();
        } else if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("end")) {
            result = (left->getOffsetForSorting() + left->getSizeForSorting()) <=> (right->getOffsetForSorting() + right->getSizeForSorting());
        } else if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("size")) {
            result = left->getSizeForSorting() <=> right->getSizeForSorting();
        } else if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("value")) {
            result = left->getValue() <=> right->getValue();
        } else if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("type")) {
            result = left->getTypeName() <=> right->getTypeName();
        } else if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("color")) {
            result = left->getColor() <=> right->getColor();
        } else if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("comment")) {
            result = left->getComment() <=> right->getComment();
        }

        return sortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending ? result == std::strong_ordering::less : result == std::strong_ordering::greater;
    }

    bool PatternDrawer::beginPatternTable(const std::vector<std::shared_ptr<pl::ptrn::Pattern>> &patterns, std::vector<pl::ptrn::Pattern*> &sortedPatterns, float height) const {
        if (!ImGui::BeginTable("##Patterntable", 9, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable | ImGuiTableFlags_Hideable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, height))) {
            return false;
        }

        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("hex.ui.pattern_drawer.favorites"_lang, ImGuiTableColumnFlags_NoHeaderLabel | ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_NoReorder | ImGuiTableColumnFlags_IndentDisable | (m_favorites.empty() ? ImGuiTableColumnFlags_None : ImGuiTableColumnFlags_NoHide), ImGui::GetTextLineHeight(), ImGui::GetID("favorite"));
        ImGui::TableSetupColumn("hex.ui.pattern_drawer.var_name"_lang,  ImGuiTableColumnFlags_PreferSortAscending | ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_IndentEnable, 0, ImGui::GetID("name"));
        ImGui::TableSetupColumn("hex.ui.pattern_drawer.color"_lang,     ImGuiTableColumnFlags_PreferSortAscending, 0, ImGui::GetID("color"));
        ImGui::TableSetupColumn("hex.ui.pattern_drawer.start"_lang,     ImGuiTableColumnFlags_PreferSortAscending | ImGuiTableColumnFlags_DefaultSort, 0, ImGui::GetID("start"));
        ImGui::TableSetupColumn("hex.ui.pattern_drawer.end"_lang,       ImGuiTableColumnFlags_PreferSortAscending | ImGuiTableColumnFlags_DefaultSort, 0, ImGui::GetID("end"));
        ImGui::TableSetupColumn("hex.ui.pattern_drawer.size"_lang,      ImGuiTableColumnFlags_PreferSortAscending, 0, ImGui::GetID("size"));
        ImGui::TableSetupColumn("hex.ui.pattern_drawer.type"_lang,      ImGuiTableColumnFlags_PreferSortAscending, 0, ImGui::GetID("type"));
        ImGui::TableSetupColumn("hex.ui.pattern_drawer.value"_lang,     ImGuiTableColumnFlags_PreferSortAscending, 0, ImGui::GetID("value"));
        ImGui::TableSetupColumn("hex.ui.pattern_drawer.comment"_lang,   ImGuiTableColumnFlags_PreferSortAscending | ImGuiTableColumnFlags_DefaultHide, 0, ImGui::GetID("comment"));

        auto sortSpecs = ImGui::TableGetSortSpecs();

        if (patterns.empty()) {
            sortedPatterns.clear();
            return true;
        }

        if (!sortSpecs->SpecsDirty && !sortedPatterns.empty()) {
            return true;
        }

        if (!m_favoritesUpdateTask.isRunning()) {
            sortedPatterns.clear();
            std::transform(patterns.begin(), patterns.end(), std::back_inserter(sortedPatterns), [](const std::shared_ptr<pl::ptrn::Pattern> &pattern) {
                return pattern.get();
            });

            std::stable_sort(sortedPatterns.begin(), sortedPatterns.end(), [this, &sortSpecs](const pl::ptrn::Pattern *left, const pl::ptrn::Pattern *right) -> bool {
                return this->sortPatterns(sortSpecs, left, right);
            });

            for (auto &pattern : sortedPatterns) {
                pattern->sort([this, &sortSpecs](const pl::ptrn::Pattern *left, const pl::ptrn::Pattern *right){
                    return this->sortPatterns(sortSpecs, left, right);
                });
            }

            sortSpecs->SpecsDirty = false;
        }

        return true;
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

    void PatternDrawer::draw(const std::vector<std::shared_ptr<pl::ptrn::Pattern>> &patterns, const pl::PatternLanguage *runtime, float height) {
        if (runtime == nullptr) {
            this->reset();
        } else {
            auto runId = runtime->getRunId();
            if (runId != m_lastRunId) {
                this->reset();
                m_lastRunId = runId;
            }
        }

        std::scoped_lock lock(s_resetDrawMutex);

        m_hoverCallback(nullptr);

        const auto treeStyleButton = [this](auto icon, TreeStyle style, const char *tooltip) {
            bool pushed = false;

            if (m_treeStyle == style) {
                ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
                pushed = true;
            }

            if (ImGuiExt::DimmedIconButton(icon, ImGui::GetStyleColorVec4(ImGuiCol_Text)))
                m_treeStyle = style;

            if (pushed)
                ImGui::PopStyleColor();

            ImGuiExt::InfoTooltip(tooltip);
        };

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered()) {
            this->resetEditing();
        }

        ImGui::PushItemWidth(-(ImGui::GetTextLineHeightWithSpacing() * 8));
        if (ImGuiExt::InputTextIcon("##Search", ICON_VS_FILTER, m_filterText)) {
            m_filter = parseRValueFilter(m_filterText).value_or(Filter{ });
            updateFilter();
        }
        ImGui::PopItemWidth();

        ImGui::SameLine();

        ImGuiExt::DimmedIconToggle(ICON_VS_BOOK, &m_showSpecName);
        ImGuiExt::InfoTooltip("hex.ui.pattern_drawer.spec_name"_lang);

        ImGui::SameLine();

        treeStyleButton(ICON_VS_SYMBOL_KEYWORD, TreeStyle::Default,         "hex.ui.pattern_drawer.tree_style.tree"_lang);
        ImGui::SameLine(0, 0);
        treeStyleButton(ICON_VS_LIST_TREE,      TreeStyle::AutoExpanded,    "hex.ui.pattern_drawer.tree_style.auto_expanded"_lang);
        ImGui::SameLine(0, 0);
        treeStyleButton(ICON_VS_LIST_FLAT,      TreeStyle::Flattened,       "hex.ui.pattern_drawer.tree_style.flattened"_lang);

        ImGui::SameLine(0, 15_scaled);

        const auto startPos = ImGui::GetCursorPos();

        ImGui::BeginDisabled(runtime == nullptr);
        if (ImGuiExt::DimmedIconButton(ICON_VS_EXPORT, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
            ImGui::OpenPopup("ExportPatterns");
        }
        ImGui::EndDisabled();

        ImGuiExt::InfoTooltip("hex.ui.pattern_drawer.export"_lang);

        ImGui::SetNextWindowPos(ImGui::GetWindowPos() + ImVec2(startPos.x, ImGui::GetCursorPosY()));
        if (ImGui::BeginPopup("ExportPatterns")) {
            for (const auto &formatter : m_formatters) {
                const auto name = [&]{
                    auto formatterName = formatter->getName();
                    std::transform(formatterName.begin(), formatterName.end(), formatterName.begin(), [](char c){ return char(std::toupper(c)); });

                    return formatterName;
                }();

                const auto &extension = formatter->getFileExtension();

                if (ImGui::MenuItem(name.c_str())) {
                    fs::openFileBrowser(fs::DialogMode::Save, { fs::ItemFilter(name, extension) }, [&](const std::fs::path &path) {
                        auto result = formatter->format(*runtime);

                        wolv::io::File output(path, wolv::io::File::Mode::Create);
                        output.writeVector(result);
                    });
                }
            }
            ImGui::EndPopup();
        }

        if (beginPatternTable(patterns, m_sortedPatterns, height)) {
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImGui::GetColorU32(ImGuiCol_HeaderHovered, 0.4F));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImGui::GetColorU32(ImGuiCol_HeaderActive, 0.4F));
            ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, ImGui::CalcTextSize(" ").x * 2);
            ImGui::TableHeadersRow();

            m_showFavoriteStars = false;
            if (!m_favoritesUpdateTask.isRunning()) {
                int id = 1;
                bool doTableNextRow = false;

                if (!m_favorites.empty() && !patterns.empty()) {
                    ImGui::TableNextColumn();
                    ImGui::TableNextColumn();
                    ImGui::PushID(id);
                    const auto open = ImGui::TreeNodeEx("##Favorites", ImGuiTreeNodeFlags_DrawLinesToNodes | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_OpenOnArrow);
                    ImGui::SameLine();
                    ImGui::TextUnformatted("hex.ui.pattern_drawer.favorites"_lang);
                    if (open) {
                        for (auto &[path, pattern] : m_favorites) {
                            if (pattern == nullptr)
                                continue;

                            ImGui::PushID(pattern->getDisplayName().c_str());
                            this->draw(*pattern);
                            ImGui::PopID();
                        }

                        ImGui::TreePop();
                    }
                    ImGui::PopID();

                    id += 1;
                    doTableNextRow = true;
                }

                if (!m_groups.empty() && !patterns.empty()) {
                    for (auto &[groupName, groupPatterns]: m_groups) {
                        if (doTableNextRow) {
                            ImGui::TableNextRow();
                        }

                        ImGui::TableNextColumn();
                        ImGui::TableNextColumn();
                        ImGui::PushID(id);
                        if (ImGui::TreeNodeEx(groupName.c_str(), ImGuiTreeNodeFlags_DrawLinesToNodes | ImGuiTreeNodeFlags_SpanFullWidth)) {
                            for (auto &groupPattern: groupPatterns) {
                                if (groupPattern == nullptr)
                                    continue;

                                ImGui::PushID(id);
                                this->draw(*groupPattern);
                                ImGui::PopID();

                                id += 1;
                            }

                            ImGui::TreePop();
                        }
                        ImGui::PopID();

                        id += 1;
                        doTableNextRow = true;
                    }
                }

                m_showFavoriteStars = true;

                for (auto &pattern : m_filter.path.empty() ? m_sortedPatterns : m_filteredPatterns) {
                    ImGui::PushID(id);
                    this->draw(*pattern);
                    ImGui::PopID();

                    id += 1;
                }
            }

            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);

            ImGui::EndTable();
        }

        if (!m_filtersUpdated && !patterns.empty()) {
            m_filtersUpdated = true;

            if (!m_favoritesUpdateTask.isRunning()) {
                m_favoritesUpdateTask = TaskManager::createTask("hex.ui.pattern_drawer.updating", TaskManager::NoProgress, [this, patterns, runtime](auto &task) {
                    size_t updatedFavorites = 0;

                    {
                        const auto favorites = runtime->getPatternsWithAttribute("hex::favorite");
                        for (const auto &pattern : favorites) {
                            m_favorites.insert({ getPatternPath(pattern), pattern->clone() });
                        }

                        const auto groupAttribute = "hex::group";
                        const auto groups = runtime->getPatternsWithAttribute(groupAttribute);
                        for (const auto &pattern : groups) {
                            const auto arguments = pattern->getAttributeArguments(groupAttribute);
                            if (!arguments.empty()) {
                                const auto &groupName = arguments.front().toString();
                                if (!m_groups.contains(groupName))
                                    m_groups.insert({ groupName, { } });

                                m_groups[groupName].push_back(pattern->clone());
                            }
                        }
                    }

                    for (auto &pattern : patterns) {
                        std::vector<std::string> patternPath;

                        size_t startFavoriteCount = m_favorites.size();
                        if (startFavoriteCount == m_favorites.size())
                            continue;

                        patternPath.clear();
                        traversePatternTree(*pattern, patternPath, [&, this](const pl::ptrn::Pattern &currPattern) {
                            for (auto &[path, favoritePattern] : m_favorites) {
                                if (updatedFavorites == m_favorites.size())
                                    task.interrupt();
                                task.update();

                                if (matchesFilter(patternPath, path, true)) {
                                    favoritePattern = currPattern.clone();
                                    updatedFavorites += 1;

                                    break;
                                }
                            }
                        });
                    }

                    std::erase_if(m_favorites, [](const auto &entry) {
                        const auto &[path, favoritePattern] = entry;

                        return favoritePattern == nullptr;
                    });
                });
            }

            updateFilter();

        }

        m_jumpToPattern = nullptr;

        if (m_favoritesUpdateTask.isRunning()) {
            ImGuiExt::TextOverlay("hex.ui.pattern_drawer.updating"_lang, ImGui::GetWindowPos() + ImGui::GetWindowSize() / 2, ImGui::GetWindowWidth() * 0.5);
        }
    }

    void PatternDrawer::reset() {
        std::scoped_lock lock(s_resetDrawMutex);

        this->resetEditing();
        m_displayEnd.clear();
        m_visualizedPatterns.clear();
        m_currVisualizedPattern = nullptr;
        m_sortedPatterns.clear();
        m_filteredPatterns.clear();
        m_visualizerDrawer.clearLastVisualizerError();
        m_currPatternPath.clear();

        m_favoritesUpdateTask.interrupt();

        for (auto &[path, pattern] : m_favorites)
            pattern = nullptr;
        for (auto &[groupName, patterns]: m_groups) {
            for (auto &pattern: patterns)
                pattern = nullptr;
        }

        m_groups.clear();

        m_filtersUpdated = false;
    }
}