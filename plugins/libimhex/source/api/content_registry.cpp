#include <hex/api/content_registry.hpp>

#include <hex/helpers/shared_data.hpp>
#include <hex/helpers/paths.hpp>

#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>

namespace hex {

    /* Settings */

    void ContentRegistry::Settings::load() {
        bool loaded = false;
        for (const auto &dir : hex::getPath(ImHexPath::Config)) {
            std::ifstream settingsFile(dir + "/settings.json");

            if (settingsFile.good()) {
                settingsFile >> getSettingsData();
                loaded = true;
                break;
            }
        }

        if (!loaded)
            ContentRegistry::Settings::store();
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

    void ContentRegistry::Settings::add(const std::string &unlocalizedCategory, const std::string &unlocalizedName, s64 defaultValue, const ContentRegistry::Settings::Callback &callback) {
        ContentRegistry::Settings::getEntries()[unlocalizedCategory.c_str()].emplace_back(Entry{ unlocalizedName.c_str(), callback });

        auto &json = getSettingsData();

        if (!json.contains(unlocalizedCategory))
            json[unlocalizedCategory] = nlohmann::json::object();
        if (!json[unlocalizedCategory].contains(unlocalizedName) || !json[unlocalizedCategory][unlocalizedName].is_number())
            json[unlocalizedCategory][unlocalizedName] = int(defaultValue);
    }

    void ContentRegistry::Settings::add(const std::string &unlocalizedCategory, const std::string &unlocalizedName, const std::string &defaultValue, const ContentRegistry::Settings::Callback &callback) {
        ContentRegistry::Settings::getEntries()[unlocalizedCategory].emplace_back(Entry{ unlocalizedName, callback });

        auto &json = getSettingsData();

        if (!json.contains(unlocalizedCategory))
            json[unlocalizedCategory] = nlohmann::json::object();
        if (!json[unlocalizedCategory].contains(unlocalizedName) || !json[unlocalizedCategory][unlocalizedName].is_string())
            json[unlocalizedCategory][unlocalizedName] = std::string(defaultValue);
    }

    void ContentRegistry::Settings::write(const std::string &unlocalizedCategory, const std::string &unlocalizedName, s64 value) {
        auto &json = getSettingsData();

        if (!json.contains(unlocalizedCategory))
            json[unlocalizedCategory] = nlohmann::json::object();

        json[unlocalizedCategory][unlocalizedName] = value;
    }

    void ContentRegistry::Settings::write(const std::string &unlocalizedCategory, const std::string &unlocalizedName, const std::string &value) {
        auto &json = getSettingsData();

        if (!json.contains(unlocalizedCategory))
            json[unlocalizedCategory] = nlohmann::json::object();

        json[unlocalizedCategory][unlocalizedName] = value;
    }

    void ContentRegistry::Settings::write(const std::string &unlocalizedCategory, const std::string &unlocalizedName, const std::vector<std::string>& value) {
        auto &json = getSettingsData();

        if (!json.contains(unlocalizedCategory))
            json[unlocalizedCategory] = nlohmann::json::object();

        json[unlocalizedCategory][unlocalizedName] = value;
    }


    s64 ContentRegistry::Settings::read(const std::string &unlocalizedCategory, const std::string &unlocalizedName, s64 defaultValue) {
        auto &json = getSettingsData();

        if (!json.contains(unlocalizedCategory))
            return defaultValue;
        if (!json[unlocalizedCategory].contains(unlocalizedName))
            return defaultValue;

        if (!json[unlocalizedCategory][unlocalizedName].is_number())
            json[unlocalizedCategory][unlocalizedName] = defaultValue;

        return json[unlocalizedCategory][unlocalizedName].get<s64>();
    }

    std::string ContentRegistry::Settings::read(const std::string &unlocalizedCategory, const std::string &unlocalizedName, const std::string &defaultValue) {
        auto &json = getSettingsData();

        if (!json.contains(unlocalizedCategory))
            return defaultValue;
        if (!json[unlocalizedCategory].contains(unlocalizedName))
            return defaultValue;

        if (!json[unlocalizedCategory][unlocalizedName].is_string())
            json[unlocalizedCategory][unlocalizedName] = defaultValue;

        return json[unlocalizedCategory][unlocalizedName].get<std::string>();
    }

    std::vector<std::string> ContentRegistry::Settings::read(const std::string &unlocalizedCategory, const std::string &unlocalizedName, const std::vector<std::string>& defaultValue) {
        auto &json = getSettingsData();

        if (!json.contains(unlocalizedCategory))
            return defaultValue;
        if (!json[unlocalizedCategory].contains(unlocalizedName))
            return defaultValue;

        if (!json[unlocalizedCategory][unlocalizedName].is_array())
            json[unlocalizedCategory][unlocalizedName] = defaultValue;

        if (!json[unlocalizedCategory][unlocalizedName].array().empty() && !json[unlocalizedCategory][unlocalizedName][0].is_string())
            json[unlocalizedCategory][unlocalizedName] = defaultValue;

        return json[unlocalizedCategory][unlocalizedName].get<std::vector<std::string>>();
    }


    std::map<std::string, std::vector<ContentRegistry::Settings::Entry>>& ContentRegistry::Settings::getEntries() {
        return SharedData::settingsEntries;
    }

    nlohmann::json ContentRegistry::Settings::getSetting(const std::string &unlocalizedCategory, const std::string &unlocalizedName) {
        auto &settings = getSettingsData();

        if (!settings.contains(unlocalizedCategory)) return { };
        if (!settings[unlocalizedCategory].contains(unlocalizedName)) return { };

        return settings[unlocalizedCategory][unlocalizedName];
    }

    nlohmann::json& ContentRegistry::Settings::getSettingsData() {
        return SharedData::settingsJson;
    }


    /* Command Palette Commands */

    void ContentRegistry::CommandPaletteCommands::add(ContentRegistry::CommandPaletteCommands::Type type, const std::string &command, const std::string &unlocalizedDescription, const std::function<std::string(std::string)> &displayCallback, const std::function<void(std::string)> &executeCallback) {
        getEntries().push_back(ContentRegistry::CommandPaletteCommands::Entry{ type, command, unlocalizedDescription, displayCallback, executeCallback });
    }

    std::vector<ContentRegistry::CommandPaletteCommands::Entry>& ContentRegistry::CommandPaletteCommands::getEntries() {
        return SharedData::commandPaletteCommands;
    }


    /* Pattern Language Functions */


    void ContentRegistry::PatternLanguageFunctions::add(const Namespace &ns, const std::string &name, u32 parameterCount, const ContentRegistry::PatternLanguageFunctions::Callback &func, bool dangerous) {
        std::string functionName;
        for (auto &scope : ns)
            functionName += scope + "::";

        functionName += name;

        getEntries()[functionName] = Function { parameterCount, func, dangerous };
    }

    std::map<std::string, ContentRegistry::PatternLanguageFunctions::Function>& ContentRegistry::PatternLanguageFunctions::getEntries() {
        return SharedData::patternLanguageFunctions;
    }


    /* Views */

    void ContentRegistry::Views::impl::add(View *view) {
        getEntries().emplace_back(view);
    }

    std::vector<View*>& ContentRegistry::Views::getEntries() {
        return SharedData::views;
    }


    /* Tools */

    void ContentRegistry::Tools:: add(const std::string &unlocalizedName, const std::function<void()> &function) {
        getEntries().emplace_back(impl::Entry{ unlocalizedName, function });
    }

    std::vector<ContentRegistry::Tools::impl::Entry>& ContentRegistry::Tools::getEntries() {
        return SharedData::toolsEntries;
    }


    /* Data Inspector */

    void ContentRegistry::DataInspector::add(const std::string &unlocalizedName, size_t requiredSize, ContentRegistry::DataInspector::impl::GeneratorFunction function) {
        getEntries().push_back({ unlocalizedName, requiredSize, std::move(function) });
    }

    std::vector<ContentRegistry::DataInspector::impl::Entry>& ContentRegistry::DataInspector::getEntries() {
        return SharedData::dataInspectorEntries;
    }

    /* Data Processor Nodes */

    void ContentRegistry::DataProcessorNode::impl::add(const impl::Entry &entry) {
        getEntries().push_back(entry);
    }

    void ContentRegistry::DataProcessorNode::addSeparator() {
        getEntries().push_back({ "", "", []{ return nullptr; } });
    }

    std::vector<ContentRegistry::DataProcessorNode::impl::Entry>& ContentRegistry::DataProcessorNode::getEntries() {
        return SharedData::dataProcessorNodes;
    }

    /* Languages */

    void ContentRegistry::Language::registerLanguage(const std::string &name, const std::string &languageCode) {
        getLanguages().insert({ languageCode, name });
    }

    void ContentRegistry::Language::addLocalizations(const std::string &languageCode, const LanguageDefinition &definition) {
        getLanguageDefinitions()[languageCode].push_back(definition);
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

    void ContentRegistry::Interface::addToolbarItem(const ContentRegistry::Interface::DrawCallback &function){
        getToolbarItems().push_back(function);
    }


    std::vector<ContentRegistry::Interface::DrawCallback>& ContentRegistry::Interface::getWelcomeScreenEntries() {
        return SharedData::welcomeScreenEntries;
    }
    std::vector<ContentRegistry::Interface::DrawCallback>& ContentRegistry::Interface::getFooterItems() {
        return SharedData::footerItems;
    }
    std::vector<ContentRegistry::Interface::DrawCallback>& ContentRegistry::Interface::getToolbarItems() {
        return SharedData::toolbarItems;
    }


    /* Providers */

    void ContentRegistry::Provider::impl::addProviderName(const std::string &unlocalizedName) {
        SharedData::providerNames.push_back(unlocalizedName);
    }

    const std::vector<std::string> &ContentRegistry::Provider::getEntries() {
        return SharedData::providerNames;
    }
}