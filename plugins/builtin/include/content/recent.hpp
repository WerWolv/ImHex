#pragma once

#include<string>
#include<atomic>
#include<list>

#include <nlohmann/json.hpp>

#include <wolv/io/fs.hpp>

namespace hex::plugin::builtin::recent {
    struct RecentEntry {
        std::string displayName;
        std::string type;
        std::fs::path filePath;

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
    void updateRecentEntries();
    void loadRecentEntry(const RecentEntry &recentEntry);

    void draw();
    void drawFileMenuItem();

}