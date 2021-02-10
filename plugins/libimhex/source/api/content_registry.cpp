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

    void ContentRegistry::Settings::write(std::string_view category, std::string_view name, s64 value) {
        auto &json = getSettingsData();

        if (!json.contains(category.data()))
            json[category.data()] = nlohmann::json::object();

        json[category.data()][name.data()] = value;
    }

    void ContentRegistry::Settings::write(std::string_view category, std::string_view name, std::string_view value) {
        auto &json = getSettingsData();

        if (!json.contains(category.data()))
            json[category.data()] = nlohmann::json::object();

        json[category.data()][name.data()] = value;
    }

    void ContentRegistry::Settings::write(std::string_view category, std::string_view name, const std::vector<std::string>& value) {
        auto &json = getSettingsData();

        if (!json.contains(category.data()))
            json[category.data()] = nlohmann::json::object();

        json[category.data()][name.data()] = value;
    }


    s64 ContentRegistry::Settings::read(std::string_view category, std::string_view name, s64 defaultValue) {
        auto &json = getSettingsData();

        if (!json.contains(category.data()))
            return defaultValue;
        if (!json[category.data()].contains(name.data()))
            return defaultValue;

        return json[category.data()][name.data()].get<s64>();
    }

    std::string ContentRegistry::Settings::read(std::string_view category, std::string_view name, std::string_view defaultValue) {
        auto &json = getSettingsData();

        if (!json.contains(category.data()))
            return defaultValue.data();
        if (!json[category.data()].contains(name.data()))
            return defaultValue.data();

        return json[category.data()][name.data()].get<std::string>();
    }

    std::vector<std::string> ContentRegistry::Settings::read(std::string_view category, std::string_view name, const std::vector<std::string>& defaultValue) {
        auto &json = getSettingsData();

        if (!json.contains(category.data()))
            return defaultValue;
        if (!json[category.data()].contains(name.data()))
            return defaultValue;

        return json[category.data()][name.data()].get<std::vector<std::string>>();
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

    void ContentRegistry::CommandPaletteCommands::add(ContentRegistry::CommandPaletteCommands::Type type, std::string_view command, std::string_view description, const std::function<std::string(std::string)> &displayCallback, const std::function<void(std::string)> &executeCallback) {
        getEntries().push_back(ContentRegistry::CommandPaletteCommands::Entry{ type, command.data(), description.data(), displayCallback, executeCallback });
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

    void ContentRegistry::Views::add(std::unique_ptr<View> &&view) {
        getEntries().emplace_back(std::move(view));
    }

    std::vector<std::unique_ptr<View>>& ContentRegistry::Views::getEntries() {
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
        getEntries().push_back({ name.data(), requiredSize, std::move(function) });
    }

    std::vector<ContentRegistry::DataInspector::Entry>& ContentRegistry::DataInspector::getEntries() {
        return SharedData::dataInspectorEntries;
    }

    /* Data Processor Nodes */

    void ContentRegistry::DataProcessorNode::add(const Entry &entry) {
        getEntries().push_back(entry);
    }

    void ContentRegistry::DataProcessorNode::addSeparator() {
        getEntries().push_back({ "", "", []{ return nullptr; } });
    }

    std::vector<ContentRegistry::DataProcessorNode::Entry>& ContentRegistry::DataProcessorNode::getEntries() {
        return SharedData::dataProcessorNodes;
    }

    /* Languages */

    void ContentRegistry::Language::registerLanguage(std::string_view name, std::string_view languageCode) {
        getLanguages().insert({ languageCode.data(), name.data() });
    }

    void ContentRegistry::Language::addLocalizations(std::string_view languageCode, const LanguageDefinition &definition) {
        getLanguageDefinitions()[languageCode.data()].push_back(definition);
    }

    std::map<std::string, std::string>& ContentRegistry::Language::getLanguages() {
        return SharedData::languageNames;
    }

    std::map<std::string, std::vector<LanguageDefinition>>& ContentRegistry::Language::getLanguageDefinitions() {
        return SharedData::languageDefinitions;
    }
}