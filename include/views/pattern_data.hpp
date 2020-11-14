#pragma once

#include <hex.hpp>
#include <string>

#include "providers/provider.hpp"
#include "utils.hpp"

#include <random>

namespace hex {

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
        PatternData(u64 offset, size_t size, const std::string &name, u32 color = 0)
        : m_offset(offset), m_size(size), m_color(color), m_name(name) {
            if (color == 0)
                color = std::mt19937(std::random_device()())();

            color &= ~0xFF00'0000;
            color |= 0x5000'0000;

            this->m_color = color;
        }
        virtual ~PatternData() = default;

        [[nodiscard]] u64 getOffset() const { return this->m_offset; }
        [[nodiscard]] size_t getSize() const { return this->m_size; }

        [[nodiscard]] u32 getColor() const { return this->m_color; }
        [[nodiscard]] const std::string& getName() const { return this->m_name; }

        virtual std::string format(prv::Provider* &provider) = 0;
        virtual std::string getTypeName() = 0;

    private:
        u64 m_offset;
        size_t m_size;

        u32 m_color;
        std::string m_name;
    };


    class PatternDataUnsigned : public PatternData {
    public:
        PatternDataUnsigned(u64 offset, size_t size, const std::string &name, u32 color = 0) : PatternData(offset, size, name, color) { }

        std::string format(prv::Provider* &provider) override {
            u64 data = 0;
            provider->read(this->getOffset(), &data, this->getSize());

            return hex::format("%lu (0x%08lx)", data, data);
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
        PatternDataSigned(u64 offset, size_t size, const std::string &name, u32 color = 0) : PatternData(offset, size, name, color) { }

        std::string format(prv::Provider* &provider) override {
            u64 data = 0;
            provider->read(this->getOffset(), &data, this->getSize());

            s64 signedData = signedData = hex::signExtend(data, this->getSize(), 64);

            return hex::format("%ld (0x%08lx)", signedData, data);
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
        PatternDataFloat(u64 offset, size_t size, const std::string &name, u32 color = 0) : PatternData(offset, size, name, color) { }

        std::string format(prv::Provider* &provider) override {
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

            return hex::format("%f (0x%08lx)", formatData, formatData);
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
        PatternDataCharacter(u64 offset, size_t size, const std::string &name, u32 color = 0) : PatternData(offset, size, name, color) { }

        std::string format(prv::Provider* &provider) override {
            char character;
            provider->read(this->getOffset(), &character, 1);

            return hex::format("'%c'", character);
        }

        std::string getTypeName() override {
            return "Character";
        }
    };

    class PatternDataString : public PatternData {
    public:
        PatternDataString(u64 offset, size_t size, const std::string &name, u32 color = 0) : PatternData(offset, size, name, color) { }

        std::string format(prv::Provider* &provider) override {
            std::vector<u8> buffer(this->getSize() + 1, 0x00);
            provider->read(this->getOffset(), buffer.data(), this->getSize());
            buffer[this->getSize()] = '\0';

            return hex::format("\"%s\"", makeDisplayable(buffer.data(), this->getSize()).c_str());
        }

        std::string getTypeName() override {
           return "String";
        }
    };

    class PatternDataEnum : public PatternData {
    public:
        PatternDataEnum(u64 offset, size_t size, const std::string &name, const std::string &enumName, std::vector<std::pair<u64, std::string>> enumValues, u32 color = 0)
            : PatternData(offset, size, name, color), m_enumName(enumName), m_enumValues(enumValues) { }

        std::string format(prv::Provider* &provider) override {
            u64 value = 0;
            provider->read(this->getOffset(), &value, this->getSize());

            for (auto [enumValue, name] : this->m_enumValues) {
                if (value == enumValue)
                    return hex::format("%lu (0x%08lx)  :  %s::%s", value, value, this->m_enumName.c_str(), name.c_str());
            }

            return hex::format("%lu (0x%08lx)  :  %s::???", value, value, this->m_enumName.c_str());
        }

        std::string getTypeName() override {
            return "enum " + this->m_enumName;
        }

    private:
        std::string m_enumName;
        std::vector<std::pair<u64, std::string>> m_enumValues;
    };


}