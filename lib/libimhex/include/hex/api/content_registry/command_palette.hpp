#pragma once

#include <hex.hpp>

#include <hex/api/localization_manager.hpp>

#include <string>
#include <functional>
#include <optional>
#include <vector>

EXPORT_MODULE namespace hex {

    /* Command Palette Command Registry. Allows adding of new commands to the command palette */
    namespace ContentRegistry::CommandPalette {

        enum class Type : u32 {
            SymbolCommand,
            KeywordCommand
        };

        namespace impl {

            struct QueryResult {
                std::string name;
                std::function<void(std::string)> callback;
            };

            using DisplayCallback = std::function<std::string(std::string)>;
            using ExecuteCallback = std::function<std::optional<std::string>(std::string)>;
            using QueryCallback   = std::function<std::vector<QueryResult>(std::string)>;

            struct Entry {
                Type type;
                std::string command;
                UnlocalizedString unlocalizedDescription;
                DisplayCallback displayCallback;
                ExecuteCallback executeCallback;
            };

            struct Handler {
                Type type;
                std::string command;
                QueryCallback queryCallback;
                DisplayCallback displayCallback;
            };

            const std::vector<Entry>& getEntries();
            const std::vector<Handler>& getHandlers();

        }

        /**
         * @brief Adds a new command to the command palette
         * @param type The type of the command
         * @param command The command to add
         * @param unlocalizedDescription The description of the command
         * @param displayCallback The callback that will be called when the command is displayed in the command palette
         * @param executeCallback The callback that will be called when the command is executed
         */
        void add(
            Type type,
            const std::string &command,
            const UnlocalizedString &unlocalizedDescription,
            const impl::DisplayCallback &displayCallback,
            const impl::ExecuteCallback &executeCallback = [](auto) { return std::nullopt; });

        /**
         * @brief Adds a new command handler to the command palette
         * @param type The type of the command
         * @param command The command to add
         * @param queryCallback The callback that will be called when the command palette wants to load the name and callback items
         * @param displayCallback The callback that will be called when the command is displayed in the command palette
         */
        void addHandler(
            Type type,
            const std::string &command,
            const impl::QueryCallback &queryCallback,
            const impl::DisplayCallback &displayCallback);
    }

}