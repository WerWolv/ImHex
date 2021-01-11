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

        struct Events {
            Events() = delete;

            static auto get(std::string_view name);
        };

        struct CommandPaletteCommands {
            CommandPaletteCommands() = delete;

            enum class Type : u32 {
                SymbolCommand,
                KeywordCommand
            };

            struct Entry {
                Type type;
                std::string command;
                std::string description;
                std::function<std::string(std::string)> callback;
            };

            static void add(Type type, std::string_view command, std::string_view description, const std::function<std::string(std::string)> &callback);
            static std::vector<Entry> getEntries();
        };
    };

}