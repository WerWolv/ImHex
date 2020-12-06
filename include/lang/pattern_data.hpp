#pragma once

#include <hex.hpp>
#include <string>

#include "imgui.h"
#include "imgui_memory_editor.h"

#include "providers/provider.hpp"
#include "helpers/utils.hpp"

#include <random>

namespace hex::lang {

    using namespace ::std::literals::string_literals;

    namespace {

        std::string makeDisplayable(u8 *data, size_t size) {
            std::string result;
            for (u8* c = data; c < (data + size); c++) {
                if (iscntrl(*c) || *c > 0x7F)
                    result += " ";
                else
                    result += *c;
            }

            return result;
        }

    }

    class PatternData {
    public:
        enum class Type { Padding, Unsigned, Signed, Float, Character, String, Struct, Union, Array, Enum };

        PatternData(Type type, u64 offset, size_t size, const std::string &name, std::endian endianess, u32 color = 0)
        : m_type(type), m_offset(offset), m_size(size), m_name(name), m_endianess(endianess), m_color(color) {
            constexpr u32 Palette[] = { 0x50b4771f, 0x500e7fff, 0x502ca02c, 0x502827d6, 0x50bd6794, 0x504b568c, 0x50c277e3, 0x507f7f7f, 0x5022bdbc, 0x50cfbe17 };

            if (color != 0)
                return;

            this->m_color = Palette[PatternData::s_paletteOffset++];

            if (PatternData::s_paletteOffset >= (sizeof(Palette) / sizeof(u32)))
                PatternData::s_paletteOffset = 0;
        }
        virtual ~PatternData() = default;

        [[nodiscard]] Type getPatternType() const { return this->m_type; }
        [[nodiscard]] u64 getOffset() const { return this->m_offset; }
        [[nodiscard]] size_t getSize() const { return this->m_size; }

        [[nodiscard]] const std::string& getName() const { return this->m_name; }
        void setName(std::string name) { this->m_name = name; }

        [[nodiscard]] u32 getColor() const { return this->m_color; }
        void setColor(u32 color) { this->m_color = color; }

        [[nodiscard]] std::endian getEndianess() const { return this->m_endianess; }
        void setEndianess(std::endian endianess) { this->m_endianess = endianess; }

        virtual void createEntry(prv::Provider* &provider) = 0;
        virtual std::string getTypeName() = 0;

        virtual std::optional<u32> highlightBytes(size_t offset) {
            if (offset >= this->getOffset() && offset < (this->getOffset() + this->getSize()))
                return this->getColor();
            else
                return { };
        }

        virtual void sort(ImGuiTableSortSpecs *sortSpecs, prv::Provider *provider) { }

        static bool sortPatternDataTable(ImGuiTableSortSpecs *sortSpecs, prv::Provider *provider, lang::PatternData* left, lang::PatternData* right) {
            if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("name")) {
                if (sortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending)
                    return left->getName() > right->getName();
                else
                    return left->getName() < right->getName();
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

                if (left->m_endianess != std::endian::native)
                    std::reverse(leftBuffer.begin(), leftBuffer.end());
                if (right->m_endianess != std::endian::native)
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

        static void resetPalette() { PatternData::s_paletteOffset = 0; }

    protected:
        void createDefaultEntry(std::string value) {
            ImGui::TableNextRow();
            ImGui::TreeNodeEx(this->getName().c_str(), ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_AllowItemOverlap);
            ImGui::TableNextColumn();
            if (ImGui::Selectable(("##PatternDataLine"s + std::to_string(this->getOffset())).c_str(), false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap)) {
                Region selectRegion = { this->getOffset(), this->getSize() };
                View::postEvent(Events::SelectionChangeRequest, &selectRegion);
            }
            ImGui::SameLine();
            ImGui::Text("%s", this->getName().c_str());
            ImGui::TableNextColumn();
            ImGui::ColorButton("color", ImColor(this->getColor()), ImGuiColorEditFlags_NoTooltip, ImVec2(ImGui::GetColumnWidth(), 14));
            ImGui::TableNextColumn();
            ImGui::Text("0x%08lx : 0x%08lx", this->getOffset(), this->getOffset() + this->getSize() - 1);
            ImGui::TableNextColumn();
            ImGui::Text("0x%04lx", this->getSize());
            ImGui::TableNextColumn();
            ImGui::TextColored(ImColor(0xFF9BC64D), "%s", this->getTypeName().c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", value.c_str());
        }

    protected:
        std::endian m_endianess = std::endian::native;

    private:
        Type m_type;
        u64 m_offset;
        size_t m_size;

        u32 m_color;
        std::string m_name;

        static inline u8 s_paletteOffset = 0;

    };

    class PatternDataPadding : public PatternData {
    public:
        PatternDataPadding(u64 offset, size_t size) : PatternData(Type::Padding, offset, size, "", { }, 0x00FFFFFF) { }

        void createEntry(prv::Provider* &provider) override {
        }

        std::string getTypeName() override {
            return "";
        }
    };

    class PatternDataPointer : public PatternData {
    public:
        PatternDataPointer(u64 offset, size_t size, const std::string &name, PatternData *pointedAt, std::endian endianess, u32 color = 0)
        : PatternData(Type::Unsigned, offset, size, name, endianess, color), m_pointedAt(pointedAt) {
            this->m_pointedAt->setName("*" + this->m_pointedAt->getName());
        }

        void createEntry(prv::Provider* &provider) override {
            u64 data = 0;
            provider->read(this->getOffset(), &data, this->getSize());
            data = hex::changeEndianess(data, this->getSize(), this->m_endianess);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            bool open = ImGui::TreeNodeEx(this->getName().c_str(), ImGuiTreeNodeFlags_SpanFullWidth);
            ImGui::TableNextColumn();
            ImGui::ColorButton("color", ImColor(this->getColor()), ImGuiColorEditFlags_NoTooltip, ImVec2(ImGui::GetColumnWidth(), 14));
            ImGui::TableNextColumn();
            ImGui::Text("0x%08lx : 0x%08lx", this->getOffset(), this->getOffset() + this->getSize() - 1);
            ImGui::TableNextColumn();
            ImGui::Text("0x%04lx", this->getSize());
            ImGui::TableNextColumn();
            ImGui::TextColored(ImColor(0xFF9BC64D), "%s", this->getTypeName().c_str());
            ImGui::TableNextColumn();
            ImGui::Text("*(0x%0*llx)", this->getSize() * 2, data);

            if (open) {
                this->m_pointedAt->createEntry(provider);

                ImGui::TreePop();
            }
        }

        virtual std::optional<u32> highlightBytes(size_t offset) {
            if (offset >= this->getOffset() && offset < (this->getOffset() + this->getSize()))
                return this->getColor();
            else if (auto color = this->m_pointedAt->highlightBytes(offset); color.has_value())
                return color.value();
            else
                return { };
        }

        std::string getTypeName() override {
            return "Pointer";
        }

    private:
        PatternData *m_pointedAt;
    };

    class PatternDataUnsigned : public PatternData {
    public:
        PatternDataUnsigned(u64 offset, size_t size, const std::string &name, std::endian endianess, u32 color = 0)
            : PatternData(Type::Unsigned, offset, size, name, endianess, color) { }

        void createEntry(prv::Provider* &provider) override {
            u64 data = 0;
            provider->read(this->getOffset(), &data, this->getSize());
            data = hex::changeEndianess(data, this->getSize(), this->m_endianess);

            this->createDefaultEntry(hex::format("%lu (0x%0*lx)", data, this->getSize() * 2, data));
        }

        std::string getTypeName() override {
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
        PatternDataSigned(u64 offset, size_t size, const std::string &name, std::endian endianess, u32 color = 0)
            : PatternData(Type::Signed, offset, size, name, endianess, color) { }

       void createEntry(prv::Provider* &provider) override {
            u64 data = 0;
            provider->read(this->getOffset(), &data, this->getSize());
            data = hex::changeEndianess(data, this->getSize(), this->m_endianess);

            s64 signedData = signedData = hex::signExtend(data, this->getSize(), 64);

           this->createDefaultEntry(hex::format("%ld (0x%0*lx)", signedData, this->getSize() * 2, data));
        }

        std::string getTypeName() override {
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
        PatternDataFloat(u64 offset, size_t size, const std::string &name, std::endian endianess, u32 color = 0)
            : PatternData(Type::Float, offset, size, name, endianess, color) { }

        void createEntry(prv::Provider* &provider) override {
            double formatData = 0;
            if (this->getSize() == 4) {
                float data = 0;
                provider->read(this->getOffset(), &data, 4);
                data = hex::changeEndianess(data, 4, this->m_endianess);

                formatData = data;
            } else if (this->getSize() == 8) {
                double data = 0;
                provider->read(this->getOffset(), &data, 8);
                data = hex::changeEndianess(data, 8, this->m_endianess);

                formatData = data;
            }

            this->createDefaultEntry(hex::format("%f (0x%0*lx)", formatData, this->getSize() * 2, formatData));
        }

        std::string getTypeName() override {
            switch (this->getSize()) {
                case 4:     return "float";
                case 8:     return "double";
                default:    return "Floating point data";
            }
        }
    };

    class PatternDataCharacter : public PatternData {
    public:
        PatternDataCharacter(u64 offset, size_t size, const std::string &name, std::endian endianess, u32 color = 0)
            : PatternData(Type::Character, offset, size, name, endianess, color) { }

        void createEntry(prv::Provider* &provider) override {
            char character;
            provider->read(this->getOffset(), &character, 1);

            this->createDefaultEntry(hex::format("'%c'", character));
        }

        std::string getTypeName() override {
            return "Character";
        }
    };

    class PatternDataString : public PatternData {
    public:
        PatternDataString(u64 offset, size_t size, const std::string &name, std::endian endianess, u32 color = 0)
            : PatternData(Type::String, offset, size, name, endianess, color) { }

        void createEntry(prv::Provider* &provider) override {
            std::vector<u8> buffer(this->getSize() + 1, 0x00);
            provider->read(this->getOffset(), buffer.data(), this->getSize());
            buffer[this->getSize()] = '\0';

            this->createDefaultEntry(hex::format("\"%s\"", makeDisplayable(buffer.data(), this->getSize()).c_str()));
        }

        std::string getTypeName() override {
           return "String";
        }
    };

    class PatternDataArray : public PatternData {
    public:
        PatternDataArray(u64 offset, size_t size, const std::string &name, std::endian endianess, const std::vector<PatternData*> & entries, u32 color = 0)
            : PatternData(Type::Array, offset, size, name, endianess, color), m_entries(entries) { }

        void createEntry(prv::Provider* &provider) override {
            if (this->m_entries.empty())
                return;

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            bool open = ImGui::TreeNodeEx(this->getName().c_str(), ImGuiTreeNodeFlags_SpanFullWidth);
            ImGui::TableNextColumn();
            ImGui::ColorButton("color", ImColor(this->getColor()), ImGuiColorEditFlags_NoTooltip, ImVec2(ImGui::GetColumnWidth(), 14));
            ImGui::TableNextColumn();
            ImGui::Text("0x%08lx : 0x%08lx", this->getOffset(), this->getOffset() + this->getSize() - 1);
            ImGui::TableNextColumn();
            ImGui::Text("0x%04lx", this->getSize());
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
                    member->createEntry(provider);

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

        std::string getTypeName() override {
            return this->m_entries[0]->getTypeName() + "[" + std::to_string(this->m_entries.size()) + "]";
        }

    private:
        std::vector<PatternData*> m_entries;
    };

    class PatternDataStruct : public PatternData {
    public:
        PatternDataStruct(u64 offset, size_t size, const std::string &name, std::endian endianess, const std::string &structName, const std::vector<PatternData*> & members, u32 color = 0)
                : PatternData(Type::Struct, offset, size, name, endianess, color), m_structName(structName), m_members(members), m_sortedMembers(members) { }

        void createEntry(prv::Provider* &provider) override {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            bool open = ImGui::TreeNodeEx(this->getName().c_str(), ImGuiTreeNodeFlags_SpanFullWidth);
            ImGui::TableNextColumn();
            ImGui::TableNextColumn();
            ImGui::Text("0x%08lx : 0x%08lx", this->getOffset(), this->getOffset() + this->getSize() - 1);
            ImGui::TableNextColumn();
            ImGui::Text("0x%04lx", this->getSize());
            ImGui::TableNextColumn();
            ImGui::TextColored(ImColor(0xFFD69C56), "struct"); ImGui::SameLine(); ImGui::Text("%s", this->m_structName.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", "{ ... }");

            if (open) {
                for (auto &member : this->m_sortedMembers)
                    member->createEntry(provider);

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

        void sort(ImGuiTableSortSpecs *sortSpecs, prv::Provider *provider) override {
            this->m_sortedMembers = this->m_members;

            std::sort(this->m_sortedMembers.begin(), this->m_sortedMembers.end(), [&sortSpecs, &provider](PatternData *left, PatternData *right) {
                return PatternData::sortPatternDataTable(sortSpecs, provider, left, right);
            });

            for (auto &member : this->m_members)
                member->sort(sortSpecs, provider);
        }

        std::string getTypeName() override {
            return "struct " + this->m_structName;
        }

    private:
        std::string m_structName;
        std::vector<PatternData*> m_members;
        std::vector<PatternData*> m_sortedMembers;
    };

    class PatternDataUnion : public PatternData {
    public:
        PatternDataUnion(u64 offset, size_t size, const std::string &name, const std::string &unionName, const std::vector<PatternData*> & members, std::endian endianess, u32 color = 0)
                : PatternData(Type::Union, offset, size, name, endianess, color), m_unionName(unionName), m_members(members), m_sortedMembers(members) { }

        void createEntry(prv::Provider* &provider) override {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            bool open = ImGui::TreeNodeEx(this->getName().c_str(), ImGuiTreeNodeFlags_SpanFullWidth);
            ImGui::TableNextColumn();
            ImGui::TableNextColumn();
            ImGui::Text("0x%08lx : 0x%08lx", this->getOffset(), this->getOffset() + this->getSize() - 1);
            ImGui::TableNextColumn();
            ImGui::Text("0x%04lx", this->getSize());
            ImGui::TableNextColumn();
            ImGui::TextColored(ImColor(0xFFD69C56), "union"); ImGui::SameLine(); ImGui::Text("%s", this->m_unionName.c_str());

            ImGui::TableNextColumn();
            ImGui::Text("%s", "{ ... }");

            if (open) {
                for (auto &member : this->m_sortedMembers)
                    member->createEntry(provider);

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

        void sort(ImGuiTableSortSpecs *sortSpecs, prv::Provider *provider) override {
            this->m_sortedMembers = this->m_members;

            std::sort(this->m_sortedMembers.begin(), this->m_sortedMembers.end(), [&sortSpecs, &provider](PatternData *left, PatternData *right) {
                return PatternData::sortPatternDataTable(sortSpecs, provider, left, right);
            });

            for (auto &member : this->m_members)
                member->sort(sortSpecs, provider);
        }

        std::string getTypeName() override {
            return "union " + this->m_unionName;
        }

    private:
        std::string m_unionName;
        std::vector<PatternData*> m_members;
        std::vector<PatternData*> m_sortedMembers;
    };

    class PatternDataEnum : public PatternData {
    public:
        PatternDataEnum(u64 offset, size_t size, const std::string &name, const std::string &enumName, std::vector<std::pair<u64, std::string>> enumValues, std::endian endianess, u32 color = 0)
            : PatternData(Type::Enum, offset, size, name, endianess, color), m_enumName(enumName), m_enumValues(enumValues) { }

        void createEntry(prv::Provider* &provider) override {
            u64 value = 0;
            provider->read(this->getOffset(), &value, this->getSize());
            value = hex::changeEndianess(value, this->getSize(), this->m_endianess);

            std::string valueString = this->m_enumName + "::";

            bool foundValue = false;
            for (auto &[entryValue, entryName] : this->m_enumValues) {
                if (value == entryValue) {
                    valueString += entryName;
                    foundValue = true;
                    break;
                }
            }

            if (!foundValue)
                valueString += "???";

            ImGui::TableNextRow();
            ImGui::TreeNodeEx(this->getName().c_str(), ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanFullWidth);
            ImGui::TableNextColumn();
            if (ImGui::Selectable(("##PatternDataLine"s + std::to_string(this->getOffset())).c_str(), false, ImGuiSelectableFlags_SpanAllColumns)) {
                Region selectRegion = { this->getOffset(), this->getSize() };
                View::postEvent(Events::SelectionChangeRequest, &selectRegion);
            }
            ImGui::SameLine();
            ImGui::Text("%s", this->getName().c_str());
            ImGui::TableNextColumn();
            ImGui::ColorButton("color", ImColor(this->getColor()), ImGuiColorEditFlags_NoTooltip, ImVec2(ImGui::GetColumnWidth(), 14));
            ImGui::TableNextColumn();
            ImGui::Text("0x%08lx : 0x%08lx", this->getOffset(), this->getOffset() + this->getSize() - 1);
            ImGui::TableNextColumn();
            ImGui::Text("0x%04lx", this->getSize());
            ImGui::TableNextColumn();
            ImGui::TextColored(ImColor(0xFFD69C56), "enum"); ImGui::SameLine(); ImGui::Text("%s", this->m_enumName.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", hex::format("%s (0x%0*lx)", valueString.c_str(), this->getSize() * 2, value).c_str());
        }

        std::string getTypeName() override {
            return "enum " + this->m_enumName;
        }

    private:
        std::string m_enumName;
        std::vector<std::pair<u64, std::string>> m_enumValues;
    };

    class PatternDataBitfield : public PatternData {
    public:
        PatternDataBitfield(u64 offset, size_t size, const std::string &name, const std::string &bitfieldName, std::vector<std::pair<std::string, size_t>> fields, std::endian endianess, u32 color = 0)
                : PatternData(Type::Enum, offset, size, name, endianess, color), m_bitfieldName(bitfieldName), m_fields(fields) { }

        void createEntry(prv::Provider* &provider) override {
            u64 value = 0;
            provider->read(this->getOffset(), &value, this->getSize());
            value = hex::changeEndianess(value, this->getSize(), this->m_endianess);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            bool open = ImGui::TreeNodeEx(this->getName().c_str(), ImGuiTreeNodeFlags_SpanFullWidth);
            ImGui::TableNextColumn();
            ImGui::TableNextColumn();
            ImGui::Text("0x%08lx : 0x%08lx", this->getOffset(), this->getOffset() + this->getSize() - 1);
            ImGui::TableNextColumn();
            ImGui::Text("0x%04lx", this->getSize());
            ImGui::TableNextColumn();
            ImGui::TextColored(ImColor(0xFFD69C56), "bitfield"); ImGui::SameLine(); ImGui::Text("%s", this->m_bitfieldName.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("{ %llx }", value);

            if (open) {
                u16 bitOffset = 0;
                for (auto &[entryName, entrySize] : this->m_fields) {
                    ImGui::TableNextRow();
                    ImGui::TreeNodeEx(this->getName().c_str(), ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanFullWidth);
                    ImGui::TableNextColumn();
                    ImGui::Text("%s", entryName.c_str());
                    ImGui::TableNextColumn();
                    ImGui::ColorButton("color", ImColor(this->getColor()), ImGuiColorEditFlags_NoTooltip, ImVec2(ImGui::GetColumnWidth(), 14));
                    ImGui::TableNextColumn();
                    ImGui::Text("0x%08lx : 0x%08lx", this->getOffset() + (bitOffset >> 3), this->getOffset() + ((bitOffset + entrySize) >> 3) - 1);
                    ImGui::TableNextColumn();
                    if (entrySize == 1)
                        ImGui::Text("%llu bit", entrySize);
                    else
                        ImGui::Text("%llu bits", entrySize);
                    ImGui::TableNextColumn();
                    ImGui::Text("%s", entryName.c_str());
                    ImGui::TableNextColumn();
                    ImGui::Text("%llx", hex::extract((bitOffset + entrySize) - 1, bitOffset, value));
                    bitOffset += entrySize;
                }

                ImGui::TreePop();
            }

        }

        std::string getTypeName() override {
            return "bitfield " + this->m_bitfieldName;
        }

    private:
        std::string m_bitfieldName;
        std::vector<std::pair<std::string, size_t>> m_fields;
    };


}