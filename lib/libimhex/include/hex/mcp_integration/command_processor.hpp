#pragma once

#include <string>
#include <functional>
#include <map>
#include <nlohmann/json.hpp>

namespace hex {
    namespace mcp {

        using json = nlohmann::json;
        using CommandHandler = std::function<json(const json&)>;

        class CommandProcessor {
        public:
            CommandProcessor();
            ~CommandProcessor();

            void registerCommandHandler(const std::string& methodName, CommandHandler handler);
            std::string processCommand(const std::string& commandJson);

        private:
            json handleError(const std::string& message, int id = -1, int code = -1000);

            // Command handler methods
            json handleSearch(const json& params);
            json handleGetOffsetInfo(const json& params);
            json handleReadBytes(const json& params);
            json handleWriteBytes(const json& params);
            json handleGetSelection(const json& params);
            json handleSetSelection(const json& params);

            std::map<std::string, CommandHandler> m_commandHandlers;
        };

    }
}
