#include <hex/api/content_registry.hpp>

#include <hex/helpers/paths.hpp>
#include <hex/helpers/logger.hpp>

#include <hex/ui/view.hpp>

#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>

namespace hex {

    /* Settings */

    void ContentRegistry::Settings::load() {
        bool loaded = false;
        for (const auto &dir : hex::getPath(ImHexPath::Config)) {
            std::ifstream settingsFile(dir / "settings.json");

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
            std::ofstream settingsFile(dir / "settings.json", std::ios::trunc);

            if (settingsFile.good()) {
                settingsFile << getSettingsData();
                break;
            }
        }
    }

    void ContentRegistry::Settings::add(const std::string &unlocalizedCategory, const std::string &unlocalizedName, i64 defaultValue, const ContentRegistry::Settings::Callback &callback) {
        log::info("Registered new integer setting: [{}]: {}", unlocalizedCategory, unlocalizedName);

        ContentRegistry::Settings::getEntries()[unlocalizedCategory.c_str()].emplace_back(Entry { unlocalizedName.c_str(), callback });

        auto &json = getSettingsData();

        if (!json.contains(unlocalizedCategory))
            json[unlocalizedCategory] = nlohmann::json::object();
        if (!json[unlocalizedCategory].contains(unlocalizedName) || !json[unlocalizedCategory][unlocalizedName].is_number())
            json[unlocalizedCategory][unlocalizedName] = int(defaultValue);
    }

    void ContentRegistry::Settings::add(const std::string &unlocalizedCategory, const std::string &unlocalizedName, const std::string &defaultValue, const ContentRegistry::Settings::Callback &callback) {
        log::info("Registered new string setting: [{}]: {}", unlocalizedCategory, unlocalizedName);

        ContentRegistry::Settings::getEntries()[unlocalizedCategory].emplace_back(Entry { unlocalizedName, callback });

        auto &json = getSettingsData();

        if (!json.contains(unlocalizedCategory))
            json[unlocalizedCategory] = nlohmann::json::object();
        if (!json[unlocalizedCategory].contains(unlocalizedName) || !json[unlocalizedCategory][unlocalizedName].is_string())
            json[unlocalizedCategory][unlocalizedName] = std::string(defaultValue);
    }

    void ContentRegistry::Settings::write(const std::string &unlocalizedCategory, const std::string &unlocalizedName, i64 value) {
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

    void ContentRegistry::Settings::write(const std::string &unlocalizedCategory, const std::string &unlocalizedName, const std::vector<std::string> &value) {
        auto &json = getSettingsData();

        if (!json.contains(unlocalizedCategory))
            json[unlocalizedCategory] = nlohmann::json::object();

        json[unlocalizedCategory][unlocalizedName] = value;
    }


    i64 ContentRegistry::Settings::read(const std::string &unlocalizedCategory, const std::string &unlocalizedName, i64 defaultValue) {
        auto &json = getSettingsData();

        if (!json.contains(unlocalizedCategory))
            return defaultValue;
        if (!json[unlocalizedCategory].contains(unlocalizedName))
            return defaultValue;

        if (!json[unlocalizedCategory][unlocalizedName].is_number())
            json[unlocalizedCategory][unlocalizedName] = defaultValue;

        return json[unlocalizedCategory][unlocalizedName].get<i64>();
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

    std::vector<std::string> ContentRegistry::Settings::read(const std::string &unlocalizedCategory, const std::string &unlocalizedName, const std::vector<std::string> &defaultValue) {
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


    std::map<std::string, std::vector<ContentRegistry::Settings::Entry>> &ContentRegistry::Settings::getEntries() {
        static std::map<std::string, std::vector<ContentRegistry::Settings::Entry>> entries;

        return entries;
    }

    nlohmann::json ContentRegistry::Settings::getSetting(const std::string &unlocalizedCategory, const std::string &unlocalizedName) {
        auto &settings = getSettingsData();

        if (!settings.contains(unlocalizedCategory)) return {};
        if (!settings[unlocalizedCategory].contains(unlocalizedName)) return {};

        return settings[unlocalizedCategory][unlocalizedName];
    }

    nlohmann::json &ContentRegistry::Settings::getSettingsData() {
        static nlohmann::json settings;

        return settings;
    }


    /* Command Palette Commands */

    void ContentRegistry::CommandPaletteCommands::add(ContentRegistry::CommandPaletteCommands::Type type, const std::string &command, const std::string &unlocalizedDescription, const std::function<std::string(std::string)> &displayCallback, const std::function<void(std::string)> &executeCallback) {
        log::info("Registered new command palette command: {}", command);

        getEntries().push_back(ContentRegistry::CommandPaletteCommands::Entry { type, command, unlocalizedDescription, displayCallback, executeCallback });
    }

    std::vector<ContentRegistry::CommandPaletteCommands::Entry> &ContentRegistry::CommandPaletteCommands::getEntries() {
        static std::vector<ContentRegistry::CommandPaletteCommands::Entry> commands;

        return commands;
    }


    /* Pattern Language Functions */


    static std::string getFunctionName(const ContentRegistry::PatternLanguage::Namespace &ns, const std::string &name) {
        std::string functionName;
        for (auto &scope : ns)
            functionName += scope + "::";

        functionName += name;

        return functionName;
    }

    void ContentRegistry::PatternLanguage::addFunction(const Namespace &ns, const std::string &name, u32 parameterCount, const ContentRegistry::PatternLanguage::Callback &func) {
        log::info("Registered new pattern language function: {}", getFunctionName(ns, name));

        getFunctions()[getFunctionName(ns, name)] = Function { parameterCount, func, false };
    }

    void ContentRegistry::PatternLanguage::addDangerousFunction(const Namespace &ns, const std::string &name, u32 parameterCount, const ContentRegistry::PatternLanguage::Callback &func) {
        log::info("Registered new dangerous pattern language function: {}", getFunctionName(ns, name));

        getFunctions()[getFunctionName(ns, name)] = Function { parameterCount, func, true };
    }

    std::map<std::string, ContentRegistry::PatternLanguage::Function> &ContentRegistry::PatternLanguage::getFunctions() {
        static std::map<std::string, ContentRegistry::PatternLanguage::Function> functions;

        return functions;
    }


    static std::vector<ContentRegistry::PatternLanguage::impl::ColorPalette> s_colorPalettes;
    static u32 s_colorIndex;
    static u32 s_selectedColorPalette;

    std::vector<ContentRegistry::PatternLanguage::impl::ColorPalette> &ContentRegistry::PatternLanguage::getPalettes() {
        return s_colorPalettes;
    }

    void ContentRegistry::PatternLanguage::addColorPalette(const std::string &unlocalizedName, const std::vector<u32> &colors) {
        s_colorPalettes.push_back({ unlocalizedName,
            colors });
    }

    void ContentRegistry::PatternLanguage::setSelectedPalette(u32 index) {
        if (index < s_colorPalettes.size())
            s_selectedColorPalette = index;

        resetPalette();
    }

    u32 ContentRegistry::PatternLanguage::getNextColor() {
        if (s_colorPalettes.empty())
            return 0x00;

        auto &currColors = s_colorPalettes[s_selectedColorPalette].colors;

        u32 color = currColors[s_colorIndex];

        s_colorIndex++;
        s_colorIndex %= currColors.size();

        return color;
    }

    void ContentRegistry::PatternLanguage::resetPalette() {
        s_colorIndex = 0;
    }


    /* Views */

    void ContentRegistry::Views::impl::add(View *view) {
        log::info("Registered new view: {}", view->getUnlocalizedName());

        getEntries().insert({ view->getUnlocalizedName(), view });
    }

    std::map<std::string, View *> &ContentRegistry::Views::getEntries() {
        static std::map<std::string, View *> views;

        return views;
    }

    View *ContentRegistry::Views::getViewByName(const std::string &unlocalizedName) {
        auto &views = getEntries();

        if (views.contains(unlocalizedName))
            return views[unlocalizedName];
        else
            return nullptr;
    }


    /* Tools */

    void ContentRegistry::Tools::add(const std::string &unlocalizedName, const std::function<void()> &function) {
        log::info("Registered new tool: {}", unlocalizedName);

        getEntries().emplace_back(impl::Entry { unlocalizedName, function });
    }

    std::vector<ContentRegistry::Tools::impl::Entry> &ContentRegistry::Tools::getEntries() {
        static std::vector<ContentRegistry::Tools::impl::Entry> entries;

        return entries;
    }


    /* Data Inspector */

    void ContentRegistry::DataInspector::add(const std::string &unlocalizedName, size_t requiredSize, ContentRegistry::DataInspector::impl::GeneratorFunction function) {
        log::info("Registered new data inspector format: {}", unlocalizedName);

        getEntries().push_back({ unlocalizedName, requiredSize, std::move(function) });
    }

    std::vector<ContentRegistry::DataInspector::impl::Entry> &ContentRegistry::DataInspector::getEntries() {
        static std::vector<ContentRegistry::DataInspector::impl::Entry> entries;

        return entries;
    }

    /* Data Processor Nodes */

    void ContentRegistry::DataProcessorNode::impl::add(const impl::Entry &entry) {
        log::info("Registered new data processor node type: [{}]: ", entry.category, entry.name);

        getEntries().push_back(entry);
    }

    void ContentRegistry::DataProcessorNode::addSeparator() {
        getEntries().push_back({ "", "", [] { return nullptr; } });
    }

    std::vector<ContentRegistry::DataProcessorNode::impl::Entry> &ContentRegistry::DataProcessorNode::getEntries() {
        static std::vector<ContentRegistry::DataProcessorNode::impl::Entry> nodes;

        return nodes;
    }

    /* Languages */

    void ContentRegistry::Language::registerLanguage(const std::string &name, const std::string &languageCode) {
        log::info("Registered new language: {} ({})", name, languageCode);

        getLanguages().insert({ languageCode, name });
    }

    void ContentRegistry::Language::addLocalizations(const std::string &languageCode, const LanguageDefinition &definition) {
        log::info("Registered new localization for language {} with {} entries", languageCode, definition.getEntries().size());

        getLanguageDefinitions()[languageCode].push_back(definition);
    }

    std::map<std::string, std::string> &ContentRegistry::Language::getLanguages() {
        static std::map<std::string, std::string> languages;

        return languages;
    }

    std::map<std::string, std::vector<LanguageDefinition>> &ContentRegistry::Language::getLanguageDefinitions() {
        static std::map<std::string, std::vector<LanguageDefinition>> definitions;

        return definitions;
    }


    /* Interface */

    void ContentRegistry::Interface::registerMainMenuItem(const std::string &unlocalizedName, u32 priority) {
        log::info("Registered new main menu item: {}", unlocalizedName);

        getMainMenuItems().insert({ priority, { unlocalizedName } });
    }

    void ContentRegistry::Interface::addMenuItem(const std::string &unlocalizedMainMenuName, u32 priority, const impl::DrawCallback &function) {
        log::info("Added new menu item to menu {} with priority {}", unlocalizedMainMenuName, priority);

        getMenuItems().insert({
            priority, {unlocalizedMainMenuName, function}
        });
    }

    void ContentRegistry::Interface::addWelcomeScreenEntry(const ContentRegistry::Interface::impl::DrawCallback &function) {
        getWelcomeScreenEntries().push_back(function);
    }

    void ContentRegistry::Interface::addFooterItem(const ContentRegistry::Interface::impl::DrawCallback &function) {
        getFooterItems().push_back(function);
    }

    void ContentRegistry::Interface::addToolbarItem(const ContentRegistry::Interface::impl::DrawCallback &function) {
        getToolbarItems().push_back(function);
    }

    void ContentRegistry::Interface::addSidebarItem(const std::string &icon, const impl::DrawCallback &function) {
        getSidebarItems().push_back({ icon, function });
    }

    void ContentRegistry::Interface::addLayout(const std::string &unlocalizedName, const impl::LayoutFunction &function) {
        log::info("Added new layout: {}", unlocalizedName);

        getLayouts().push_back({ unlocalizedName, function });
    }


    std::multimap<u32, ContentRegistry::Interface::impl::MainMenuItem> &ContentRegistry::Interface::getMainMenuItems() {
        static std::multimap<u32, ContentRegistry::Interface::impl::MainMenuItem> items;

        return items;
    }
    std::multimap<u32, ContentRegistry::Interface::impl::MenuItem> &ContentRegistry::Interface::getMenuItems() {
        static std::multimap<u32, ContentRegistry::Interface::impl::MenuItem> items;

        return items;
    }

    std::vector<ContentRegistry::Interface::impl::DrawCallback> &ContentRegistry::Interface::getWelcomeScreenEntries() {
        static std::vector<ContentRegistry::Interface::impl::DrawCallback> entries;

        return entries;
    }
    std::vector<ContentRegistry::Interface::impl::DrawCallback> &ContentRegistry::Interface::getFooterItems() {
        static std::vector<ContentRegistry::Interface::impl::DrawCallback> items;

        return items;
    }
    std::vector<ContentRegistry::Interface::impl::DrawCallback> &ContentRegistry::Interface::getToolbarItems() {
        static std::vector<ContentRegistry::Interface::impl::DrawCallback> items;

        return items;
    }
    std::vector<ContentRegistry::Interface::impl::SidebarItem> &ContentRegistry::Interface::getSidebarItems() {
        static std::vector<ContentRegistry::Interface::impl::SidebarItem> items;

        return items;
    }

    std::vector<ContentRegistry::Interface::impl::Layout> &ContentRegistry::Interface::getLayouts() {
        static std::vector<ContentRegistry::Interface::impl::Layout> layouts;

        return layouts;
    }


    /* Providers */

    void ContentRegistry::Provider::impl::addProviderName(const std::string &unlocalizedName) {
        log::info("Registered new provider: {}", unlocalizedName);

        getEntries().push_back(unlocalizedName);
    }

    std::vector<std::string> &ContentRegistry::Provider::getEntries() {
        static std::vector<std::string> providerNames;

        return providerNames;
    }


    /* Data Formatters */

    void ContentRegistry::DataFormatter::add(const std::string &unlocalizedName, const impl::Callback &callback) {
        log::info("Registered new data formatter: {}", unlocalizedName);

        getEntries().push_back({ unlocalizedName, callback });
    }

    std::vector<ContentRegistry::DataFormatter::impl::Entry> &ContentRegistry::DataFormatter::getEntries() {
        static std::vector<ContentRegistry::DataFormatter::impl::Entry> entries;

        return entries;
    }


    /* File Handlers */

    void ContentRegistry::FileHandler::add(const std::vector<std::string> &extensions, const impl::Callback &callback) {
        for (const auto &extension : extensions)
            log::info("Registered new data handler for extensions: {}", extension);

        getEntries().push_back({ extensions, callback });
    }

    std::vector<ContentRegistry::FileHandler::impl::Entry> &ContentRegistry::FileHandler::getEntries() {
        static std::vector<ContentRegistry::FileHandler::impl::Entry> entries;

        return entries;
    }
}