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

            using QueryResultCallback = std::function<void(std::string)>;

            struct QueryResult {
                std::string name;
                QueryResultCallback callback;
            };

            using ContentDisplayCallback = std::function<void()>;
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

            struct ContentDisplay {
                bool showSearchBox;
                ContentDisplayCallback callback;
            };

            const std::vector<Entry>& getEntries();
            const std::vector<Handler>& getHandlers();

            std::optional<ContentDisplay>& getDisplayedContent();

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

        /**
         * @brief Specify UI content that will be displayed inside the command palette
         * @param displayCallback Display callback that will be called to display the content
         */
        void setDisplayedContent(const impl::ContentDisplayCallback &displayCallback);

        /**
         * @brief Opens the command palette window, displaying a user defined interface
         * @param displayCallback Display callback that will be called to display the content
         */
        void openWithContent(const impl::ContentDisplayCallback &displayCallback);
    }

}