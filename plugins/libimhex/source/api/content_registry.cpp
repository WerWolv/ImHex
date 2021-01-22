#include <hex/api/content_registry.hpp>

#include <hex/helpers/shared_data.hpp>

#include <filesystem>
#include <fstream>

namespace hex {

    /* Settings */

    void ContentRegistry::Settings::load() {
        std::ifstream settingsFile(std::filesystem::path((SharedData::mainArgv)[0]).parent_path() / "settings.json");

        if (settingsFile.good())
            settingsFile >> getSettingsData();
    }

    void ContentRegistry::Settings::store() {
        std::ofstream settingsFile(std::filesystem::path((SharedData::mainArgv)[0]).parent_path() / "settings.json", std::ios::trunc);
        settingsFile << getSettingsData();
    }

    void ContentRegistry::Settings::add(std::string_view category, std::string_view name, s64 defaultValue, const std::function<bool(nlohmann::json&)> &callback) {
        ContentRegistry::Settings::getEntries()[category.data()].emplace_back(Entry{ name.data(), callback });

        auto &json = getSettingsData();

        if (!json.contains(category.data()))
            json[category.data()] = nlohmann::json::object();
        if (!json[category.data()].contains(name.data()))
            json[category.data()][name.data()] = defaultValue;
    }

    void ContentRegistry::Settings::add(std::string_view category, std::string_view name, std::string_view defaultValue, const std::function<bool(nlohmann::json&)> &callback) {
        ContentRegistry::Settings::getEntries()[category.data()].emplace_back(Entry{ name.data(), callback });

        getSettingsData()[category.data()] = nlohmann::json::object();
        getSettingsData()[category.data()][name.data()] = defaultValue;
    }

    std::map<std::string, std::vector<ContentRegistry::Settings::Entry>>& ContentRegistry::Settings::getEntries() {
        return SharedData::settingsEntries;
    }

    nlohmann::json& ContentRegistry::Settings::getSettingsData() {
        return SharedData::settingsJson;
    }


    /* Events */

    auto ContentRegistry::Events::get(std::string_view name) {
        auto &customEvents = SharedData::customEvents;
        auto &lastId = SharedData::customEventsLastId;

        if (!customEvents.contains(name.data())) {
            customEvents[name.data()] = static_cast<hex::Events>(lastId);
            lastId++;
        }

        return customEvents[name.data()];
    }


    /* Command Palette Commands */

    void ContentRegistry::CommandPaletteCommands::add(ContentRegistry::CommandPaletteCommands::Type type, std::string_view command, std::string_view description, const std::function<std::string(std::string)> &callback) {
        getEntries().push_back(ContentRegistry::CommandPaletteCommands::Entry{ type, command.data(), description.data(), callback });
    }

    std::vector<ContentRegistry::CommandPaletteCommands::Entry>& ContentRegistry::CommandPaletteCommands::getEntries() {
        return SharedData::commandPaletteCommands;
    }


    /* Pattern Language Functions */

    void ContentRegistry::PatternLanguageFunctions::add(std::string_view name, u32 parameterCount, const std::function<hex::lang::ASTNode*(hex::lang::Evaluator&, std::vector<hex::lang::ASTNode*>)> &func) {
        getEntries()[name.data()] = Function{ parameterCount, func };
    }

    std::map<std::string, ContentRegistry::PatternLanguageFunctions::Function>& ContentRegistry::PatternLanguageFunctions::getEntries() {
        return SharedData::patternLanguageFunctions;
    }


    /* Views */

    View* ContentRegistry::Views::add(View *view) {
        auto &views = getEntries();

        views.push_back(view);

        return views.back();
    }

    std::vector<View*>& ContentRegistry::Views::getEntries() {
        return SharedData::views;
    }


    /* Tools */

    void ContentRegistry::Tools::add(std::string_view name, const std::function<void()> &function) {
        getEntries().emplace_back(Entry{ name.data(), function });
    }

    std::vector<ContentRegistry::Tools::Entry>& ContentRegistry::Tools::getEntries() {
        return SharedData::toolsEntries;
    }


    /* Data Inspector */

    void ContentRegistry::DataInspector::add(std::string_view name, size_t requiredSize, ContentRegistry::DataInspector::GeneratorFunction function) {
        getEntries().push_back(Entry{ name.data(), requiredSize, function });
    }

    std::vector<ContentRegistry::DataInspector::Entry>& ContentRegistry::DataInspector::getEntries() {
        return SharedData::dataInspectorEntries;
    }
}