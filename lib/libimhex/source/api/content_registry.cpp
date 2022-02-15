#include <hex/api/content_registry.hpp>

#include <hex/helpers/paths.hpp>
#include <hex/helpers/logger.hpp>

#include <hex/ui/view.hpp>

#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>

namespace hex {

    namespace ContentRegistry::Settings {

        void load() {
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
                store();
        }

        void store() {
            for (const auto &dir : hex::getPath(ImHexPath::Config)) {
                std::ofstream settingsFile(dir / "settings.json", std::ios::trunc);

                if (settingsFile.good()) {
                    settingsFile << getSettingsData();
                    break;
                }
            }
        }

        void add(const std::string &unlocalizedCategory, const std::string &unlocalizedName, i64 defaultValue, const Callback &callback) {
            log::info("Registered new integer setting: [{}]: {}", unlocalizedCategory, unlocalizedName);

            getEntries()[unlocalizedCategory.c_str()].emplace_back(Entry { unlocalizedName.c_str(), callback });

            auto &json = getSettingsData();

            if (!json.contains(unlocalizedCategory))
                json[unlocalizedCategory] = nlohmann::json::object();
            if (!json[unlocalizedCategory].contains(unlocalizedName) || !json[unlocalizedCategory][unlocalizedName].is_number())
                json[unlocalizedCategory][unlocalizedName] = int(defaultValue);
        }

        void add(const std::string &unlocalizedCategory, const std::string &unlocalizedName, const std::string &defaultValue, const Callback &callback) {
            log::info("Registered new string setting: [{}]: {}", unlocalizedCategory, unlocalizedName);

            getEntries()[unlocalizedCategory].emplace_back(Entry { unlocalizedName, callback });

            auto &json = getSettingsData();

            if (!json.contains(unlocalizedCategory))
                json[unlocalizedCategory] = nlohmann::json::object();
            if (!json[unlocalizedCategory].contains(unlocalizedName) || !json[unlocalizedCategory][unlocalizedName].is_string())
                json[unlocalizedCategory][unlocalizedName] = std::string(defaultValue);
        }

        void write(const std::string &unlocalizedCategory, const std::string &unlocalizedName, i64 value) {
            auto &json = getSettingsData();

            if (!json.contains(unlocalizedCategory))
                json[unlocalizedCategory] = nlohmann::json::object();

            json[unlocalizedCategory][unlocalizedName] = value;
        }

        void write(const std::string &unlocalizedCategory, const std::string &unlocalizedName, const std::string &value) {
            auto &json = getSettingsData();

            if (!json.contains(unlocalizedCategory))
                json[unlocalizedCategory] = nlohmann::json::object();

            json[unlocalizedCategory][unlocalizedName] = value;
        }

        void write(const std::string &unlocalizedCategory, const std::string &unlocalizedName, const std::vector<std::string> &value) {
            auto &json = getSettingsData();

            if (!json.contains(unlocalizedCategory))
                json[unlocalizedCategory] = nlohmann::json::object();

            json[unlocalizedCategory][unlocalizedName] = value;
        }


        i64 read(const std::string &unlocalizedCategory, const std::string &unlocalizedName, i64 defaultValue) {
            auto &json = getSettingsData();

            if (!json.contains(unlocalizedCategory))
                return defaultValue;
            if (!json[unlocalizedCategory].contains(unlocalizedName))
                return defaultValue;

            if (!json[unlocalizedCategory][unlocalizedName].is_number())
                json[unlocalizedCategory][unlocalizedName] = defaultValue;

            return json[unlocalizedCategory][unlocalizedName].get<i64>();
        }

        std::string read(const std::string &unlocalizedCategory, const std::string &unlocalizedName, const std::string &defaultValue) {
            auto &json = getSettingsData();

            if (!json.contains(unlocalizedCategory))
                return defaultValue;
            if (!json[unlocalizedCategory].contains(unlocalizedName))
                return defaultValue;

            if (!json[unlocalizedCategory][unlocalizedName].is_string())
                json[unlocalizedCategory][unlocalizedName] = defaultValue;

            return json[unlocalizedCategory][unlocalizedName].get<std::string>();
        }

        std::vector<std::string> read(const std::string &unlocalizedCategory, const std::string &unlocalizedName, const std::vector<std::string> &defaultValue) {
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


        std::map<std::string, std::vector<Entry>> &getEntries() {
            static std::map<std::string, std::vector<Entry>> entries;

            return entries;
        }

        nlohmann::json getSetting(const std::string &unlocalizedCategory, const std::string &unlocalizedName) {
            auto &settings = getSettingsData();

            if (!settings.contains(unlocalizedCategory)) return {};
            if (!settings[unlocalizedCategory].contains(unlocalizedName)) return {};

            return settings[unlocalizedCategory][unlocalizedName];
        }

        nlohmann::json &getSettingsData() {
            static nlohmann::json settings;

            return settings;
        }

    }


    namespace ContentRegistry::CommandPaletteCommands {

        void add(Type type, const std::string &command, const std::string &unlocalizedDescription, const DisplayCallback &displayCallback, const ExecuteCallback &executeCallback) {
            log::info("Registered new command palette command: {}", command);

            getEntries().push_back(ContentRegistry::CommandPaletteCommands::Entry { type, command, unlocalizedDescription, displayCallback, executeCallback });
        }

        std::vector<Entry> &getEntries() {
            static std::vector<Entry> commands;

            return commands;
        }

    }


    namespace ContentRegistry::PatternLanguage {

        static std::string getFunctionName(const Namespace &ns, const std::string &name) {
            std::string functionName;
            for (auto &scope : ns)
                functionName += scope + "::";

            functionName += name;

            return functionName;
        }

        void addFunction(const Namespace &ns, const std::string &name, u32 parameterCount, const Callback &func) {
            log::info("Registered new pattern language function: {}", getFunctionName(ns, name));

            getFunctions()[getFunctionName(ns, name)] = Function { parameterCount, func, false };
        }

        void addDangerousFunction(const Namespace &ns, const std::string &name, u32 parameterCount, const Callback &func) {
            log::info("Registered new dangerous pattern language function: {}", getFunctionName(ns, name));

            getFunctions()[getFunctionName(ns, name)] = Function { parameterCount, func, true };
        }

        std::map<std::string, Function> &getFunctions() {
            static std::map<std::string, Function> functions;

            return functions;
        }


        static std::vector<impl::ColorPalette> s_colorPalettes;
        static u32 s_colorIndex;
        static u32 s_selectedColorPalette;

        std::vector<impl::ColorPalette> &getPalettes() {
            return s_colorPalettes;
        }

        void addColorPalette(const std::string &unlocalizedName, const std::vector<u32> &colors) {
            s_colorPalettes.push_back({ unlocalizedName,
                colors });
        }

        void setSelectedPalette(u32 index) {
            if (index < s_colorPalettes.size())
                s_selectedColorPalette = index;

            resetPalette();
        }

        u32 getNextColor() {
            if (s_colorPalettes.empty())
                return 0x00;

            auto &currColors = s_colorPalettes[s_selectedColorPalette].colors;

            u32 color = currColors[s_colorIndex];

            s_colorIndex++;
            s_colorIndex %= currColors.size();

            return color;
        }

        void resetPalette() {
            s_colorIndex = 0;
        }

    }


    namespace ContentRegistry::Views {

        void impl::add(View *view) {
            log::info("Registered new view: {}", view->getUnlocalizedName());

            getEntries().insert({ view->getUnlocalizedName(), view });
        }

        std::map<std::string, View *> &getEntries() {
            static std::map<std::string, View *> views;

            return views;
        }

        View *getViewByName(const std::string &unlocalizedName) {
            auto &views = getEntries();

            if (views.contains(unlocalizedName))
                return views[unlocalizedName];
            else
                return nullptr;
        }

    }

    namespace ContentRegistry::Tools {

        void add(const std::string &unlocalizedName, const impl::Callback &function) {
            log::info("Registered new tool: {}", unlocalizedName);

            getEntries().emplace_back(impl::Entry { unlocalizedName, function });
        }

        std::vector<impl::Entry> &getEntries() {
            static std::vector<impl::Entry> entries;

            return entries;
        }

    }

    namespace ContentRegistry::DataInspector {

        void add(const std::string &unlocalizedName, size_t requiredSize, impl::GeneratorFunction displayGeneratorFunction, std::optional<impl::EditingFunction> editingFunction) {
            log::info("Registered new data inspector format: {}", unlocalizedName);

            getEntries().push_back({ unlocalizedName, requiredSize, std::move(displayGeneratorFunction), std::move(editingFunction) });
        }

        std::vector<impl::Entry> &getEntries() {
            static std::vector<impl::Entry> entries;

            return entries;
        }

    }

    namespace ContentRegistry::DataProcessorNode {

        void impl::add(const impl::Entry &entry) {
            log::info("Registered new data processor node type: [{}]: ", entry.category, entry.name);

            getEntries().push_back(entry);
        }

        void addSeparator() {
            getEntries().push_back({ "", "", [] { return nullptr; } });
        }

        std::vector<impl::Entry> &getEntries() {
            static std::vector<impl::Entry> nodes;

            return nodes;
        }

    }

    namespace ContentRegistry::Language {

        void registerLanguage(const std::string &name, const std::string &languageCode) {
            log::info("Registered new language: {} ({})", name, languageCode);

            getLanguages().insert({ languageCode, name });
        }

        void addLocalizations(const std::string &languageCode, const LanguageDefinition &definition) {
            log::info("Registered new localization for language {} with {} entries", languageCode, definition.getEntries().size());

            getLanguageDefinitions()[languageCode].push_back(definition);
        }

        std::map<std::string, std::string> &getLanguages() {
            static std::map<std::string, std::string> languages;

            return languages;
        }

        std::map<std::string, std::vector<LanguageDefinition>> &getLanguageDefinitions() {
            static std::map<std::string, std::vector<LanguageDefinition>> definitions;

            return definitions;
        }

    }

    namespace ContentRegistry::Interface {

        void registerMainMenuItem(const std::string &unlocalizedName, u32 priority) {
            log::info("Registered new main menu item: {}", unlocalizedName);

            getMainMenuItems().insert({ priority, { unlocalizedName } });
        }

        void addMenuItem(const std::string &unlocalizedMainMenuName, u32 priority, const impl::DrawCallback &function) {
            log::info("Added new menu item to menu {} with priority {}", unlocalizedMainMenuName, priority);

            getMenuItems().insert({
                priority, {unlocalizedMainMenuName, function}
            });
        }

        void addWelcomeScreenEntry(const impl::DrawCallback &function) {
            getWelcomeScreenEntries().push_back(function);
        }

        void addFooterItem(const impl::DrawCallback &function) {
            getFooterItems().push_back(function);
        }

        void addToolbarItem(const impl::DrawCallback &function) {
            getToolbarItems().push_back(function);
        }

        void addSidebarItem(const std::string &icon, const impl::DrawCallback &function) {
            getSidebarItems().push_back({ icon, function });
        }

        void addTitleBarButton(const std::string &icon, const std::string &unlocalizedTooltip, const impl::ClickCallback &function) {
            getTitleBarButtons().push_back({ icon, unlocalizedTooltip, function });
        }

        void addLayout(const std::string &unlocalizedName, const impl::LayoutFunction &function) {
            log::info("Added new layout: {}", unlocalizedName);

            getLayouts().push_back({ unlocalizedName, function });
        }


        std::multimap<u32, impl::MainMenuItem> &getMainMenuItems() {
            static std::multimap<u32, impl::MainMenuItem> items;

            return items;
        }
        std::multimap<u32, impl::MenuItem> &getMenuItems() {
            static std::multimap<u32, impl::MenuItem> items;

            return items;
        }

        std::vector<impl::DrawCallback> &getWelcomeScreenEntries() {
            static std::vector<impl::DrawCallback> entries;

            return entries;
        }
        std::vector<impl::DrawCallback> &getFooterItems() {
            static std::vector<impl::DrawCallback> items;

            return items;
        }
        std::vector<impl::DrawCallback> &getToolbarItems() {
            static std::vector<impl::DrawCallback> items;

            return items;
        }
        std::vector<impl::SidebarItem> &getSidebarItems() {
            static std::vector<impl::SidebarItem> items;

            return items;
        }
        std::vector<impl::TitleBarButton> &getTitleBarButtons() {
            static std::vector<impl::TitleBarButton> buttons;

            return buttons;
        }

        std::vector<impl::Layout> &getLayouts() {
            static std::vector<impl::Layout> layouts;

            return layouts;
        }

    }

    namespace ContentRegistry::Provider {

        void impl::addProviderName(const std::string &unlocalizedName) {
            log::info("Registered new provider: {}", unlocalizedName);

            getEntries().push_back(unlocalizedName);
        }

        std::vector<std::string> &getEntries() {
            static std::vector<std::string> providerNames;

            return providerNames;
        }

    }

    namespace ContentRegistry::DataFormatter {

        void add(const std::string &unlocalizedName, const impl::Callback &callback) {
            log::info("Registered new data formatter: {}", unlocalizedName);

            getEntries().push_back({ unlocalizedName, callback });
        }

        std::vector<impl::Entry> &getEntries() {
            static std::vector<impl::Entry> entries;

            return entries;
        }

    }

    namespace ContentRegistry::FileHandler {

        void add(const std::vector<std::string> &extensions, const impl::Callback &callback) {
            for (const auto &extension : extensions)
                log::info("Registered new data handler for extensions: {}", extension);

            getEntries().push_back({ extensions, callback });
        }

        std::vector<impl::Entry> &getEntries() {
            static std::vector<impl::Entry> entries;

            return entries;
        }

    }

}