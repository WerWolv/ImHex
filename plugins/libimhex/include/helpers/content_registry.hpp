#pragma once

#include <hex.hpp>

#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

namespace hex {

    class ContentRegistry {
    public:
        ContentRegistry() = delete;

        struct Settings {
            Settings() = delete;

            struct Entry {
                std::string name;
                std::function<bool(nlohmann::json&)> callback;
            };

            static void load();
            static void store();

            static void add(std::string_view category, std::string_view name, s64 defaultValue, const std::function<bool(nlohmann::json&)> &callback);
            static void add(std::string_view category, std::string_view name, std::string_view defaultValue, const std::function<bool(nlohmann::json&)> &callback);

            static std::map<std::string, std::vector<Entry>>& getEntries();
            static nlohmann::json& getSettingsData();
        };
    };

}