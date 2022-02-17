#pragma once

#include <hex.hpp>

#include <imgui.h>

#include <hex/providers/provider.hpp>
#include <hex/pattern_language/token.hpp>
#include <hex/pattern_language/evaluator.hpp>

#include <hex/ui/view.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/helpers/concepts.hpp>
#include <hex/helpers/logger.hpp>

#include <cstring>
#include <codecvt>
#include <locale>
#include <random>
#include <string>
#include <type_traits>

namespace hex::pl {

    using namespace ::std::literals::string_literals;

    namespace {

        std::string makeDisplayable(const std::string &string) {
            std::string result;
            for (auto &c : string) {
                if (iscntrl(c) || c > 0x7F)
                    result += " ";
                else
                    result += c;
            }

            if (string.length() != 0 && string.back() == '\x00')
                result.pop_back();

            return result;
        }

    }

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

    class PatternData : public PatternCreationLimiter,
                        public Cloneable<PatternData> {
    public:
        PatternData(Evaluator *evaluator, u64 offset, size_t size, u32 color = 0)
            : PatternCreationLimiter(evaluator), m_offset(offset), m_size(size), m_color(color) {

            if (color != 0)
                return;

            this->m_color       = ContentRegistry::PatternLanguage::getNextColor();
            this->m_manualColor = false;
        }

        PatternData(const PatternData &other) = default;

        ~PatternData() override = default;

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

        [[nodiscard]] virtual const PatternData *getPattern(u64 offset) const {
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

        static bool sortPatternDataTable(ImGuiTableSortSpecs *sortSpecs, prv::Provider *provider, pl::PatternData *left, pl::PatternData *right) {
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

        [[nodiscard]] virtual bool operator!=(const PatternData &other) const final { return !operator==(other); }
        [[nodiscard]] virtual bool operator==(const PatternData &other) const = 0;

        template<typename T>
        [[nodiscard]] bool areCommonPropertiesEqual(const PatternData &other) const {
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
            if (ImGui::Selectable(("##PatternDataLine"s + std::to_string(u64(this))).c_str(), false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap)) {
                ImHexApi::HexEditor::setSelection(this->getOffset(), this->getSize());
            }
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

    class PatternDataPadding : public PatternData {
    public:
        PatternDataPadding(Evaluator *evaluator, u64 offset, size_t size) : PatternData(evaluator, offset, size, 0xFF000000) { }

        [[nodiscard]] PatternData *clone() const override {
            return new PatternDataPadding(*this);
        }

        void createEntry(prv::Provider *&provider) override {
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return "";
        }

        [[nodiscard]] bool operator==(const PatternData &other) const override { return areCommonPropertiesEqual<decltype(*this)>(other); }
    };

    class PatternDataPointer : public PatternData,
                               public Inlinable {
    public:
        PatternDataPointer(Evaluator *evaluator, u64 offset, size_t size, u32 color = 0)
            : PatternData(evaluator, offset, size, color), m_pointedAt(nullptr) {
        }

        PatternDataPointer(const PatternDataPointer &other) : PatternData(other) {
            this->m_pointedAt = other.m_pointedAt->clone();
        }

        ~PatternDataPointer() override {
            delete this->m_pointedAt;
        }

        [[nodiscard]] PatternData *clone() const override {
            return new PatternDataPointer(*this);
        }

        void createEntry(prv::Provider *&provider) override {
            u64 data = 0;
            provider->read(this->getOffset(), &data, this->getSize());
            data = hex::changeEndianess(data, this->getSize(), this->getEndian());

            bool open = true;

            if (!this->isInlined()) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                open = ImGui::TreeNodeEx(this->getDisplayName().c_str(), ImGuiTreeNodeFlags_SpanFullWidth);
                ImGui::TableNextColumn();
                if (ImGui::Selectable(("##PatternDataLine"s + std::to_string(u64(this))).c_str(), false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap)) {
                    ImHexApi::HexEditor::setSelection(this->getOffset(), this->getSize());
                }
                this->drawCommentTooltip();
                ImGui::SameLine(0, 0);
                ImGui::ColorButton("color", ImColor(this->getColor()), ImGuiColorEditFlags_NoTooltip, ImVec2(ImGui::GetColumnWidth(), ImGui::GetTextLineHeight()));
                ImGui::TableNextColumn();
                ImGui::TextFormatted("0x{0:08X} : 0x{1:08X}", this->getOffset(), this->getOffset() + this->getSize() - 1);
                ImGui::TableNextColumn();
                ImGui::TextFormatted("0x{0:04X}", this->getSize());
                ImGui::TableNextColumn();
                ImGui::TextFormattedColored(ImColor(0xFF9BC64D), "{}", this->getFormattedName());
                ImGui::TableNextColumn();
                ImGui::TextFormatted("{}", formatDisplayValue(hex::format("*(0x{0:X})", data), u128(data)));
            }

            if (open) {
                this->m_pointedAt->createEntry(provider);

                if (!this->isInlined())
                    ImGui::TreePop();
            }
        }

        void getHighlightedAddresses(std::map<u64, u32> &highlight) const override {
            PatternData::getHighlightedAddresses(highlight);
            this->m_pointedAt->getHighlightedAddresses(highlight);
        }

        [[nodiscard]] std::string getFormattedName() const override {
            std::string result = this->m_pointedAt->getFormattedName() + "* : ";
            switch (this->getSize()) {
                case 1:
                    result += "u8";
                    break;
                case 2:
                    result += "u16";
                    break;
                case 4:
                    result += "u32";
                    break;
                case 8:
                    result += "u64";
                    break;
                case 16:
                    result += "u128";
                    break;
            }

            return result;
        }

        void setPointedAtPattern(PatternData *pattern) {
            this->m_pointedAt = pattern;
            this->m_pointedAt->setVariableName(hex::format("*({})", this->getVariableName()));
            this->m_pointedAt->setOffset(this->m_pointedAtAddress);
        }

        void setPointedAtAddress(u64 address) {
            this->m_pointedAtAddress = address;
        }

        [[nodiscard]] u64 getPointedAtAddress() const {
            return this->m_pointedAtAddress;
        }

        [[nodiscard]] PatternData *getPointedAtPattern() {
            return this->m_pointedAt;
        }

        void setColor(u32 color) override {
            PatternData::setColor(color);
            this->m_pointedAt->setColor(color);
        }

        [[nodiscard]] bool operator==(const PatternData &other) const override {
            return areCommonPropertiesEqual<decltype(*this)>(other) &&
                   *static_cast<const PatternDataPointer *>(&other)->m_pointedAt == *this->m_pointedAt;
        }

        void rebase(u64 base) {
            if (this->m_pointedAt != nullptr) {
                this->m_pointedAtAddress = (this->m_pointedAt->getOffset() - this->m_pointerBase) + base;
                this->m_pointedAt->setOffset(this->m_pointedAtAddress);
            }

            this->m_pointerBase = base;
        }

        [[nodiscard]] const PatternData *getPattern(u64 offset) const override {
            if (offset >= this->getOffset() && offset < (this->getOffset() + this->getSize()) && !this->isHidden())
                return this;
            else
                return this->m_pointedAt->getPattern(offset);
        }

        void setEndian(std::endian endian) override {
            this->m_pointedAt->setEndian(endian);

            PatternData::setEndian(endian);
        }

    private:
        PatternData *m_pointedAt = nullptr;
        u64 m_pointedAtAddress   = 0;

        u64 m_pointerBase = 0;
    };

    class PatternDataUnsigned : public PatternData {
    public:
        PatternDataUnsigned(Evaluator *evaluator, u64 offset, size_t size, u32 color = 0)
            : PatternData(evaluator, offset, size, color) { }

        [[nodiscard]] PatternData *clone() const override {
            return new PatternDataUnsigned(*this);
        }

        void createEntry(prv::Provider *&provider) override {
            u128 data = 0;
            provider->read(this->getOffset(), &data, this->getSize());
            data = hex::changeEndianess(data, this->getSize(), this->getEndian());

            this->createDefaultEntry(hex::format("{:d} (0x{:0{}X})", data, data, this->getSize() * 2), data);
        }

        [[nodiscard]] std::string getFormattedName() const override {
            switch (this->getSize()) {
                case 1:
                    return "u8";
                case 2:
                    return "u16";
                case 4:
                    return "u32";
                case 8:
                    return "u64";
                case 16:
                    return "u128";
                default:
                    return "Unsigned data";
            }
        }

        [[nodiscard]] bool operator==(const PatternData &other) const override { return areCommonPropertiesEqual<decltype(*this)>(other); }
    };

    class PatternDataSigned : public PatternData {
    public:
        PatternDataSigned(Evaluator *evaluator, u64 offset, size_t size, u32 color = 0)
            : PatternData(evaluator, offset, size, color) { }

        [[nodiscard]] PatternData *clone() const override {
            return new PatternDataSigned(*this);
        }

        void createEntry(prv::Provider *&provider) override {
            i128 data = 0;
            provider->read(this->getOffset(), &data, this->getSize());
            data = hex::changeEndianess(data, this->getSize(), this->getEndian());

            data = hex::signExtend(this->getSize() * 8, data);
            this->createDefaultEntry(hex::format("{:d} (0x{:0{}X})", data, data, 1 * 2), data);
        }

        [[nodiscard]] std::string getFormattedName() const override {
            switch (this->getSize()) {
                case 1:
                    return "s8";
                case 2:
                    return "s16";
                case 4:
                    return "s32";
                case 8:
                    return "s64";
                case 16:
                    return "s128";
                default:
                    return "Signed data";
            }
        }

        [[nodiscard]] bool operator==(const PatternData &other) const override { return areCommonPropertiesEqual<decltype(*this)>(other); }
    };

    class PatternDataFloat : public PatternData {
    public:
        PatternDataFloat(Evaluator *evaluator, u64 offset, size_t size, u32 color = 0)
            : PatternData(evaluator, offset, size, color) { }

        [[nodiscard]] PatternData *clone() const override {
            return new PatternDataFloat(*this);
        }

        void createEntry(prv::Provider *&provider) override {
            if (this->getSize() == 4) {
                u32 data = 0;
                provider->read(this->getOffset(), &data, 4);
                data = hex::changeEndianess(data, 4, this->getEndian());

                this->createDefaultEntry(hex::format("{:e} (0x{:0{}X})", *reinterpret_cast<float *>(&data), data, this->getSize() * 2), *reinterpret_cast<float *>(&data));
            } else if (this->getSize() == 8) {
                u64 data = 0;
                provider->read(this->getOffset(), &data, 8);
                data = hex::changeEndianess(data, 8, this->getEndian());

                this->createDefaultEntry(hex::format("{:e} (0x{:0{}X})", *reinterpret_cast<double *>(&data), data, this->getSize() * 2), *reinterpret_cast<double *>(&data));
            }
        }

        [[nodiscard]] std::string getFormattedName() const override {
            switch (this->getSize()) {
                case 4:
                    return "float";
                case 8:
                    return "double";
                default:
                    return "Floating point data";
            }
        }

        [[nodiscard]] bool operator==(const PatternData &other) const override { return areCommonPropertiesEqual<decltype(*this)>(other); }
    };

    class PatternDataBoolean : public PatternData {
    public:
        explicit PatternDataBoolean(Evaluator *evaluator, u64 offset, u32 color = 0)
            : PatternData(evaluator, offset, 1, color) { }

        [[nodiscard]] PatternData *clone() const override {
            return new PatternDataBoolean(*this);
        }

        void createEntry(prv::Provider *&provider) override {
            u8 boolean;
            provider->read(this->getOffset(), &boolean, 1);

            if (boolean == 0)
                this->createDefaultEntry("false", false);
            else if (boolean == 1)
                this->createDefaultEntry("true", true);
            else
                this->createDefaultEntry("true*", true);
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return "bool";
        }

        [[nodiscard]] bool operator==(const PatternData &other) const override { return areCommonPropertiesEqual<decltype(*this)>(other); }
    };

    class PatternDataCharacter : public PatternData {
    public:
        explicit PatternDataCharacter(Evaluator *evaluator, u64 offset, u32 color = 0)
            : PatternData(evaluator, offset, 1, color) { }

        [[nodiscard]] PatternData *clone() const override {
            return new PatternDataCharacter(*this);
        }

        void createEntry(prv::Provider *&provider) override {
            char character;
            provider->read(this->getOffset(), &character, 1);

            this->createDefaultEntry(hex::format("'{0}'", character), character);
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return "char";
        }

        [[nodiscard]] bool operator==(const PatternData &other) const override { return areCommonPropertiesEqual<decltype(*this)>(other); }
    };

    class PatternDataCharacter16 : public PatternData {
    public:
        explicit PatternDataCharacter16(Evaluator *evaluator, u64 offset, u32 color = 0)
            : PatternData(evaluator, offset, 2, color) { }

        [[nodiscard]] PatternData *clone() const override {
            return new PatternDataCharacter16(*this);
        }

        void createEntry(prv::Provider *&provider) override {
            char16_t character;
            provider->read(this->getOffset(), &character, 2);
            character = hex::changeEndianess(character, this->getEndian());

            u128 literal = character;
            this->createDefaultEntry(hex::format("'{0}'", std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> {}.to_bytes(character)), literal);
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return "char16";
        }

        [[nodiscard]] std::string toString(prv::Provider *provider) const override {
            char16_t character;
            provider->read(this->getOffset(), &character, 2);
            character = hex::changeEndianess(character, this->getEndian());

            return std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> {}.to_bytes(character);
        }

        [[nodiscard]] bool operator==(const PatternData &other) const override { return areCommonPropertiesEqual<decltype(*this)>(other); }
    };

    class PatternDataString : public PatternData {
    public:
        PatternDataString(Evaluator *evaluator, u64 offset, size_t size, u32 color = 0)
            : PatternData(evaluator, offset, size, color) { }

        [[nodiscard]] PatternData *clone() const override {
            return new PatternDataString(*this);
        }

        void createEntry(prv::Provider *&provider) override {
            auto size = std::min<size_t>(this->getSize(), 0x7F);

            if (size == 0)
                return;

            std::string buffer(size, 0x00);

            provider->read(this->getOffset(), buffer.data(), size);

            this->createDefaultEntry(hex::format("\"{0}\" {1}", makeDisplayable(buffer), size > this->getSize() ? "(truncated)" : ""), buffer);
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return "String";
        }

        [[nodiscard]] std::string toString(prv::Provider *provider) const override {
            std::string buffer(this->getSize(), 0x00);
            provider->read(this->getOffset(), buffer.data(), buffer.size());

            std::erase_if(buffer, [](auto c) {
                return c == 0x00;
            });

            return buffer;
        }

        [[nodiscard]] bool operator==(const PatternData &other) const override { return areCommonPropertiesEqual<decltype(*this)>(other); }
    };

    class PatternDataString16 : public PatternData {
    public:
        PatternDataString16(Evaluator *evaluator, u64 offset, size_t size, u32 color = 0)
            : PatternData(evaluator, offset, size, color) { }

        [[nodiscard]] PatternData *clone() const override {
            return new PatternDataString16(*this);
        }

        void createEntry(prv::Provider *&provider) override {
            auto size = std::min<size_t>(this->getSize(), 0x100);

            if (size == 0)
                return;

            std::u16string buffer(this->getSize() / sizeof(char16_t), 0x00);
            provider->read(this->getOffset(), buffer.data(), size);

            for (auto &c : buffer)
                c = hex::changeEndianess(c, 2, this->getEndian());

            buffer.erase(std::remove_if(buffer.begin(), buffer.end(), [](auto c) {
                return c == 0x00;
            }),
                buffer.end());

            auto utf8String = std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> {}.to_bytes(buffer);

            this->createDefaultEntry(hex::format("\"{0}\" {1}", utf8String, size > this->getSize() ? "(truncated)" : ""), utf8String);
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return "String16";
        }

        [[nodiscard]] std::string toString(prv::Provider *provider) const override {
            std::u16string buffer(this->getSize() / sizeof(char16_t), 0x00);
            provider->read(this->getOffset(), buffer.data(), this->getSize());

            for (auto &c : buffer)
                c = hex::changeEndianess(c, 2, this->getEndian());

            std::erase_if(buffer, [](auto c) {
                return c == 0x00;
            });

            return std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> {}.to_bytes(buffer);
        }

        [[nodiscard]] bool operator==(const PatternData &other) const override { return areCommonPropertiesEqual<decltype(*this)>(other); }
    };

    class PatternDataDynamicArray : public PatternData,
                                    public Inlinable {
    public:
        PatternDataDynamicArray(Evaluator *evaluator, u64 offset, size_t size, u32 color = 0)
            : PatternData(evaluator, offset, size, color) {
        }

        PatternDataDynamicArray(const PatternDataDynamicArray &other) : PatternData(other) {
            std::vector<PatternData *> entries;
            for (const auto &entry : other.m_entries)
                entries.push_back(entry->clone());

            this->setEntries(entries);
        }

        ~PatternDataDynamicArray() override {
            for (const auto &entry : this->m_entries)
                delete entry;
        }

        [[nodiscard]] PatternData *clone() const override {
            return new PatternDataDynamicArray(*this);
        }

        void setColor(u32 color) override {
            PatternData::setColor(color);
            for (auto &entry : this->m_entries)
                entry->setColor(color);
        }

        void createEntry(prv::Provider *&provider) override {
            if (this->m_entries.empty())
                return;

            bool open = true;
            if (!this->isInlined()) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                open = ImGui::TreeNodeEx(this->getDisplayName().c_str(), ImGuiTreeNodeFlags_SpanFullWidth);
                ImGui::TableNextColumn();
                if (ImGui::Selectable(("##PatternDataLine"s + std::to_string(u64(this))).c_str(), false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap)) {
                    ImHexApi::HexEditor::setSelection(this->getOffset(), this->getSize());
                }
                this->drawCommentTooltip();
                ImGui::TableNextColumn();
                ImGui::TextFormatted("0x{0:08X} : 0x{1:08X}", this->getOffset(), this->getOffset() + this->getSize() - 1);
                ImGui::TableNextColumn();
                ImGui::TextFormatted("0x{0:04X}", this->getSize());
                ImGui::TableNextColumn();
                ImGui::TextFormattedColored(ImColor(0xFF9BC64D), "{0}", this->m_entries[0]->getTypeName());
                ImGui::SameLine(0, 0);

                ImGui::TextUnformatted("[");
                ImGui::SameLine(0, 0);
                ImGui::TextFormattedColored(ImColor(0xFF00FF00), "{0}", this->m_entries.size());
                ImGui::SameLine(0, 0);
                ImGui::TextUnformatted("]");

                ImGui::TableNextColumn();
                ImGui::TextFormatted("{}", this->formatDisplayValue("{ ... }", this));
            }

            if (open) {
                for (u64 i = 0; i < this->m_entries.size(); i++) {
                    this->m_entries[i]->draw(provider);

                    if (i >= (this->m_displayEnd - 1)) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();

                        ImGui::Selectable("... (Double-click to see more items)", false, ImGuiSelectableFlags_SpanAllColumns);
                        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                            this->m_displayEnd += 50;

                        break;
                    }
                }

                if (!this->isInlined())
                    ImGui::TreePop();
            } else {
                this->m_displayEnd = 50;
            }
        }

        void getHighlightedAddresses(std::map<u64, u32> &highlight) const override {
            for (auto &entry : this->m_entries) {
                entry->getHighlightedAddresses(highlight);
            }
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return this->m_entries[0]->getTypeName() + "[" + std::to_string(this->m_entries.size()) + "]";
        }

        void setOffset(u64 offset) override {
            for (auto &entry : this->m_entries)
                entry->setOffset(entry->getOffset() - this->getOffset() + offset);

            PatternData::setOffset(offset);
        }

        [[nodiscard]] const std::vector<PatternData *> &getEntries() {
            return this->m_entries;
        }

        void setEntries(const std::vector<PatternData *> &entries) {
            this->m_entries = entries;

            if (this->hasOverriddenColor()) {
                for (auto &entry : this->m_entries) {
                    entry->setColor(this->getColor());
                }
            }
        }

        [[nodiscard]] bool operator==(const PatternData &other) const override {
            if (!areCommonPropertiesEqual<decltype(*this)>(other))
                return false;

            auto &otherArray = *static_cast<const PatternDataDynamicArray *>(&other);
            if (this->m_entries.size() != otherArray.m_entries.size())
                return false;

            for (u64 i = 0; i < this->m_entries.size(); i++) {
                if (*this->m_entries[i] != *otherArray.m_entries[i])
                    return false;
            }

            return true;
        }

        [[nodiscard]] const PatternData *getPattern(u64 offset) const override {
            if (this->isHidden()) return nullptr;

            for (auto pattern : this->m_entries) {
                auto result = pattern->getPattern(offset);
                if (result != nullptr)
                    return result;
            }

            return nullptr;
        }

        void setEndian(std::endian endian) override {
            for (auto &entry : this->m_entries) {
                entry->setEndian(endian);
            }

            PatternData::setEndian(endian);
        }

    private:
        std::vector<PatternData *> m_entries;
        u64 m_displayEnd = 50;
    };

    class PatternDataStaticArray : public PatternData,
                                   public Inlinable {
    public:
        PatternDataStaticArray(Evaluator *evaluator, u64 offset, size_t size, u32 color = 0)
            : PatternData(evaluator, offset, size, color) {
        }

        PatternDataStaticArray(const PatternDataStaticArray &other) : PatternData(other) {
            this->setEntries(other.getTemplate()->clone(), other.getEntryCount());
        }

        ~PatternDataStaticArray() override {
            delete this->m_template;
            delete this->m_highlightTemplate;
        }

        [[nodiscard]] PatternData *clone() const override {
            return new PatternDataStaticArray(*this);
        }

        void createEntry(prv::Provider *&provider) override {
            if (this->getEntryCount() == 0)
                return;

            bool open = true;

            if (!this->isInlined()) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                open = ImGui::TreeNodeEx(this->getDisplayName().c_str(), ImGuiTreeNodeFlags_SpanFullWidth);
                ImGui::TableNextColumn();
                if (ImGui::Selectable(("##PatternDataLine"s + std::to_string(u64(this))).c_str(), false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap)) {
                    ImHexApi::HexEditor::setSelection(this->getOffset(), this->getSize());
                }
                this->drawCommentTooltip();
                ImGui::TableNextColumn();
                ImGui::TextFormatted("0x{0:08X} : 0x{1:08X}", this->getOffset(), this->getOffset() + this->getSize() - 1);
                ImGui::TableNextColumn();
                ImGui::TextFormatted("0x{0:04X}", this->getSize());
                ImGui::TableNextColumn();
                ImGui::TextFormattedColored(ImColor(0xFF9BC64D), "{0}", this->m_template->getTypeName().c_str());
                ImGui::SameLine(0, 0);

                ImGui::TextUnformatted("[");
                ImGui::SameLine(0, 0);
                ImGui::TextFormattedColored(ImColor(0xFF00FF00), "{0}", this->m_entryCount);
                ImGui::SameLine(0, 0);
                ImGui::TextUnformatted("]");

                ImGui::TableNextColumn();
                ImGui::TextFormatted("{}", this->formatDisplayValue("{ ... }", this));
            }

            if (open) {
                auto entry = this->m_template->clone();
                for (u64 index = 0; index < this->m_entryCount; index++) {
                    entry->setVariableName(hex::format("[{0}]", index));
                    entry->setOffset(this->getOffset() + index * this->m_template->getSize());
                    entry->draw(provider);

                    if (index >= (this->m_displayEnd - 1)) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();

                        ImGui::Selectable("... (Double-click to see more items)", false, ImGuiSelectableFlags_SpanAllColumns);
                        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                            this->m_displayEnd += 50;

                        break;
                    }
                }
                delete entry;

                if (!this->isInlined())
                    ImGui::TreePop();
            } else {
                this->m_displayEnd = 50;
            }
        }

        void getHighlightedAddresses(std::map<u64, u32> &highlight) const override {
            auto entry = this->m_template->clone();

            for (u64 address = this->getOffset(); address < this->getOffset() + this->getSize(); address += entry->getSize()) {
                entry->setOffset(address);
                entry->getHighlightedAddresses(highlight);
            }

            delete entry;
        }

        void setOffset(u64 offset) override {
            this->m_template->setOffset(this->m_template->getOffset() - this->getOffset() + offset);

            PatternData::setOffset(offset);
        }

        void setColor(u32 color) override {
            PatternData::setColor(color);
            this->m_template->setColor(color);
            this->m_highlightTemplate->setColor(color);
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return this->m_template->getTypeName() + "[" + std::to_string(this->m_entryCount) + "]";
        }

        [[nodiscard]] PatternData *getTemplate() const {
            return this->m_template;
        }

        [[nodiscard]] size_t getEntryCount() const {
            return this->m_entryCount;
        }

        void setEntryCount(size_t count) {
            this->m_entryCount = count;
        }

        void setEntries(PatternData *templatePattern, size_t count) {
            this->m_template          = templatePattern;
            this->m_highlightTemplate = this->m_template->clone();
            this->m_entryCount        = count;

            if (this->hasOverriddenColor()) this->setColor(this->m_template->getColor());
            this->m_template->setEndian(templatePattern->getEndian());
        }

        [[nodiscard]] bool operator==(const PatternData &other) const override {
            if (!areCommonPropertiesEqual<decltype(*this)>(other))
                return false;

            auto &otherArray = *static_cast<const PatternDataStaticArray *>(&other);
            return *this->m_template == *otherArray.m_template && this->m_entryCount == otherArray.m_entryCount;
        }

        [[nodiscard]] const PatternData *getPattern(u64 offset) const override {
            if (this->isHidden()) return nullptr;

            if (offset >= this->getOffset() && offset < (this->getOffset() + this->getSize())) {
                this->m_highlightTemplate->setOffset((offset / this->m_highlightTemplate->getSize()) * this->m_highlightTemplate->getSize());
                return this->m_highlightTemplate->getPattern(offset);
            } else {
                return nullptr;
            }
        }

        void setEndian(std::endian endian) override {
            this->m_template->setEndian(endian);

            PatternData::setEndian(endian);
        }

    private:
        PatternData *m_template                  = nullptr;
        mutable PatternData *m_highlightTemplate = nullptr;
        size_t m_entryCount                      = 0;
        u64 m_displayEnd                         = 50;
    };

    class PatternDataStruct : public PatternData,
                              public Inlinable {
    public:
        PatternDataStruct(Evaluator *evaluator, u64 offset, size_t size, u32 color = 0)
            : PatternData(evaluator, offset, size, color) {
        }

        PatternDataStruct(const PatternDataStruct &other) : PatternData(other) {
            for (const auto &member : other.m_members)
                this->m_members.push_back(member->clone());
            this->m_sortedMembers = this->m_members;
        }

        ~PatternDataStruct() override {
            for (const auto &member : this->m_members)
                delete member;
        }

        [[nodiscard]] PatternData *clone() const override {
            return new PatternDataStruct(*this);
        }

        void createEntry(prv::Provider *&provider) override {
            bool open = true;

            if (!this->isInlined()) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                open = ImGui::TreeNodeEx(this->getDisplayName().c_str(), ImGuiTreeNodeFlags_SpanFullWidth);
                ImGui::TableNextColumn();
                if (ImGui::Selectable(("##PatternDataLine"s + std::to_string(u64(this))).c_str(), false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap)) {
                    ImHexApi::HexEditor::setSelection(this->getOffset(), this->getSize());
                }
                this->drawCommentTooltip();
                ImGui::TableNextColumn();
                ImGui::TextFormatted("0x{0:08X} : 0x{1:08X}", this->getOffset(), this->getOffset() + this->getSize() - (this->getSize() == 0 ? 0 : 1));
                ImGui::TableNextColumn();
                ImGui::TextFormatted("0x{0:04X}", this->getSize());
                ImGui::TableNextColumn();
                ImGui::TextFormattedColored(ImColor(0xFFD69C56), "struct");
                ImGui::SameLine();
                ImGui::TextUnformatted(this->getTypeName().c_str());
                ImGui::TableNextColumn();
                ImGui::TextFormatted("{}", this->formatDisplayValue("{ ... }", this));
            }

            if (open) {
                for (auto &member : this->m_sortedMembers)
                    member->draw(provider);

                if (!this->isInlined())
                    ImGui::TreePop();
            }
        }

        void getHighlightedAddresses(std::map<u64, u32> &highlight) const override {
            for (auto &member : this->m_members) {
                member->getHighlightedAddresses(highlight);
            }
        }

        void setOffset(u64 offset) override {
            for (auto &member : this->m_members)
                member->setOffset(member->getOffset() - this->getOffset() + offset);

            PatternData::setOffset(offset);
        }

        void setColor(u32 color) override {
            PatternData::setColor(color);
            for (auto &member : this->m_members) {
                if (!member->hasOverriddenColor())
                    member->setColor(color);
            }
        }

        void sort(ImGuiTableSortSpecs *sortSpecs, prv::Provider *provider) override {
            this->m_sortedMembers = this->m_members;

            std::sort(this->m_sortedMembers.begin(), this->m_sortedMembers.end(), [&sortSpecs, &provider](PatternData *left, PatternData *right) {
                return PatternData::sortPatternDataTable(sortSpecs, provider, left, right);
            });

            for (auto &member : this->m_members)
                member->sort(sortSpecs, provider);
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return "struct " + PatternData::getTypeName();
        }

        [[nodiscard]] const auto &getMembers() const {
            return this->m_members;
        }

        void setMembers(const std::vector<PatternData *> &members) {
            this->m_members.clear();

            for (auto &member : members) {
                if (member == nullptr) continue;

                this->m_members.push_back(member);
            }

            this->m_sortedMembers = this->m_members;
        }

        [[nodiscard]] bool operator==(const PatternData &other) const override {
            if (!areCommonPropertiesEqual<decltype(*this)>(other))
                return false;

            auto &otherStruct = *static_cast<const PatternDataStruct *>(&other);
            if (this->m_members.size() != otherStruct.m_members.size())
                return false;

            for (u64 i = 0; i < this->m_members.size(); i++) {
                if (*this->m_members[i] != *otherStruct.m_members[i])
                    return false;
            }

            return true;
        }

        [[nodiscard]] const PatternData *getPattern(u64 offset) const override {
            if (this->isHidden()) return nullptr;

            auto iter = std::find_if(this->m_members.begin(), this->m_members.end(), [offset](PatternData *pattern) {
                return offset >= pattern->getOffset() && offset < (pattern->getOffset() + pattern->getSize());
            });

            if (iter == this->m_members.end())
                return nullptr;
            else
                return (*iter)->getPattern(offset);
        }

        void setEndian(std::endian endian) override {
            for (auto &member : this->m_members) {
                if (!member->hasOverriddenEndian())
                    member->setEndian(endian);
            }

            PatternData::setEndian(endian);
        }

    private:
        std::vector<PatternData *> m_members;
        std::vector<PatternData *> m_sortedMembers;
    };

    class PatternDataUnion : public PatternData,
                             public Inlinable {
    public:
        PatternDataUnion(Evaluator *evaluator, u64 offset, size_t size, u32 color = 0)
            : PatternData(evaluator, offset, size, color) {
        }

        PatternDataUnion(const PatternDataUnion &other) : PatternData(other) {
            for (const auto &member : other.m_members)
                this->m_members.push_back(member->clone());
            this->m_sortedMembers = this->m_members;
        }

        ~PatternDataUnion() override {
            for (const auto &member : this->m_members)
                delete member;
        }

        [[nodiscard]] PatternData *clone() const override {
            return new PatternDataUnion(*this);
        }

        void createEntry(prv::Provider *&provider) override {
            bool open = true;

            if (!this->isInlined()) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                open = ImGui::TreeNodeEx(this->getDisplayName().c_str(), ImGuiTreeNodeFlags_SpanFullWidth);
                ImGui::TableNextColumn();
                if (ImGui::Selectable(("##PatternDataLine"s + std::to_string(u64(this))).c_str(), false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap)) {
                    ImHexApi::HexEditor::setSelection(this->getOffset(), this->getSize());
                }
                this->drawCommentTooltip();
                ImGui::TableNextColumn();
                ImGui::TextFormatted("0x{0:08X} : 0x{1:08X}", this->getOffset(), std::max(this->getOffset() + this->getSize() - (this->getSize() == 0 ? 0 : 1), u64(0)));
                ImGui::TableNextColumn();
                ImGui::TextFormatted("0x{0:04X}", this->getSize());
                ImGui::TableNextColumn();
                ImGui::TextFormattedColored(ImColor(0xFFD69C56), "union");
                ImGui::SameLine();
                ImGui::TextUnformatted(PatternData::getTypeName().c_str());

                ImGui::TableNextColumn();
                ImGui::TextFormatted("{}", this->formatDisplayValue("{ ... }", this));
            }

            if (open) {
                for (auto &member : this->m_sortedMembers)
                    member->draw(provider);

                if (!this->isInlined())
                    ImGui::TreePop();
            }
        }

        void getHighlightedAddresses(std::map<u64, u32> &highlight) const override {
            for (auto &member : this->m_members) {
                member->getHighlightedAddresses(highlight);
            }
        }

        void setOffset(u64 offset) override {
            for (auto &member : this->m_members)
                member->setOffset(member->getOffset() - this->getOffset() + offset);

            PatternData::setOffset(offset);
        }

        void setColor(u32 color) override {
            PatternData::setColor(color);
            for (auto &member : this->m_members) {
                if (!member->hasOverriddenColor())
                    member->setColor(color);
            }
        }

        void sort(ImGuiTableSortSpecs *sortSpecs, prv::Provider *provider) override {
            this->m_sortedMembers = this->m_members;

            std::sort(this->m_sortedMembers.begin(), this->m_sortedMembers.end(), [&sortSpecs, &provider](PatternData *left, PatternData *right) {
                return PatternData::sortPatternDataTable(sortSpecs, provider, left, right);
            });

            for (auto &member : this->m_members)
                member->sort(sortSpecs, provider);
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return "union " + PatternData::getTypeName();
            ;
        }

        [[nodiscard]] const auto &getMembers() const {
            return this->m_members;
        }

        void setMembers(const std::vector<PatternData *> &members) {
            this->m_members.clear();
            for (auto &member : members) {
                if (member == nullptr) continue;

                this->m_members.push_back(member);
            }

            this->m_sortedMembers = this->m_members;
        }

        [[nodiscard]] bool operator==(const PatternData &other) const override {
            if (!areCommonPropertiesEqual<decltype(*this)>(other))
                return false;

            auto &otherUnion = *static_cast<const PatternDataUnion *>(&other);
            if (this->m_members.size() != otherUnion.m_members.size())
                return false;

            for (u64 i = 0; i < this->m_members.size(); i++) {
                if (*this->m_members[i] != *otherUnion.m_members[i])
                    return false;
            }

            return true;
        }

        [[nodiscard]] const PatternData *getPattern(u64 offset) const override {
            if (this->isHidden()) return nullptr;

            auto largestMember = std::find_if(this->m_members.begin(), this->m_members.end(), [this](PatternData *pattern) {
                return pattern->getSize() == this->getSize();
            });

            if (largestMember == this->m_members.end())
                return nullptr;
            else
                return (*largestMember)->getPattern(offset);
            ;
        }

        void setEndian(std::endian endian) override {
            for (auto &member : this->m_members) {
                if (!member->hasOverriddenEndian())
                    member->setEndian(endian);
            }

            PatternData::setEndian(endian);
        }

    private:
        std::vector<PatternData *> m_members;
        std::vector<PatternData *> m_sortedMembers;
    };

    class PatternDataEnum : public PatternData {
    public:
        PatternDataEnum(Evaluator *evaluator, u64 offset, size_t size, u32 color = 0)
            : PatternData(evaluator, offset, size, color) {
        }

        [[nodiscard]] PatternData *clone() const override {
            return new PatternDataEnum(*this);
        }

        void createEntry(prv::Provider *&provider) override {
            u64 value = 0;
            provider->read(this->getOffset(), &value, this->getSize());
            value = hex::changeEndianess(value, this->getSize(), this->getEndian());

            std::string valueString = PatternData::getTypeName() + "::";

            bool foundValue = false;
            for (auto &[entryValueLiteral, entryName] : this->m_enumValues) {
                bool matches = std::visit(overloaded {
                                              [&, name = entryName](auto &&entryValue) {
                                                  if (value == entryValue) {
                                                      valueString += name;
                                                      foundValue = true;
                                                      return true;
                                                  }

                                                  return false;
                                              },
                                              [](std::string &) { return false; },
                                              [](PatternData *) { return false; } },
                    entryValueLiteral);
                if (matches)
                    break;
            }

            if (!foundValue)
                valueString += "???";

            ImGui::TableNextRow();
            ImGui::TreeNodeEx(this->getDisplayName().c_str(), ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_AllowItemOverlap);
            this->drawCommentTooltip();
            ImGui::TableNextColumn();
            if (ImGui::Selectable(("##PatternDataLine"s + std::to_string(u64(this))).c_str(), false, ImGuiSelectableFlags_SpanAllColumns)) {
                ImHexApi::HexEditor::setSelection(this->getOffset(), this->getSize());
            }
            ImGui::SameLine();
            ImGui::TextUnformatted(this->getDisplayName().c_str());
            ImGui::TableNextColumn();
            ImGui::ColorButton("color", ImColor(this->getColor()), ImGuiColorEditFlags_NoTooltip, ImVec2(ImGui::GetColumnWidth(), ImGui::GetTextLineHeight()));
            ImGui::TableNextColumn();
            ImGui::TextFormatted("0x{0:08X} : 0x{1:08X}", this->getOffset(), this->getOffset() + this->getSize() - 1);
            ImGui::TableNextColumn();
            ImGui::TextFormatted("0x{0:04X}", this->getSize());
            ImGui::TableNextColumn();
            ImGui::TextFormattedColored(ImColor(0xFFD69C56), "enum");
            ImGui::SameLine();
            ImGui::TextUnformatted(PatternData::getTypeName().c_str());
            ImGui::TableNextColumn();
            ImGui::TextFormatted("{}", this->formatDisplayValue(hex::format("{} (0x{:0{}X})", valueString.c_str(), value, this->getSize() * 2), this));
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return "enum " + PatternData::getTypeName();
        }

        [[nodiscard]] const auto &getEnumValues() const {
            return this->m_enumValues;
        }

        void setEnumValues(const std::vector<std::pair<Token::Literal, std::string>> &enumValues) {
            this->m_enumValues = enumValues;
        }

        [[nodiscard]] bool operator==(const PatternData &other) const override {
            if (!areCommonPropertiesEqual<decltype(*this)>(other))
                return false;

            auto &otherEnum = *static_cast<const PatternDataEnum *>(&other);
            if (this->m_enumValues.size() != otherEnum.m_enumValues.size())
                return false;

            for (u64 i = 0; i < this->m_enumValues.size(); i++) {
                if (this->m_enumValues[i] != otherEnum.m_enumValues[i])
                    return false;
            }

            return true;
        }

    private:
        std::vector<std::pair<Token::Literal, std::string>> m_enumValues;
    };


    class PatternDataBitfieldField : public PatternData {
    public:
        PatternDataBitfieldField(Evaluator *evaluator, u64 offset, u8 bitOffset, u8 bitSize, PatternData *bitField, u32 color = 0)
            : PatternData(evaluator, offset, 0, color), m_bitOffset(bitOffset), m_bitSize(bitSize), m_bitField(bitField) {
        }

        [[nodiscard]] PatternData *clone() const override {
            return new PatternDataBitfieldField(*this);
        }

        void createEntry(prv::Provider *&provider) override {
            std::vector<u8> value(this->m_bitField->getSize(), 0);
            provider->read(this->m_bitField->getOffset(), &value[0], value.size());

            if (this->m_bitField->getEndian() != std::endian::native)
                std::reverse(value.begin(), value.end());

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(this->getDisplayName().c_str());
            ImGui::TableNextColumn();
            ImGui::ColorButton("color", ImColor(this->getColor()), ImGuiColorEditFlags_NoTooltip, ImVec2(ImGui::GetColumnWidth(), ImGui::GetTextLineHeight()));
            ImGui::TableNextColumn();
            if (this->m_bitSize == 1)
                ImGui::TextFormatted("0x{0:08X} bit {1}", this->getOffset() + this->m_bitOffset / 8, (this->m_bitOffset + (this->m_bitSize - 1)) % 8);
            else
                ImGui::TextFormatted("0x{0:08X} bits {1} - {2}", this->getOffset() + this->m_bitOffset / 8, this->m_bitOffset % 8, this->m_bitOffset % 8 + (this->m_bitSize - 1) % 8);
            ImGui::TableNextColumn();
            if (this->m_bitSize == 1)
                ImGui::TextFormatted("{0} bit", this->m_bitSize);
            else
                ImGui::TextFormatted("{0} bits", this->m_bitSize);
            ImGui::TableNextColumn();
            ImGui::TextFormattedColored(ImColor(0xFF9BC64D), "bits");
            ImGui::TableNextColumn();
            {
                u8 numBytes = (this->m_bitSize / 8) + 1;

                u64 extractedValue = hex::extract(this->m_bitOffset + (this->m_bitSize - 1), this->m_bitOffset, value);
                ImGui::TextFormatted("{}", this->formatDisplayValue(hex::format("{0} (0x{1:X})", extractedValue, extractedValue), this));
            }
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return "bits";
        }

        [[nodiscard]] u8 getBitOffset() const {
            return this->m_bitOffset;
        }

        [[nodiscard]] u8 getBitSize() const {
            return this->m_bitSize;
        }

        [[nodiscard]] bool operator==(const PatternData &other) const override {
            if (!areCommonPropertiesEqual<decltype(*this)>(other))
                return false;

            auto &otherBitfieldField = *static_cast<const PatternDataBitfieldField *>(&other);
            return this->m_bitOffset == otherBitfieldField.m_bitOffset && this->m_bitSize == otherBitfieldField.m_bitSize;
        }

    private:
        u8 m_bitOffset, m_bitSize;
        PatternData *m_bitField;
    };

    class PatternDataBitfield : public PatternData,
                                public Inlinable {
    public:
        PatternDataBitfield(Evaluator *evaluator, u64 offset, size_t size, u32 color = 0)
            : PatternData(evaluator, offset, size, color) {
        }

        PatternDataBitfield(const PatternDataBitfield &other) : PatternData(other) {
            for (auto &field : other.m_fields)
                this->m_fields.push_back(field->clone());
        }

        ~PatternDataBitfield() override {
            for (auto field : this->m_fields)
                delete field;
        }

        [[nodiscard]] PatternData *clone() const override {
            return new PatternDataBitfield(*this);
        }

        void createEntry(prv::Provider *&provider) override {
            std::vector<u8> value(this->getSize(), 0);
            provider->read(this->getOffset(), &value[0], value.size());

            if (this->m_endian == std::endian::little)
                std::reverse(value.begin(), value.end());

            bool open = true;
            if (!this->isInlined()) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                open = ImGui::TreeNodeEx(this->getDisplayName().c_str(), ImGuiTreeNodeFlags_SpanFullWidth);
                ImGui::TableNextColumn();
                if (ImGui::Selectable(("##PatternDataLine"s + std::to_string(u64(this))).c_str(), false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap)) {
                    ImHexApi::HexEditor::setSelection(this->getOffset(), this->getSize());
                }
                this->drawCommentTooltip();
                ImGui::TableNextColumn();
                ImGui::TextFormatted("0x{0:08X} : 0x{1:08X}", this->getOffset(), this->getOffset() + this->getSize() - 1);
                ImGui::TableNextColumn();
                ImGui::TextFormatted("0x{0:04X}", this->getSize());
                ImGui::TableNextColumn();
                ImGui::TextFormattedColored(ImColor(0xFFD69C56), "bitfield");
                ImGui::SameLine();
                ImGui::TextUnformatted(PatternData::getTypeName().c_str());
                ImGui::TableNextColumn();

                std::string valueString = "{ ";
                for (auto i : value)
                    valueString += hex::format("{0:02X} ", i);
                valueString += "}";

                ImGui::TextFormatted("{}", this->formatDisplayValue(valueString, this));
            }

            if (open) {

                for (auto &field : this->m_fields)
                    field->draw(provider);

                if (!this->isInlined())
                    ImGui::TreePop();
            }
        }

        void setOffset(u64 offset) override {
            for (auto &field : this->m_fields)
                field->setOffset(field->getOffset() - this->getOffset() + offset);

            PatternData::setOffset(offset);
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return "bitfield " + PatternData::getTypeName();
        }

        [[nodiscard]] const auto &getFields() const {
            return this->m_fields;
        }

        void setFields(const std::vector<PatternData *> &fields) {
            this->m_fields = fields;

            for (auto &field : this->m_fields) {
                field->setSize(this->getSize());
                field->setColor(this->getColor());
            }
        }

        void setColor(u32 color) override {
            PatternData::setColor(color);
            for (auto &field : this->m_fields)
                field->setColor(color);
        }

        [[nodiscard]] bool operator==(const PatternData &other) const override {
            if (!areCommonPropertiesEqual<decltype(*this)>(other))
                return false;

            auto &otherBitfield = *static_cast<const PatternDataBitfield *>(&other);
            if (this->m_fields.size() != otherBitfield.m_fields.size())
                return false;

            for (u64 i = 0; i < this->m_fields.size(); i++) {
                if (*this->m_fields[i] != *otherBitfield.m_fields[i])
                    return false;
            }

            return true;
        }

    private:
        std::vector<PatternData *> m_fields;
    };

}
