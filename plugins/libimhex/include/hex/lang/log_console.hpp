#pragma once

#include <hex.hpp>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace hex::lang {

    class LogConsole {
    public:
        enum Level {
            Debug,
            Info,
            Warning,
            Error
        };

        const auto& getLog() { return this->m_consoleLog; }

        using EvaluateError = std::string;

        void log(Level level, std::string_view message) {
            switch (level) {
                default:
                case Level::Debug:   this->m_consoleLog.emplace_back(level, "[-] " + std::string(message)); break;
                case Level::Info:    this->m_consoleLog.emplace_back(level, "[i] " + std::string(message)); break;
                case Level::Warning: this->m_consoleLog.emplace_back(level, "[*] " + std::string(message)); break;
                case Level::Error:   this->m_consoleLog.emplace_back(level, "[!] " + std::string(message)); break;
            }
        }

        [[noreturn]] void abortEvaluation(std::string_view message) {
            throw EvaluateError(message);
        }

        void clear() {
            this->m_consoleLog.clear();
        }

    private:
        std::vector<std::pair<Level, std::string>> m_consoleLog;
    };

}