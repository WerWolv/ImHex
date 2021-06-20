#include <hex/api/content_registry.hpp>

#include <hex/helpers/shared_data.hpp>

#include <filesystem>
#include <fstream>

namespace hex {

    /* Settings */

    void ContentRegistry::Settings::load() {
        for (const auto &dir : hex::getPath(ImHexPath::Config)) {
            std::ifstream settingsFile(dir + "/settings.json");

            if (settingsFile.good()) {
                settingsFile >> getSettingsData();
                break;
            }
        }
    }

    void ContentRegistry::Settings::store() {
        for (const auto &dir : hex::getPath(ImHexPath::Config)) {
            std::ofstream settingsFile(dir + "/settings.json", std::ios::trunc);

            if (settingsFile.good()) {
                settingsFile << getSettingsData();
                break;
            }
        }
    }

    void ContentRegistry::Settings::add(std::string_view unlocalizedCategory, std::string_view unlocalizedName, s64 defaultValue, const std::function<bool(std::string_view, nlohmann::json&)> &callback) {
        ContentRegistry::Settings::getEntries()[unlocalizedCategory.data()].emplace_back(Entry{ unlocalizedName.data(), callback });

        auto &json = getSettingsData();

        if (!json.contains(unlocalizedCategory.data()))
            json[unlocalizedCategory.data()] = nlohmann::json::object();
        if (!json[unlocalizedCategory.data()].contains(unlocalizedName.data()) || !json[unlocalizedCategory.data()][unlocalizedName.data()].is_number())
            json[unlocalizedCategory.data()][unlocalizedName.data()] = int(defaultValue);
    }

    void ContentRegistry::Settings::add(std::string_view unlocalizedCategory, std::string_view unlocalizedName, std::string_view defaultValue, const std::function<bool(std::string_view, nlohmann::json&)> &callback) {
        ContentRegistry::Settings::getEntries()[unlocalizedCategory.data()].emplace_back(Entry{ unlocalizedName.data(), callback });

        auto &json = getSettingsData();

        if (!json.contains(unlocalizedCategory.data()))
            json[unlocalizedCategory.data()] = nlohmann::json::object();
        if (!json[unlocalizedCategory.data()].contains(unlocalizedName.data()) || !json[unlocalizedCategory.data()][unlocalizedName.data()].is_string())
            json[unlocalizedCategory.data()][unlocalizedName.data()] = std::string(defaultValue);
    }

    void ContentRegistry::Settings::write(std::string_view unlocalizedCategory, std::string_view unlocalizedName, s64 value) {
        auto &json = getSettingsData();

        if (!json.contains(unlocalizedCategory.data()))
            json[unlocalizedCategory.data()] = nlohmann::json::object();

        json[unlocalizedCategory.data()][unlocalizedName.data()] = value;
    }

    void ContentRegistry::Settings::write(std::string_view unlocalizedCategory, std::string_view unlocalizedName, std::string_view value) {
        auto &json = getSettingsData();

        if (!json.contains(unlocalizedCategory.data()))
            json[unlocalizedCategory.data()] = nlohmann::json::object();

        json[unlocalizedCategory.data()][unlocalizedName.data()] = value;
    }

    void ContentRegistry::Settings::write(std::string_view unlocalizedCategory, std::string_view unlocalizedName, const std::vector<std::string>& value) {
        auto &json = getSettingsData();

        if (!json.contains(unlocalizedCategory.data()))
            json[unlocalizedCategory.data()] = nlohmann::json::object();

        json[unlocalizedCategory.data()][unlocalizedName.data()] = value;
    }


    s64 ContentRegistry::Settings::read(std::string_view unlocalizedCategory, std::string_view unlocalizedName, s64 defaultValue) {
        auto &json = getSettingsData();

        if (!json.contains(unlocalizedCategory.data()))
            return defaultValue;
        if (!json[unlocalizedCategory.data()].contains(unlocalizedName.data()))
            return defaultValue;

        return json[unlocalizedCategory.data()][unlocalizedName.data()].get<s64>();
    }

    std::string ContentRegistry::Settings::read(std::string_view unlocalizedCategory, std::string_view unlocalizedName, std::string_view defaultValue) {
        auto &json = getSettingsData();

        if (!json.contains(unlocalizedCategory.data()))
            return defaultValue.data();
        if (!json[unlocalizedCategory.data()].contains(unlocalizedName.data()))
            return defaultValue.data();

        return json[unlocalizedCategory.data()][unlocalizedName.data()].get<std::string>();
    }

    std::vector<std::string> ContentRegistry::Settings::read(std::string_view unlocalizedCategory, std::string_view unlocalizedName, const std::vector<std::string>& defaultValue) {
        auto &json = getSettingsData();

        if (!json.contains(unlocalizedCategory.data()))
            return defaultValue;
        if (!json[unlocalizedCategory.data()].contains(unlocalizedName.data()))
            return defaultValue;

        return json[unlocalizedCategory.data()][unlocalizedName.data()].get<std::vector<std::string>>();
    }


    std::map<std::string, std::vector<ContentRegistry::Settings::Entry>>& ContentRegistry::Settings::getEntries() {
        return SharedData::settingsEntries;
    }

    nlohmann::json ContentRegistry::Settings::getSetting(std::string_view unlocalizedCategory, std::string_view unlocalizedName) {
        auto &settings = getSettingsData();

        if (!settings.contains(unlocalizedCategory)) return { };
        if (!settings[unlocalizedCategory.data()].contains(unlocalizedName)) return { };

        return settings[unlocalizedCategory.data()][unlocalizedName.data()];
    }

    nlohmann::json& ContentRegistry::Settings::getSettingsData() {
        return SharedData::settingsJson;
    }


    /* Command Palette Commands */

    void ContentRegistry::CommandPaletteCommands::add(ContentRegistry::CommandPaletteCommands::Type type, std::string_view command, std::string_view unlocalizedDescription, const std::function<std::string(std::string)> &displayCallback, const std::function<void(std::string)> &executeCallback) {
        getEntries().push_back(ContentRegistry::CommandPaletteCommands::Entry{ type, command.data(), unlocalizedDescription.data(), displayCallback, executeCallback });
    }

    std::vector<ContentRegistry::CommandPaletteCommands::Entry>& ContentRegistry::CommandPaletteCommands::getEntries() {
        return SharedData::commandPaletteCommands;
    }


    /* Pattern Language Functions */

    void ContentRegistry::PatternLanguageFunctions::add(std::string_view name, u32 parameterCount, const std::function<hex::lang::ASTNode*(hex::lang::Evaluator&, std::vector<hex::lang::ASTNode*>&)> &func) {
        getEntries()[name.data()] = Function{ parameterCount, func };
    }

    std::map<std::string, ContentRegistry::PatternLanguageFunctions::Function>& ContentRegistry::PatternLanguageFunctions::getEntries() {
        return SharedData::patternLanguageFunctions;
    }


    /* Views */

    void ContentRegistry::Views::add(View *view) {
        getEntries().emplace_back(view);
    }

    std::vector<View*>& ContentRegistry::Views::getEntries() {
        return SharedData::views;
    }


    /* Tools */

    void ContentRegistry::Tools:: add(std::string_view unlocalizedName, const std::function<void()> &function) {
        getEntries().emplace_back(Entry{ unlocalizedName.data(), function });
    }

    std::vector<ContentRegistry::Tools::Entry>& ContentRegistry::Tools::getEntries() {
        return SharedData::toolsEntries;
    }


    /* Data Inspector */

    void ContentRegistry::DataInspector::add(std::string_view unlocalizedName, size_t requiredSize, ContentRegistry::DataInspector::GeneratorFunction function) {
        getEntries().push_back({ unlocalizedName.data(), requiredSize, std::move(function) });
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

        EventManager::post<EventSettingsChanged>();
    }

    std::map<std::string, std::string>& ContentRegistry::Language::getLanguages() {
        return SharedData::languageNames;
    }

    std::map<std::string, std::vector<LanguageDefinition>>& ContentRegistry::Language::getLanguageDefinitions() {
        return SharedData::languageDefinitions;
    }


    void ContentRegistry::Interface::addWelcomeScreenEntry(const ContentRegistry::Interface::DrawCallback &function) {
        getWelcomeScreenEntries().push_back(function);
    }

    void ContentRegistry::Interface::addFooterItem(const ContentRegistry::Interface::DrawCallback &function){
        getFooterItems().push_back(function);
    }


    std::vector<ContentRegistry::Interface::DrawCallback>& ContentRegistry::Interface::getWelcomeScreenEntries() {
        return SharedData::welcomeScreenEntries;
    }
    std::vector<ContentRegistry::Interface::DrawCallback>& ContentRegistry::Interface::getFooterItems() {
        return SharedData::footerItems;
    }
}