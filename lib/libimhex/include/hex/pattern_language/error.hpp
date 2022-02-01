#pragma once

#include <hex.hpp>

#include <stdexcept>
#include <string>

namespace hex::pl {

    class PatternLanguageError : public std::exception {
    public:
        PatternLanguageError(u32 lineNumber, std::string message) : m_lineNumber(lineNumber), m_message(std::move(message)) { }

        [[nodiscard]] const char *what() const noexcept override {
            return this->m_message.c_str();
        }

        [[nodiscard]] u32 getLineNumber() const {
            return this->m_lineNumber;
        }

    private:
        u32 m_lineNumber;
        std::string m_message;
    };

}