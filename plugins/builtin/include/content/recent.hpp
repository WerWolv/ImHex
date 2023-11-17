#pragma once

#include <string>

#include <nlohmann/json.hpp>

#include <wolv/io/fs.hpp>

namespace hex::plugin::builtin::recent {
    
    /**
     * @brief Structure used to represent a recent other
     */
    struct RecentEntry {
        
        /**
         * @brief Name that should be used to display the entry to the user
         */
        std::string displayName;

        /**
         * @brief type of this entry. Might be a provider id (e.g. hex.builtin.provider.file)
         * or "project" in case of a project
         */
        std::string type;

        /**
         * @brief path of this entry file
         */
        std::fs::path entryFilePath;

        /**
         * @brief Entire json data of the recent entry (include the fields above)
         * Used for custom settings set by the providers
         */
        nlohmann::json data;

        bool operator==(const RecentEntry &other) const {
            return HashFunction()(*this) == HashFunction()(other);
        }

        std::size_t getHash() const {
            return HashFunction()(*this);
        }

        struct HashFunction {
            std::size_t operator()(const RecentEntry& provider) const {
                return
                    (std::hash<std::string>()(provider.displayName)) ^
                    (std::hash<std::string>()(provider.type) << 1);
            }
        };

    };

    void registerEventHandlers();

    /**
     * @brief Scan the files in ImHexPath::Recent to get the recent entries, and delete duplicates.
     */
    void updateRecentEntries();
    
    /**
     * @brief Load a recent entry in ImHex. The entry might be a provider of a project
     * @param recentEntry entry to load
     */
    void loadRecentEntry(const RecentEntry &recentEntry);

    /**
     * @brief Draw the recent providers in the welcome screen
     */
    void draw();

    /**
     * @brief Adds the "open recent" item in the "File" menu
     */
    void addMenuItems();

}