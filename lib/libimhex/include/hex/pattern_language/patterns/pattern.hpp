#pragma once

#include <hex.hpp>

#include <imgui.h>
#include <hex/ui/imgui_imhex_extensions.h>

#include <hex/pattern_language/evaluator.hpp>
#include <hex/providers/provider.hpp>

#include <string>

namespace hex::pl {

    using namespace ::std::literals::string_literals;

    class Inlinable {
    public:
        [[nodiscard]] bool isInlined() const { return this->m_inlined; }
        void setInlined(bool inlined) { this->m_inlined = inlined; }

    private:
        bool m_inlined = false;
    };

    class PatternCreationLimiter {
    public:
        explicit PatternCreationLimiter(Evaluator *evaluator) : m_evaluator(evaluator) {
            if (getEvaluator() == nullptr) return;

            getEvaluator()->patternCreated();
        }

        PatternCreationLimiter(const PatternCreationLimiter &other) : PatternCreationLimiter(other.m_evaluator) { }

        virtual ~PatternCreationLimiter() {
            if (getEvaluator() == nullptr) return;

            getEvaluator()->patternDestroyed();
        }

        [[nodiscard]] Evaluator *getEvaluator() const {
            return this->m_evaluator;
        }

    private:
        Evaluator *m_evaluator = nullptr;
    };

    class Pattern : public PatternCreationLimiter,
                    public Cloneable<Pattern> {
    public:
        Pattern(Evaluator *evaluator, u64 offset, size_t size, u32 color = 0)
            : PatternCreationLimiter(evaluator), m_offset(offset), m_size(size), m_color(color) {

            if (color != 0)
                return;

            this->m_color       = ContentRegistry::PatternLanguage::getNextColor();
            this->m_manualColor = false;
        }

        Pattern(const Pattern &other) = default;

        ~Pattern() override = default;

        [[nodiscard]] u64 getOffset() const { return this->m_offset; }
        virtual void setOffset(u64 offset) { this->m_offset = offset; }

        [[nodiscard]] size_t getSize() const { return this->m_size; }
        void setSize(size_t size) { this->m_size = size; }

        [[nodiscard]] const std::string &getVariableName() const { return this->m_variableName; }
        void setVariableName(std::string name) { this->m_variableName = std::move(name); }

        [[nodiscard]] const std::optional<std::string> &getComment() const { return this->m_comment; }
        void setComment(std::string comment) { this->m_comment = std::move(comment); }

        [[nodiscard]] const std::string &getTypeName() const { return this->m_typeName; }
        void setTypeName(std::string name) { this->m_typeName = std::move(name); }

        [[nodiscard]] u32 getColor() const { return this->m_color; }
        virtual void setColor(u32 color) {
            this->m_color       = color;
            this->m_manualColor = true;
        }
        [[nodiscard]] bool hasOverriddenColor() const { return this->m_manualColor; }

        [[nodiscard]] std::endian getEndian() const {
            if (this->getEvaluator() == nullptr) return std::endian::native;
            else return this->m_endian.value_or(this->getEvaluator()->getDefaultEndian());
        }
        virtual void setEndian(std::endian endian) { this->m_endian = endian; }
        [[nodiscard]] bool hasOverriddenEndian() const { return this->m_endian.has_value(); }

        [[nodiscard]] std::string getDisplayName() const { return this->m_displayName.value_or(this->m_variableName); }
        void setDisplayName(const std::string &name) { this->m_displayName = name; }

        [[nodiscard]] const auto &getTransformFunction() const { return this->m_transformFunction; }
        void setTransformFunction(const ContentRegistry::PatternLanguage::Function &function) { this->m_transformFunction = function; }
        [[nodiscard]] const auto &getFormatterFunction() const { return this->m_formatterFunction; }
        void setFormatterFunction(const ContentRegistry::PatternLanguage::Function &function) { this->m_formatterFunction = function; }

        virtual void createEntry(prv::Provider *&provider)         = 0;
        [[nodiscard]] virtual std::string getFormattedName() const = 0;

        [[nodiscard]] virtual const Pattern *getPattern(u64 offset) const {
            if (offset >= this->getOffset() && offset < (this->getOffset() + this->getSize()) && !this->isHidden())
                return this;
            else
                return nullptr;
        }

        virtual void getHighlightedAddresses(std::map<u64, u32> &highlight) const {
            if (this->isHidden()) return;

            for (u64 i = 0; i < this->getSize(); i++)
                highlight.insert({ this->getOffset() + i, this->getColor() });

            this->getEvaluator()->handleAbort();
        }

        virtual void sort(ImGuiTableSortSpecs *sortSpecs, prv::Provider *provider) { }

        [[nodiscard]] virtual std::string toString(prv::Provider *provider) const {
            return hex::format("{} {} @ 0x{:X}", this->getTypeName(), this->getVariableName(), this->getOffset());
        }

        static bool sortPatternTable(ImGuiTableSortSpecs *sortSpecs, prv::Provider *provider, pl::Pattern *left, pl::Pattern *right) {
            if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("name")) {
                if (sortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending)
                    return left->getDisplayName() > right->getDisplayName();
                else
                    return left->getDisplayName() < right->getDisplayName();
            } else if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("offset")) {
                if (sortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending)
                    return left->getOffset() > right->getOffset();
                else
                    return left->getOffset() < right->getOffset();
            } else if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("size")) {
                if (sortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending)
                    return left->getSize() > right->getSize();
                else
                    return left->getSize() < right->getSize();
            } else if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("value")) {
                size_t biggerSize = std::max(left->getSize(), right->getSize());
                std::vector<u8> leftBuffer(biggerSize, 0x00), rightBuffer(biggerSize, 0x00);

                provider->read(left->getOffset(), leftBuffer.data(), left->getSize());
                provider->read(right->getOffset(), rightBuffer.data(), right->getSize());

                if (left->m_endian != std::endian::native)
                    std::reverse(leftBuffer.begin(), leftBuffer.end());
                if (right->m_endian != std::endian::native)
                    std::reverse(rightBuffer.begin(), rightBuffer.end());

                if (sortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending)
                    return leftBuffer > rightBuffer;
                else
                    return leftBuffer < rightBuffer;
            } else if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("type")) {
                if (sortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending)
                    return left->getTypeName() > right->getTypeName();
                else
                    return left->getTypeName() < right->getTypeName();
            } else if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("color")) {
                if (sortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending)
                    return left->getColor() > right->getColor();
                else
                    return left->getColor() < right->getColor();
            }

            return false;
        }

        void draw(prv::Provider *provider) {
            if (isHidden()) return;

            this->createEntry(provider);
        }

        void setHidden(bool hidden) {
            this->m_hidden = hidden;
        }

        [[nodiscard]] bool isHidden() const {
            return this->m_hidden;
        }

        void setLocal(bool local) {
            this->m_local = local;
        }

        [[nodiscard]] bool isLocal() const {
            return this->m_local;
        }

        [[nodiscard]] virtual bool operator!=(const Pattern &other) const final { return !operator==(other); }
        [[nodiscard]] virtual bool operator==(const Pattern &other) const = 0;

        template<typename T>
        [[nodiscard]] bool areCommonPropertiesEqual(const Pattern &other) const {
            return typeid(other) == typeid(std::remove_cvref_t<T>) &&
                   this->m_offset == other.m_offset &&
                   this->m_size == other.m_size &&
                   this->m_hidden == other.m_hidden &&
                   (this->m_endian == other.m_endian || (!this->m_endian.has_value() && other.m_endian == std::endian::native) || (!other.m_endian.has_value() && this->m_endian == std::endian::native)) &&
                   this->m_variableName == other.m_variableName &&
                   this->m_typeName == other.m_typeName &&
                   this->m_comment == other.m_comment &&
                   this->m_local == other.m_local;
        }

        [[nodiscard]] std::string formatDisplayValue(const std::string &value, const Token::Literal &literal) const {
            if (!this->m_formatterFunction.has_value())
                return value;
            else {
                try {
                    auto result = this->m_formatterFunction->func(this->getEvaluator(), { literal });

                    if (result.has_value()) {
                        if (auto displayValue = std::get_if<std::string>(&result.value()); displayValue != nullptr)
                            return *displayValue;
                        else
                            return "???";
                    } else {
                        return "???";
                    }
                } catch (PatternLanguageError &error) {
                    return "Error: "s + error.what();
                }
            }
        }

    protected:
        void createDefaultEntry(const std::string &value, const Token::Literal &literal) const {
            ImGui::TableNextRow();
            ImGui::TreeNodeEx(this->getDisplayName().c_str(), ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_AllowItemOverlap);
            ImGui::TableNextColumn();

            ImGui::PushID(this->getOffset());
            ImGui::PushID(this->getVariableName().c_str());
            if (ImGui::Selectable("##PatternLine", false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap)) {
                ImHexApi::HexEditor::setSelection(this->getOffset(), this->getSize());
            }
            ImGui::PopID();
            ImGui::PopID();

            this->drawCommentTooltip();
            ImGui::SameLine();
            ImGui::TextUnformatted(this->getDisplayName().c_str());
            ImGui::TableNextColumn();
            ImGui::ColorButton("color", ImColor(this->getColor()), ImGuiColorEditFlags_NoTooltip, ImVec2(ImGui::GetColumnWidth(), ImGui::GetTextLineHeight()));
            ImGui::TableNextColumn();
            ImGui::TextFormatted("0x{0:08X} : 0x{1:08X}", this->getOffset(), this->getOffset() + this->getSize() - 1);
            ImGui::TableNextColumn();
            ImGui::TextFormatted("0x{0:04X}", this->getSize());
            ImGui::TableNextColumn();
            ImGui::TextFormattedColored(ImColor(0xFF9BC64D), "{}", this->getFormattedName());
            ImGui::TableNextColumn();
            ImGui::TextFormatted("{}", formatDisplayValue(value, literal));
        }

        void drawCommentTooltip() const {
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) && this->getComment().has_value()) {
                ImGui::BeginTooltip();
                ImGui::TextUnformatted(this->getComment()->c_str());
                ImGui::EndTooltip();
            }
        }

    protected:
        std::optional<std::endian> m_endian;
        bool m_hidden = false;

    private:
        u64 m_offset  = 0x00;
        size_t m_size = 0x00;

        u32 m_color = 0x00;
        std::optional<std::string> m_displayName;
        std::string m_variableName;
        std::optional<std::string> m_comment;
        std::string m_typeName;

        std::optional<ContentRegistry::PatternLanguage::Function> m_formatterFunction;
        std::optional<ContentRegistry::PatternLanguage::Function> m_transformFunction;

        bool m_local       = false;
        bool m_manualColor = false;
    };

}