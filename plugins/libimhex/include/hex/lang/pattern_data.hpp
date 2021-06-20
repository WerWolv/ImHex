#pragma once

#include <hex.hpp>

#include <imgui.h>

#include <hex/providers/provider.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/lang/token.hpp>
#include <hex/views/view.hpp>

#include <cstring>
#include <codecvt>
#include <locale>
#include <random>
#include <string>

namespace hex::lang {

    using namespace ::std::literals::string_literals;

    namespace {

        std::string makeDisplayable(u8 *data, size_t size) {
            std::string result;
            for (u8* c = data; c < (data + size); c++) {
                if (iscntrl(*c) || *c > 0x7F)
                    result += " ";
                else
                    result += static_cast<char>(*c);
            }

            if (*(data + size - 1) == '\x00')
                result.pop_back();

            return result;
        }

    }

    class PatternData {
    public:
        PatternData(u64 offset, size_t size, u32 color = 0)
        : m_offset(offset), m_size(size), m_color(color), m_parent(nullptr) {
            constexpr u32 Palette[] = { 0x70b4771f, 0x700e7fff, 0x702ca02c, 0x702827d6, 0x70bd6794, 0x704b568c, 0x70c277e3, 0x707f7f7f, 0x7022bdbc, 0x70cfbe17 };

            if (color != 0)
                return;

            this->m_color = Palette[SharedData::patternPaletteOffset++];

            if (SharedData::patternPaletteOffset >= (sizeof(Palette) / sizeof(u32)))
                SharedData::patternPaletteOffset = 0;
        }
        virtual ~PatternData() = default;

        virtual PatternData* clone() = 0;

        [[nodiscard]] u64 getOffset() const { return this->m_offset; }
        void setOffset(u64 offset) { this->m_offset = offset; }

        [[nodiscard]] size_t getSize() const { return this->m_size; }
        void setSize(size_t size) { this->m_size = size; }

        [[nodiscard]] const std::string& getVariableName() const { return this->m_variableName; }
        void setVariableName(std::string name) { this->m_variableName = std::move(name); }

        [[nodiscard]] const std::optional<std::string>& getComment() const { return this->m_comment; }
        void setComment(std::string comment) { this->m_comment = std::move(comment); }

        [[nodiscard]] const std::string& getTypeName() const { return this->m_typeName; }
        void setTypeName(std::string name) { this->m_typeName = std::move(name); }

        [[nodiscard]] u32 getColor() const { return this->m_color; }
        void setColor(u32 color) { this->m_color = color; }

        [[nodiscard]] std::endian getEndian() const { return this->m_endian; }
        void setEndian(std::endian endian) { this->m_endian = endian; }

        [[nodiscard]] PatternData* getParent() const { return this->m_parent; }
        void setParent(PatternData *parent) { this->m_parent = parent; }

        virtual void createEntry(prv::Provider* &provider) = 0;
        [[nodiscard]] virtual std::string getFormattedName() const = 0;

        virtual std::optional<u32> highlightBytes(size_t offset) {
            auto currOffset = this->getOffset();
            if (offset >= currOffset && offset < (currOffset + this->getSize()))
                return this->getColor();
            else
                return { };
        }

        virtual std::map<u64, u32> getHighlightedAddresses() {
            if (this->isHidden()) return { };
            if (this->m_highlightedAddresses.empty()) {
                for (u64 i = 0; i < this->getSize(); i++)
                    this->m_highlightedAddresses.insert({ this->getOffset() + i, this->getColor() });
            }

            return this->m_highlightedAddresses;
        }

        virtual void sort(ImGuiTableSortSpecs *sortSpecs, prv::Provider *provider) { }

        static bool sortPatternDataTable(ImGuiTableSortSpecs *sortSpecs, prv::Provider *provider, lang::PatternData* left, lang::PatternData* right) {
            if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("name")) {
                if (sortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending)
                    return left->getVariableName() > right->getVariableName();
                else
                    return left->getVariableName() < right->getVariableName();
            }
            else if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("offset")) {
                if (sortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending)
                    return left->getOffset() > right->getOffset();
                else
                    return left->getOffset() < right->getOffset();
            }
            else if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("size")) {
                if (sortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending)
                    return left->getSize() > right->getSize();
                else
                    return left->getSize() < right->getSize();
            }
            else if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("value")) {
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
            }
            else if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("type")) {
                if (sortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending)
                    return left->getTypeName() > right->getTypeName();
                else
                    return left->getTypeName() < right->getTypeName();
            }
            else if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("color")) {
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

        static void resetPalette() { SharedData::patternPaletteOffset = 0; }

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

    protected:
        void createDefaultEntry(std::string_view value) const {
            ImGui::TableNextRow();
            ImGui::TreeNodeEx(this->getVariableName().c_str(), ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_AllowItemOverlap);
            ImGui::TableNextColumn();
            if (ImGui::Selectable(("##PatternDataLine"s + std::to_string(this->getOffset())).c_str(), false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap)) {
                EventManager::post<RequestSelectionChange>(Region { this->getOffset(), this->getSize() });
            }
            this->drawCommentTooltip();
            ImGui::SameLine();
            ImGui::Text("%s", this->getVariableName().c_str());
            ImGui::TableNextColumn();
            ImGui::ColorButton("color", ImColor(this->getColor()), ImGuiColorEditFlags_NoTooltip, ImVec2(ImGui::GetColumnWidth(), ImGui::GetTextLineHeight()));
            ImGui::TableNextColumn();
            ImGui::Text("0x%08llX : 0x%08llX", this->getOffset(), this->getOffset() + this->getSize() - 1);
            ImGui::TableNextColumn();
            ImGui::Text("0x%04llX", this->getSize());
            ImGui::TableNextColumn();
            ImGui::TextColored(ImColor(0xFF9BC64D), "%s", this->getFormattedName().c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", value.data());
        }

        void drawCommentTooltip() const {
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) && this->getComment().has_value()) {
                ImGui::BeginTooltip();
                ImGui::TextUnformatted(this->getComment()->c_str());
                ImGui::EndTooltip();
            }
        }

    protected:
        std::endian m_endian = std::endian::native;
        std::map<u64, u32> m_highlightedAddresses;
        bool m_hidden = false;

    private:
        u64 m_offset;
        size_t m_size;

        u32 m_color;
        std::string m_variableName;
        std::optional<std::string> m_comment;
        std::string m_typeName;

        PatternData *m_parent;
        bool m_local = false;
    };

    class PatternDataPadding : public PatternData {
    public:
        PatternDataPadding(u64 offset, size_t size) : PatternData(offset, size, 0xFF000000) { }

        PatternData* clone() override {
            return new PatternDataPadding(*this);
        }

        void createEntry(prv::Provider* &provider) override {
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return "";
        }
    };

    class PatternDataPointer : public PatternData {
    public:
        PatternDataPointer(u64 offset, size_t size, u32 color = 0)
        : PatternData(offset, size, color), m_pointedAt(nullptr) {
        }

        PatternDataPointer(const PatternDataPointer &other) : PatternData(other.getOffset(), other.getSize(), other.getColor()) {
            this->m_pointedAt = other.m_pointedAt->clone();
        }

        ~PatternDataPointer() override {
            delete this->m_pointedAt;
        }

        PatternData* clone() override {
            return new PatternDataPointer(*this);
        }

        void createEntry(prv::Provider* &provider) override {
            u64 data = 0;
            provider->read(this->getOffset(), &data, this->getSize());
            data = hex::changeEndianess(data, this->getSize(), this->getEndian());

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            bool open = ImGui::TreeNodeEx(this->getVariableName().c_str(), ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_AllowItemOverlap);
            this->drawCommentTooltip();
            ImGui::TableNextColumn();
            ImGui::ColorButton("color", ImColor(this->getColor()), ImGuiColorEditFlags_NoTooltip, ImVec2(ImGui::GetColumnWidth(), ImGui::GetTextLineHeight()));
            ImGui::TableNextColumn();
            ImGui::Text("0x%08llX : 0x%08llX", this->getOffset(), this->getOffset() + this->getSize() - 1);
            ImGui::TableNextColumn();
            ImGui::Text("0x%04llX", this->getSize());
            ImGui::TableNextColumn();
            ImGui::TextColored(ImColor(0xFF9BC64D), "%s", this->getFormattedName().c_str());
            ImGui::TableNextColumn();
            ImGui::Text("*(0x%llX)", data);

            if (open) {
                this->m_pointedAt->createEntry(provider);

                ImGui::TreePop();
            }
        }

        std::optional<u32> highlightBytes(size_t offset) override {
            if (offset >= this->getOffset() && offset < (this->getOffset() + this->getSize()))
                return this->getColor();
            else if (auto color = this->m_pointedAt->highlightBytes(offset); color.has_value())
                return color.value();
            else
                return { };
        }

        std::map<u64, u32> getHighlightedAddresses() override {
            if (this->m_highlightedAddresses.empty()) {
                auto ownAddresses = PatternData::getHighlightedAddresses();
                auto pointedToAddresses = this->m_pointedAt->getHighlightedAddresses();

                ownAddresses.merge(pointedToAddresses);
                this->m_highlightedAddresses = ownAddresses;
            }

            return this->m_highlightedAddresses;
        }
        [[nodiscard]] std::string getFormattedName() const override {
            std::string result = this->m_pointedAt->getFormattedName() + "* : ";
            switch (this->getSize()) {
                case 1:     result += "u8";  break;
                case 2:     result += "u16";  break;
                case 4:     result += "u32";  break;
                case 8:     result += "u64";  break;
                case 16:    result += "u128"; break;
            }

            return result;
        }

        void setPointedAtPattern(PatternData *pattern) {
            this->m_pointedAt = pattern;
            this->m_pointedAt->setVariableName("*" + this->getVariableName());
        }

        [[nodiscard]] PatternData* getPointedAtPattern() {
            return this->m_pointedAt;
        }

    private:
        PatternData *m_pointedAt;
    };

    class PatternDataUnsigned : public PatternData {
    public:
        PatternDataUnsigned(u64 offset, size_t size, u32 color = 0)
            : PatternData(offset, size, color) { }

        PatternData* clone() override {
            return new PatternDataUnsigned(*this);
        }

        void createEntry(prv::Provider* &provider) override {
            u64 data = 0;
            provider->read(this->getOffset(), &data, this->getSize());
            data = hex::changeEndianess(data, this->getSize(), this->getEndian());

            this->createDefaultEntry(hex::format("{:d} (0x{:0{}X})", data, data, this->getSize() * 2));
        }

        [[nodiscard]] std::string getFormattedName() const override {
            switch (this->getSize()) {
                case 1:     return "u8";
                case 2:     return "u16";
                case 4:     return "u32";
                case 8:     return "u64";
                case 16:    return "u128";
                default:    return "Unsigned data";
            }
        }
    };

    class PatternDataSigned : public PatternData {
    public:
        PatternDataSigned(u64 offset, size_t size, u32 color = 0)
            : PatternData(offset, size, color) { }

        PatternData* clone() override {
            return new PatternDataSigned(*this);
        }

       void createEntry(prv::Provider* &provider) override {
            u128 data = 0;
            provider->read(this->getOffset(), &data, this->getSize());
            data = hex::changeEndianess(data, this->getSize(), this->getEndian());

            switch (this->getSize()) {
                case 1: {
                    s8 signedData;
                    std::memcpy(&signedData, &data, 1);
                    this->createDefaultEntry(hex::format("{:d} (0x{:0{}X})", signedData, data, 1 * 2));
                }
                break;
                case 2: {
                    s16 signedData;
                    std::memcpy(&signedData, &data, 2);
                    this->createDefaultEntry(hex::format("{:d} (0x{:0{}X})", signedData, data, 2 * 2));
                }
                break;
                case 4: {
                    s32 signedData;
                    std::memcpy(&signedData, &data, 4);
                    this->createDefaultEntry(hex::format("{:d} (0x{:0{}X})", signedData, data, 4 * 2));
                }
                break;
                case 8: {
                    s64 signedData;
                    std::memcpy(&signedData, &data, 8);
                    this->createDefaultEntry(hex::format("{:d} (0x{:0{}X})", signedData, data, 8 * 2));
                }
                break;
                case 16: {
                    s128 signedData;
                    std::memcpy(&signedData, &data, 16);
                    this->createDefaultEntry(hex::format("{:d} (0x{:0{}X})", signedData, data, 16 * 2));
                }
                break;
            }
        }

        [[nodiscard]] std::string getFormattedName() const override {
            switch (this->getSize()) {
                case 1:     return "s8";
                case 2:     return "s16";
                case 4:     return "s32";
                case 8:     return "s64";
                case 16:    return "s128";
                default:    return "Signed data";
            }
        }
    };

    class PatternDataFloat : public PatternData {
    public:
        PatternDataFloat(u64 offset, size_t size, u32 color = 0)
            : PatternData(offset, size, color) { }

        PatternData* clone() override {
            return new PatternDataFloat(*this);
        }

        void createEntry(prv::Provider* &provider) override {
            if (this->getSize() == 4) {
                u32 data = 0;
                provider->read(this->getOffset(), &data, 4);
                data = hex::changeEndianess(data, 4, this->getEndian());

                this->createDefaultEntry(hex::format("{:e} (0x{:0{}X})", *reinterpret_cast<float*>(&data), data, this->getSize() * 2));
            } else if (this->getSize() == 8) {
                u64 data = 0;
                provider->read(this->getOffset(), &data, 8);
                data = hex::changeEndianess(data, 8, this->getEndian());

                this->createDefaultEntry(hex::format("{:e} (0x{:0{}X})", *reinterpret_cast<double*>(&data), data, this->getSize() * 2));
            }
        }

        [[nodiscard]] std::string getFormattedName() const override {
            switch (this->getSize()) {
                case 4:     return "float";
                case 8:     return "double";
                default:    return "Floating point data";
            }
        }
    };

    class PatternDataBoolean : public PatternData {
    public:
        explicit PatternDataBoolean(u64 offset, u32 color = 0)
                : PatternData(offset, 1, color) { }

        PatternData* clone() override {
            return new PatternDataBoolean(*this);
        }

        void createEntry(prv::Provider* &provider) override {
            u8 boolean;
            provider->read(this->getOffset(), &boolean, 1);

            if (boolean == 0)
                this->createDefaultEntry("false");
            else if (boolean == 1)
                this->createDefaultEntry("true");
            else
                this->createDefaultEntry("true*");
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return "bool";
        }
    };

    class PatternDataCharacter : public PatternData {
    public:
        explicit PatternDataCharacter(u64 offset, u32 color = 0)
            : PatternData(offset, 1, color) { }

        PatternData* clone() override {
            return new PatternDataCharacter(*this);
        }

        void createEntry(prv::Provider* &provider) override {
            char character;
            provider->read(this->getOffset(), &character, 1);

            this->createDefaultEntry(hex::format("'{0}'", character));
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return "char";
        }
    };

    class PatternDataCharacter16 : public PatternData {
    public:
        explicit PatternDataCharacter16(u64 offset, u32 color = 0)
                : PatternData(offset, 2, color) { }

        PatternData* clone() override {
            return new PatternDataCharacter16(*this);
        }

        void createEntry(prv::Provider* &provider) override {
            char16_t character;
            provider->read(this->getOffset(), &character, 2);

            this->createDefaultEntry(hex::format("'{0}'", std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>{}.to_bytes(character)));
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return "char16";
        }
    };

    class PatternDataString : public PatternData {
    public:
        PatternDataString(u64 offset, size_t size, u32 color = 0)
            : PatternData(offset, size, color) { }

        PatternData* clone() override {
            return new PatternDataString(*this);
        }

        void createEntry(prv::Provider* &provider) override {
            std::vector<u8> buffer(this->getSize() + 1, 0x00);
            provider->read(this->getOffset(), buffer.data(), this->getSize());
            buffer[this->getSize()] = '\0';

            this->createDefaultEntry(hex::format("\"{0}\"", makeDisplayable(buffer.data(), this->getSize()).c_str()));
        }

        [[nodiscard]] std::string getFormattedName() const override {
           return "String";
        }
    };

    class PatternDataString16 : public PatternData {
    public:
        PatternDataString16(u64 offset, size_t size, u32 color = 0)
                : PatternData(offset, size, color) { }

        PatternData* clone() override {
            return new PatternDataString16(*this);
        }

        void createEntry(prv::Provider* &provider) override {
            std::u16string buffer(this->getSize() + 1, 0x00);
            provider->read(this->getOffset(), buffer.data(), this->getSize());
            buffer[this->getSize()] = '\0';

            auto utf8String = std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>{}.to_bytes(buffer);

            this->createDefaultEntry(hex::format("\"{0}\"", utf8String)) ;
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return "String16";
        }
    };

    class PatternDataArray : public PatternData {
    public:
        PatternDataArray(u64 offset, size_t size, u32 color = 0)
            : PatternData(offset, size, color) {
        }

        PatternDataArray(const PatternDataArray &other) : PatternData(other.getOffset(), other.getSize(), other.getColor()) {
            std::vector<PatternData*> entries;
            for (const auto &entry : other.m_entries)
                entries.push_back(entry->clone());

            this->setEntries(entries);
        }

        ~PatternDataArray() override {
            for (const auto &entry : this->m_entries)
                delete entry;
        }

        PatternData* clone() override {
            return new PatternDataArray(*this);
        }

        void createEntry(prv::Provider* &provider) override {
            if (this->m_entries.empty())
                return;

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            bool open = ImGui::TreeNodeEx(this->getVariableName().c_str(), ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_AllowItemOverlap);
            this->drawCommentTooltip();
            ImGui::TableNextColumn();
            ImGui::ColorButton("color", ImColor(this->getColor()), ImGuiColorEditFlags_NoTooltip, ImVec2(ImGui::GetColumnWidth(), ImGui::GetTextLineHeight()));
            ImGui::TableNextColumn();
            ImGui::Text("0x%08llX : 0x%08llX", this->getOffset(), this->getOffset() + this->getSize() - 1);
            ImGui::TableNextColumn();
            ImGui::Text("0x%04llX", this->getSize());
            ImGui::TableNextColumn();
            ImGui::TextColored(ImColor(0xFF9BC64D), "%s", this->m_entries[0]->getTypeName().c_str());
            ImGui::SameLine(0, 0);

            ImGui::TextUnformatted("[");
            ImGui::SameLine(0, 0);
            ImGui::TextColored(ImColor(0xFF00FF00), "%llu", this->m_entries.size());
            ImGui::SameLine(0, 0);
            ImGui::TextUnformatted("]");

            ImGui::TableNextColumn();
            ImGui::Text("%s", "{ ... }");

            if (open) {
                for (auto &member : this->m_entries)
                    member->draw(provider);

                ImGui::TreePop();
            }
        }

        std::optional<u32> highlightBytes(size_t offset) override{
            for (auto &entry : this->m_entries) {
                if (auto color = entry->highlightBytes(offset); color.has_value())
                    return color.value();
            }

            return { };
        }

        std::map<u64, u32> getHighlightedAddresses() override {
            if (this->m_highlightedAddresses.empty()) {
                for (auto &entry : this->m_entries) {
                    this->m_highlightedAddresses.merge(entry->getHighlightedAddresses());
                }
            }

            return this->m_highlightedAddresses;
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return this->m_entries[0]->getTypeName() + "[" + std::to_string(this->m_entries.size()) + "]";
        }

        [[nodiscard]] const std::vector<PatternData*>& getEntries() {
            return this->m_entries;
        }

        void setEntries(const std::vector<PatternData*> &entries) {
            this->m_entries = entries;

            for (auto &entry : this->m_entries) {
                entry->setColor(this->getColor());
                entry->setParent(this);
            }
        }

    private:
        std::vector<PatternData*> m_entries;
    };

    class PatternDataStruct : public PatternData {
    public:
        PatternDataStruct(u64 offset, size_t size, u32 color = 0) : PatternData(offset, size, color){
        }

        PatternDataStruct(const PatternDataStruct &other) : PatternData(other.getOffset(), other.getSize(), other.getColor()) {
            for (const auto &member : other.m_members)
                this->m_members.push_back(member->clone());
        }

        ~PatternDataStruct() override {
            for (const auto &member : this->m_members)
                delete member;
        }

        PatternData* clone() override {
            return new PatternDataStruct(*this);
        }

        void createEntry(prv::Provider* &provider) override {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            bool open = ImGui::TreeNodeEx(this->getVariableName().c_str(), ImGuiTreeNodeFlags_SpanFullWidth);
            this->drawCommentTooltip();
            ImGui::TableNextColumn();
            ImGui::TableNextColumn();
            ImGui::Text("0x%08llX : 0x%08llX", this->getOffset(), this->getOffset() + this->getSize() - (this->getSize() == 0 ? 0 : 1));
            ImGui::TableNextColumn();
            ImGui::Text("0x%04llX", this->getSize());
            ImGui::TableNextColumn();
            ImGui::TextColored(ImColor(0xFFD69C56), "struct"); ImGui::SameLine(); ImGui::Text("%s", this->getTypeName().c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", "{ ... }");

            if (open) {
                for (auto &member : this->m_sortedMembers)
                    member->draw(provider);

                ImGui::TreePop();
            }

        }

        std::optional<u32> highlightBytes(size_t offset) override{
            for (auto &member : this->m_members) {
                if (auto color = member->highlightBytes(offset); color.has_value())
                    return color.value();
            }

            return { };
        }

        std::map<u64, u32> getHighlightedAddresses() override {
            if (this->m_highlightedAddresses.empty()) {
                for (auto &member : this->m_members) {
                    this->m_highlightedAddresses.merge(member->getHighlightedAddresses());
                }
            }

            return this->m_highlightedAddresses;
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

        [[nodiscard]] const auto& getMembers() const {
            return this->m_members;
        }

        void setMembers(const std::vector<PatternData*> & members) {
            this->m_members.clear();

            for (auto &member : members) {
                if (member == nullptr) continue;

                this->m_members.push_back(member);
                member->setParent(this);
            }

            this->m_sortedMembers = this->m_members;
        }

    private:
        std::vector<PatternData*> m_members;
        std::vector<PatternData*> m_sortedMembers;
    };

    class PatternDataUnion : public PatternData {
    public:
        PatternDataUnion(u64 offset, size_t size, u32 color = 0) : PatternData(offset, size, color) {

        }

        PatternDataUnion(const PatternDataUnion &other) : PatternData(other.getOffset(), other.getSize(), other.getColor()) {
            for (const auto &member : other.m_members)
                this->m_members.push_back(member->clone());
        }

        ~PatternDataUnion() override {
            for (const auto &member : this->m_members)
                delete member;
        }

        PatternData* clone() override {
            return new PatternDataUnion(*this);
        }

        void createEntry(prv::Provider* &provider) override {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            bool open = ImGui::TreeNodeEx(this->getVariableName().c_str(), ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_AllowItemOverlap);
            this->drawCommentTooltip();
            ImGui::TableNextColumn();
            ImGui::TableNextColumn();
            ImGui::Text("0x%08llX : 0x%08llX", this->getOffset(), std::max(this->getOffset() + this->getSize() - (this->getSize() == 0 ? 0 : 1), u64(0)));
            ImGui::TableNextColumn();
            ImGui::Text("0x%04llX", this->getSize());
            ImGui::TableNextColumn();
            ImGui::TextColored(ImColor(0xFFD69C56), "union"); ImGui::SameLine(); ImGui::Text("%s", PatternData::getTypeName().c_str());

            ImGui::TableNextColumn();
            ImGui::Text("%s", "{ ... }");

            if (open) {
                for (auto &member : this->m_sortedMembers)
                    member->draw(provider);

                ImGui::TreePop();
            }

        }

        std::optional<u32> highlightBytes(size_t offset) override{
            for (auto &member : this->m_members) {
                if (auto color = member->highlightBytes(offset); color.has_value())
                    return color.value();
            }

            return { };
        }

        std::map<u64, u32> getHighlightedAddresses() override {
            if (this->m_highlightedAddresses.empty()) {
                for (auto &member : this->m_members) {
                    this->m_highlightedAddresses.merge(member->getHighlightedAddresses());
                }
            }

            return this->m_highlightedAddresses;
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
            return "union " + PatternData::getTypeName();;
        }

        [[nodiscard]] const auto& getMembers() const {
            return this->m_members;
        }

        void setMembers(const std::vector<PatternData*> & members) {
            for (auto &member : members) {
                if (member == nullptr) continue;

                this->m_members.push_back(member);
                member->setParent(this);
            }

            this->m_sortedMembers = this->m_members;
        }

    private:
        std::vector<PatternData*> m_members;
        std::vector<PatternData*> m_sortedMembers;
    };

    class PatternDataEnum : public PatternData {
    public:
        PatternDataEnum(u64 offset, size_t size, u32 color = 0) : PatternData(offset, size, color) {

        }

        PatternData* clone() override {
            return new PatternDataEnum(*this);
        }

        void createEntry(prv::Provider* &provider) override {
            u64 value = 0;
            provider->read(this->getOffset(), &value, this->getSize());
            value = hex::changeEndianess(value, this->getSize(), this->getEndian());

            std::string valueString = PatternData::getTypeName() + "::";

            bool foundValue = false;
            for (auto &[entryValueLiteral, entryName] : this->m_enumValues) {
                bool matches = std::visit([&, name = entryName](auto &&entryValue) {
                    if (value == entryValue) {
                        valueString += name;
                        foundValue = true;
                        return true;
                    }

                    return false;
                }, entryValueLiteral);
                if (matches)
                    break;
            }

            if (!foundValue)
                valueString += "???";

            ImGui::TableNextRow();
            ImGui::TreeNodeEx(this->getVariableName().c_str(), ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_AllowItemOverlap);
            this->drawCommentTooltip();
            ImGui::TableNextColumn();
            if (ImGui::Selectable(("##PatternDataLine"s + std::to_string(this->getOffset())).c_str(), false, ImGuiSelectableFlags_SpanAllColumns)) {
                EventManager::post<RequestSelectionChange>(Region { this->getOffset(), this->getSize() });
            }
            ImGui::SameLine();
            ImGui::Text("%s", this->getVariableName().c_str());
            ImGui::TableNextColumn();
            ImGui::ColorButton("color", ImColor(this->getColor()), ImGuiColorEditFlags_NoTooltip, ImVec2(ImGui::GetColumnWidth(), ImGui::GetTextLineHeight()));
            ImGui::TableNextColumn();
            ImGui::Text("0x%08llX : 0x%08llX", this->getOffset(), this->getOffset() + this->getSize() - 1);
            ImGui::TableNextColumn();
            ImGui::Text("0x%04llX", this->getSize());
            ImGui::TableNextColumn();
            ImGui::TextColored(ImColor(0xFFD69C56), "enum"); ImGui::SameLine(); ImGui::Text("%s", PatternData::getTypeName().c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", hex::format("{} (0x{:0{}X})", valueString.c_str(), value, this->getSize() * 2).c_str());
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return "enum " + PatternData::getTypeName();
        }

        [[nodiscard]] const auto& getEnumValues() const {
            return this->m_enumValues;
        }

        void setEnumValues(const std::vector<std::pair<Token::IntegerLiteral, std::string>> &enumValues) {
            this->m_enumValues = enumValues;
        }

    private:
        std::vector<std::pair<Token::IntegerLiteral, std::string>> m_enumValues;
    };

    class PatternDataBitfield : public PatternData {
    public:
        PatternDataBitfield(u64 offset, size_t size, u32 color = 0) : PatternData(offset, size, color) {

        }

        PatternData* clone() override {
            return new PatternDataBitfield(*this);
        }

        void createEntry(prv::Provider* &provider) override {
            std::vector<u8> value(this->getSize(), 0);
            provider->read(this->getOffset(), &value[0], value.size());

            if (this->m_endian == std::endian::big)
                std::reverse(value.begin(), value.end());

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            bool open = ImGui::TreeNodeEx(this->getVariableName().c_str(), ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_AllowItemOverlap);
            this->drawCommentTooltip();
            ImGui::TableNextColumn();
            ImGui::TableNextColumn();
            ImGui::Text("0x%08llX : 0x%08llX", this->getOffset(), this->getOffset() + this->getSize() - 1);
            ImGui::TableNextColumn();
            ImGui::Text("0x%04llX", this->getSize());
            ImGui::TableNextColumn();
            ImGui::TextColored(ImColor(0xFFD69C56), "bitfield"); ImGui::SameLine(); ImGui::Text("%s", PatternData::getTypeName().c_str());
            ImGui::TableNextColumn();

            std::string valueString = "{ ";
            for (auto i : value)
                valueString += hex::format("{0:02X} ", i);
            valueString += "}";

            ImGui::TextUnformatted(valueString.c_str());

            if (open) {
                u16 bitOffset = 0;
                for (auto &[entryName, entrySize] : this->m_fields) {
                    ImGui::TableNextRow();
                    ImGui::TreeNodeEx(this->getVariableName().c_str(), ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_AllowItemOverlap);
                    ImGui::TableNextColumn();
                    ImGui::Text("%s", entryName.c_str());
                    ImGui::TableNextColumn();
                    ImGui::ColorButton("color", ImColor(this->getColor()), ImGuiColorEditFlags_NoTooltip, ImVec2(ImGui::GetColumnWidth(), ImGui::GetTextLineHeight()));
                    ImGui::TableNextColumn();
                    ImGui::Text("0x%08llX : 0x%08llX", this->getOffset() + (bitOffset >> 3), this->getOffset() + ((bitOffset + entrySize) >> 3));
                    ImGui::TableNextColumn();
                    if (entrySize == 1)
                        ImGui::Text("%llu bit", entrySize);
                    else
                        ImGui::Text("%llu bits", entrySize);
                    ImGui::TableNextColumn();
                    ImGui::TextColored(ImColor(0xFF9BC64D), "bits");
                    ImGui::TableNextColumn();
                    {
                        u128 fieldValue = 0;
                        std::memcpy(&fieldValue, value.data() + (bitOffset / 8), (entrySize / 8) + 1);
                        ImGui::Text("%llX", hex::extract((bitOffset + entrySize) - 1 - ((bitOffset / 8) * 8), bitOffset - ((bitOffset / 8) * 8), fieldValue));
                    }
                    bitOffset += entrySize;
                }

                ImGui::TreePop();
            }

        }

        [[nodiscard]] std::string getFormattedName() const override {
            return "bitfield " + PatternData::getTypeName();
        }

        [[nodiscard]] const auto& getFields() const {
            return this->m_fields;
        }

        void setFields(const std::vector<std::pair<std::string, size_t>> &fields) {
            this->m_fields = fields;
        }

    private:
        std::vector<std::pair<std::string, size_t>> m_fields;
    };

}