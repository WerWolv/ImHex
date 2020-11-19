#pragma once

#include <hex.hpp>
#include <string>

#include "imgui.h"
#include "imgui_memory_editor.h"

#include "providers/provider.hpp"
#include "utils.hpp"

#include <random>

namespace hex::lang {

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
        enum class Type { Unsigned, Signed, Float, Character, String, Struct, Array, Enum };

        PatternData(Type type, u64 offset, size_t size, const std::string &name, u32 color = 0)
        : m_type(type), m_offset(offset), m_size(size), m_color(color), m_name(name) {
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

        [[nodiscard]] u32 getColor() const { return this->m_color; }
        [[nodiscard]] const std::string& getName() const { return this->m_name; }

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
            else if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("position")) {
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
            ImGui::TreeNodeEx(this->getName().c_str(), ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanFullWidth);
            ImGui::TableNextColumn();
            ImGui::ColorButton("color", ImColor(this->getColor()), ImGuiColorEditFlags_NoTooltip);
            ImGui::TableNextColumn();
            ImGui::Text("%s", this->getName().c_str());
            ImGui::TableNextColumn();
            ImGui::Text("0x%08lx : 0x%08lx", this->getOffset(), this->getOffset() + this->getSize());
            ImGui::TableNextColumn();
            ImGui::Text("0x%04lx", this->getSize());
            ImGui::TableNextColumn();
            ImGui::Text("%s", this->getTypeName().c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", value.c_str());
        }

    private:
        Type m_type;
        u64 m_offset;
        size_t m_size;

        u32 m_color;
        std::string m_name;

        static inline u8 s_paletteOffset = 0;
    };


    class PatternDataUnsigned : public PatternData {
    public:
        PatternDataUnsigned(u64 offset, size_t size, const std::string &name, u32 color = 0) : PatternData(Type::Unsigned, offset, size, name, color) { }

        void createEntry(prv::Provider* &provider) override {
            u64 data = 0;
            provider->read(this->getOffset(), &data, this->getSize());

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
        PatternDataSigned(u64 offset, size_t size, const std::string &name, u32 color = 0) : PatternData(Type::Signed, offset, size, name, color) { }

       void createEntry(prv::Provider* &provider) override {
            u64 data = 0;
            provider->read(this->getOffset(), &data, this->getSize());

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
        PatternDataFloat(u64 offset, size_t size, const std::string &name, u32 color = 0) : PatternData(Type::Float, offset, size, name, color) { }

        void createEntry(prv::Provider* &provider) override {
            double formatData = 0;
            if (this->getSize() == 4) {
                float data = 0;
                provider->read(this->getOffset(), &data, 4);
                formatData = data;
            } else if (this->getSize() == 8) {
                double data = 0;
                provider->read(this->getOffset(), &data, 8);
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
        PatternDataCharacter(u64 offset, size_t size, const std::string &name, u32 color = 0) : PatternData(Type::Character, offset, size, name, color) { }

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
        PatternDataString(u64 offset, size_t size, const std::string &name, u32 color = 0) : PatternData(Type::String, offset, size, name, color) { }

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
        PatternDataArray(u64 offset, size_t size, const std::string &name, const std::vector<PatternData*> & entries, u32 color = 0)
            : PatternData(Type::Array, offset, size, name, color), m_entries(entries) { }

        void createEntry(prv::Provider* &provider) override {
            ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
            ImGui::TableNextColumn();
            ImGui::ColorButton("color", ImColor(this->getColor()), ImGuiColorEditFlags_NoTooltip);
            ImGui::TableNextColumn();
            bool open = ImGui::TreeNodeEx(this->getName().c_str(), ImGuiTreeNodeFlags_SpanFullWidth);
            ImGui::TableNextColumn();
            ImGui::Text("0x%08lx : 0x%08lx", this->getOffset(), this->getOffset() + this->getSize());
            ImGui::TableNextColumn();
            ImGui::Text("0x%04lx", this->getSize());
            ImGui::TableNextColumn();
            ImGui::Text("%s", this->getTypeName().c_str());
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
        PatternDataStruct(u64 offset, size_t size, const std::string &name, const std::string &structName, const std::vector<PatternData*> & members, u32 color = 0)
                : PatternData(Type::Struct, offset, size, name, color), m_structName(structName), m_members(members), m_sortedMembers(members) { }

        void createEntry(prv::Provider* &provider) override {
            ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
            ImGui::TableNextColumn();
            ImGui::ColorButton("color", ImColor(this->getColor()), ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_AlphaPreview);
            ImGui::TableNextColumn();
            bool open = ImGui::TreeNodeEx(this->getName().c_str(), ImGuiTreeNodeFlags_SpanFullWidth);
            ImGui::TableNextColumn();
            ImGui::Text("0x%08lx : 0x%08lx", this->getOffset(), this->getOffset() + this->getSize());
            ImGui::TableNextColumn();
            ImGui::Text("0x%04lx", this->getSize());
            ImGui::TableNextColumn();
            ImGui::Text("%s", this->getTypeName().c_str());
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

    class PatternDataEnum : public PatternData {
    public:
        PatternDataEnum(u64 offset, size_t size, const std::string &name, const std::string &enumName, std::vector<std::pair<u64, std::string>> enumValues, u32 color = 0)
            : PatternData(Type::Enum, offset, size, name, color), m_enumName(enumName), m_enumValues(enumValues) { }

        void createEntry(prv::Provider* &provider) override {
            u64 value = 0;
            provider->read(this->getOffset(), &value, this->getSize());

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

            this->createDefaultEntry(hex::format("%s (0x0*lx)", valueString.c_str(), this->getSize() * 2, value));
        }

        std::string getTypeName() override {
            return "enum " + this->m_enumName;
        }

    private:
        std::string m_enumName;
        std::vector<std::pair<u64, std::string>> m_enumValues;
    };


}