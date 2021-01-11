#include <helpers/content_registry.hpp>

#include <helpers/shared_data.hpp>

#include <filesystem>
#include <fstream>

namespace hex {

    /* Settings */

    void ContentRegistry::Settings::load() {
        std::ifstream settingsFile(std::filesystem::path(mainArgv[0]).parent_path() / "settings.json");

        if (settingsFile.good())
            settingsFile >> getSettingsData();
    }

    void ContentRegistry::Settings::store() {
        std::ofstream settingsFile(std::filesystem::path(mainArgv[0]).parent_path() / "settings.json", std::ios::trunc);
        settingsFile << getSettingsData();
    }

    void ContentRegistry::Settings::add(std::string_view category, std::string_view name, s64 defaultValue, const std::function<bool(nlohmann::json&)> &callback) {
        ContentRegistry::Settings::getEntries()[category.data()].emplace_back(Entry{ name.data(), callback });

        auto &json = (*SharedData::get().settingsJson);

        if (!json.contains(category.data()))
            json[category.data()] = nlohmann::json::object();
        if (!json[category.data()].contains(name.data()))
            json[category.data()][name.data()] = defaultValue;
    }

    void ContentRegistry::Settings::add(std::string_view category, std::string_view name, std::string_view defaultValue, const std::function<bool(nlohmann::json&)> &callback) {
        ContentRegistry::Settings::getEntries()[category.data()].emplace_back(Entry{ name.data(), callback });

        (*SharedData::get().settingsJson)[category.data()] = nlohmann::json::object();
        (*SharedData::get().settingsJson)[category.data()][name.data()] = defaultValue;
    }

    std::map<std::string, std::vector<ContentRegistry::Settings::Entry>>& ContentRegistry::Settings::getEntries() {
        return *SharedData::get().settingsEntries;
    }

    nlohmann::json& ContentRegistry::Settings::getSettingsData() {
        return *SharedData::get().settingsJson;
    }


    /* Events */

    auto ContentRegistry::Events::get(std::string_view name) {
        auto &customEvents = *SharedData::get().customEvents;
        auto &lastId = *SharedData::get().customEventsLastId;

        if (!customEvents.contains(name.data())) {
            customEvents[name.data()] = static_cast<hex::Events>(lastId);
            lastId++;
        }

        return customEvents[name.data()];
    }


    /* Command Palette Commands */

    void ContentRegistry::CommandPaletteCommands::add(ContentRegistry::CommandPaletteCommands::Type type, std::string_view command, std::string_view description, const std::function<std::string(std::string)> &callback) {
        SharedData::get().commandPaletteCommands->push_back(ContentRegistry::CommandPaletteCommands::Entry{ type, command.data(), description.data(), callback });
    }

    std::vector<ContentRegistry::CommandPaletteCommands::Entry> ContentRegistry::CommandPaletteCommands::getEntries() {
        return *SharedData::get().commandPaletteCommands;
    }

}