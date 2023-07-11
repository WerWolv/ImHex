#include <hex/api/content_registry.hpp>

#include <hex/helpers/fs.hpp>
#include <hex/helpers/logger.hpp>

#include <hex/ui/view.hpp>
#include <hex/data_processor/node.hpp>

#include <filesystem>
#include <thread>
#include <ranges>

#include <nlohmann/json.hpp>

#include <wolv/io/file.hpp>

namespace hex {

    namespace ContentRegistry::Settings {

        constexpr auto SettingsFile = "settings.json";

        namespace impl {

            std::map<Category, std::vector<Entry>> &getEntries() {
                static std::map<Category, std::vector<Entry>> entries;

                return entries;
            }

            std::map<std::string, std::string> &getCategoryDescriptions() {
                static std::map<std::string, std::string> descriptions;

                return descriptions;
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

            void load() {
                bool loaded = false;
                for (const auto &dir : fs::getDefaultPaths(fs::ImHexPath::Config)) {
                    wolv::io::File file(dir / SettingsFile, wolv::io::File::Mode::Read);

                    if (file.isValid()) {
                        getSettingsData() = nlohmann::json::parse(file.readString());
                        loaded = true;
                        break;
                    }
                }

                if (!loaded)
                    store();
            }

            void store() {
                for (const auto &dir : fs::getDefaultPaths(fs::ImHexPath::Config)) {
                    wolv::io::File file(dir / SettingsFile, wolv::io::File::Mode::Create);

                    if (file.isValid()) {
                        file.writeString(getSettingsData().dump(4));
                        break;
                    }
                }
            }

            void clear() {
                for (const auto &dir : fs::getDefaultPaths(fs::ImHexPath::Config)) {
                    wolv::io::fs::remove(dir / SettingsFile);
                }
            }

            static auto getCategoryEntry(const std::string &unlocalizedCategory) {
                auto &entries        = getEntries();
                const size_t curSlot = entries.size();
                auto found           = entries.find(Category { unlocalizedCategory });

                if (found == entries.end()) {
                    auto [iter, _] = entries.emplace(Category { unlocalizedCategory, curSlot }, std::vector<Entry> {});
                    return iter;
                }

                return found;
            }

        }

        void add(const std::string &unlocalizedCategory, const std::string &unlocalizedName, i64 defaultValue, const impl::Callback &callback, bool requiresRestart) {
            log::debug("Registered new integer setting: [{}]: {}", unlocalizedCategory, unlocalizedName);

            impl::getCategoryEntry(unlocalizedCategory)->second.emplace_back(impl::Entry { unlocalizedName, requiresRestart, callback });

            auto &json = impl::getSettingsData();

            if (!json.contains(unlocalizedCategory))
                json[unlocalizedCategory] = nlohmann::json::object();
            if (!json[unlocalizedCategory].contains(unlocalizedName) || !json[unlocalizedCategory][unlocalizedName].is_number())
                json[unlocalizedCategory][unlocalizedName] = int(defaultValue);
        }

        void add(const std::string &unlocalizedCategory, const std::string &unlocalizedName, const std::string &defaultValue, const impl::Callback &callback, bool requiresRestart) {
            log::debug("Registered new string setting: [{}]: {}", unlocalizedCategory, unlocalizedName);

            impl::getCategoryEntry(unlocalizedCategory)->second.emplace_back(impl::Entry { unlocalizedName, requiresRestart, callback });

            auto &json = impl::getSettingsData();

            if (!json.contains(unlocalizedCategory))
                json[unlocalizedCategory] = nlohmann::json::object();
            if (!json[unlocalizedCategory].contains(unlocalizedName) || !json[unlocalizedCategory][unlocalizedName].is_string())
                json[unlocalizedCategory][unlocalizedName] = std::string(defaultValue);
        }

        void add(const std::string &unlocalizedCategory, const std::string &unlocalizedName, const std::vector<std::string> &defaultValue, const impl::Callback &callback, bool requiresRestart) {
            log::debug("Registered new string array setting: [{}]: {}", unlocalizedCategory, unlocalizedName);

            impl::getCategoryEntry(unlocalizedCategory)->second.emplace_back(impl::Entry { unlocalizedName, requiresRestart, callback });

            auto &json = impl::getSettingsData();

            if (!json.contains(unlocalizedCategory))
                json[unlocalizedCategory] = nlohmann::json::object();
            if (!json[unlocalizedCategory].contains(unlocalizedName) || !json[unlocalizedCategory][unlocalizedName].is_array())
                json[unlocalizedCategory][unlocalizedName] = defaultValue;
        }

        void addCategoryDescription(const std::string &unlocalizedCategory, const std::string &unlocalizedCategoryDescription) {
            impl::getCategoryDescriptions()[unlocalizedCategory] = unlocalizedCategoryDescription;
        }

        void write(const std::string &unlocalizedCategory, const std::string &unlocalizedName, i64 value) {
            auto &json = impl::getSettingsData();

            if (!json.contains(unlocalizedCategory))
                json[unlocalizedCategory] = nlohmann::json::object();

            json[unlocalizedCategory][unlocalizedName] = value;
        }

        void write(const std::string &unlocalizedCategory, const std::string &unlocalizedName, const std::string &value) {
            auto &json = impl::getSettingsData();

            if (!json.contains(unlocalizedCategory))
                json[unlocalizedCategory] = nlohmann::json::object();

            json[unlocalizedCategory][unlocalizedName] = value;
        }

        void write(const std::string &unlocalizedCategory, const std::string &unlocalizedName, const std::vector<std::string> &value) {
            auto &json = impl::getSettingsData();

            if (!json.contains(unlocalizedCategory))
                json[unlocalizedCategory] = nlohmann::json::object();

            json[unlocalizedCategory][unlocalizedName] = value;
        }


        i64 read(const std::string &unlocalizedCategory, const std::string &unlocalizedName, i64 defaultValue) {
            auto &json = impl::getSettingsData();

            if (!json.contains(unlocalizedCategory))
                return defaultValue;
            if (!json[unlocalizedCategory].contains(unlocalizedName))
                return defaultValue;

            if (!json[unlocalizedCategory][unlocalizedName].is_number())
                json[unlocalizedCategory][unlocalizedName] = defaultValue;

            return json[unlocalizedCategory][unlocalizedName].get<i64>();
        }

        std::string read(const std::string &unlocalizedCategory, const std::string &unlocalizedName, const std::string &defaultValue) {
            auto &json = impl::getSettingsData();

            if (!json.contains(unlocalizedCategory))
                return defaultValue;
            if (!json[unlocalizedCategory].contains(unlocalizedName))
                return defaultValue;

            if (!json[unlocalizedCategory][unlocalizedName].is_string())
                json[unlocalizedCategory][unlocalizedName] = defaultValue;

            return json[unlocalizedCategory][unlocalizedName].get<std::string>();
        }

        std::vector<std::string> read(const std::string &unlocalizedCategory, const std::string &unlocalizedName, const std::vector<std::string> &defaultValue) {
            auto &json = impl::getSettingsData();

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

    }


    namespace ContentRegistry::CommandPaletteCommands {

        void add(Type type, const std::string &command, const std::string &unlocalizedDescription, const impl::DisplayCallback &displayCallback, const impl::ExecuteCallback &executeCallback) {
            log::debug("Registered new command palette command: {}", command);

            impl::getEntries().push_back(ContentRegistry::CommandPaletteCommands::impl::Entry { type, command, unlocalizedDescription, displayCallback, executeCallback });
        }

        void addHandler(Type type, const std::string &command, const impl::QueryCallback &queryCallback, const impl::DisplayCallback &displayCallback) {
            log::debug("Registered new command palette command handler: {}", command);

            impl::getHandlers().push_back(ContentRegistry::CommandPaletteCommands::impl::Handler { type, command, queryCallback, displayCallback });
        }

        namespace impl {

            std::vector<Entry> &getEntries() {
                static std::vector<Entry> commands;

                return commands;
            }

            std::vector<Handler> &getHandlers() {
                static std::vector<Handler> commands;

                return commands;
            }

        }

    }


    namespace ContentRegistry::PatternLanguage {

        static std::string getFunctionName(const pl::api::Namespace &ns, const std::string &name) {
            std::string functionName;
            for (auto &scope : ns)
                functionName += scope + "::";

            functionName += name;

            return functionName;
        }

        pl::PatternLanguage& getRuntime() {
            static PerProvider<pl::PatternLanguage> runtime;

            return *runtime;
        }

        std::mutex& getRuntimeLock() {
            static std::mutex runtimeLock;

            return runtimeLock;
        }

        void configureRuntime(pl::PatternLanguage &runtime, prv::Provider *provider) {
            runtime.reset();

            if (provider != nullptr) {
                runtime.setDataSource(provider->getBaseAddress(), provider->getActualSize(),
                                      [provider](u64 offset, u8 *buffer, size_t size) {
                                          provider->read(offset, buffer, size);
                                      },
                                      [provider](u64 offset, const u8 *buffer, size_t size) {
                                          if (provider->isWritable())
                                              provider->write(offset, buffer, size);
                                      }
                );
            }

            runtime.setIncludePaths(fs::getDefaultPaths(fs::ImHexPath::PatternsInclude) | fs::getDefaultPaths(fs::ImHexPath::Patterns));

            for (const auto &func : impl::getFunctions()) {
                if (func.dangerous)
                    runtime.addDangerousFunction(func.ns, func.name, func.parameterCount, func.callback);
                else
                    runtime.addFunction(func.ns, func.name, func.parameterCount, func.callback);
            }

            for (const auto &[name, callback] : impl::getPragmas()) {
                runtime.addPragma(name, callback);
            }

            runtime.addDefine("__IMHEX__");
            runtime.addDefine("__IMHEX_VERSION__", ImHexApi::System::getImHexVersion());
        }

        void addPragma(const std::string &name, const pl::api::PragmaHandler &handler) {
            log::debug("Registered new pattern language pragma: {}", name);

            impl::getPragmas()[name] = handler;
        }

        void addFunction(const pl::api::Namespace &ns, const std::string &name, pl::api::FunctionParameterCount parameterCount, const pl::api::FunctionCallback &func) {
            log::debug("Registered new pattern language function: {}", getFunctionName(ns, name));

            impl::getFunctions().push_back({
                ns, name,
                parameterCount, func,
                false
            });
        }

        void addDangerousFunction(const pl::api::Namespace &ns, const std::string &name, pl::api::FunctionParameterCount parameterCount, const pl::api::FunctionCallback &func) {
            log::debug("Registered new dangerous pattern language function: {}", getFunctionName(ns, name));

            impl::getFunctions().push_back({
                ns, name,
                parameterCount, func,
                true
            });
        }


        void addVisualizer(const std::string &name, const impl::VisualizerFunctionCallback &function, u32 parameterCount) {
            log::debug("Registered new pattern visualizer function: {}", name);
            impl::getVisualizers()[name] = impl::Visualizer { parameterCount, function };
        }

        void addInlineVisualizer(const std::string &name, const impl::VisualizerFunctionCallback &function, u32 parameterCount) {
            log::debug("Registered new inline pattern visualizer function: {}", name);
            impl::getInlineVisualizers()[name] = impl::Visualizer { parameterCount, function };
        }


        namespace impl {

            std::map<std::string, impl::Visualizer> &getVisualizers() {
                static std::map<std::string, impl::Visualizer> visualizers;

                return visualizers;
            }

            std::map<std::string, impl::Visualizer> &getInlineVisualizers() {
                static std::map<std::string, impl::Visualizer> visualizers;

                return visualizers;
            }

            std::map<std::string, pl::api::PragmaHandler> &getPragmas() {
                static std::map<std::string, pl::api::PragmaHandler> pragmas;

                return pragmas;
            }

            std::vector<impl::FunctionDefinition> &getFunctions() {
                static std::vector<impl::FunctionDefinition> functions;

                return functions;
            }

        }


    }


    namespace ContentRegistry::Views {

        namespace impl {

            std::map<std::string, std::unique_ptr<View>> &getEntries() {
                static std::map<std::string, std::unique_ptr<View>> views;

                return views;
            }

        }

        void impl::add(std::unique_ptr<View> &&view) {
            log::debug("Registered new view: {}", view->getUnlocalizedName());

            impl::getEntries().insert({ view->getUnlocalizedName(), std::move(view) });
        }

        View* getViewByName(const std::string &unlocalizedName) {
            auto &views = impl::getEntries();

            if (views.contains(unlocalizedName))
                return views[unlocalizedName].get();
            else
                return nullptr;
        }

    }

    namespace ContentRegistry::Tools {

        void add(const std::string &unlocalizedName, const impl::Callback &function) {
            log::debug("Registered new tool: {}", unlocalizedName);

            impl::getEntries().emplace_back(impl::Entry { unlocalizedName, function, false });
        }

        namespace impl {

            std::vector<Entry> &getEntries() {
                static std::vector<Entry> tools;

                return tools;
            }

        }

    }

    namespace ContentRegistry::DataInspector {

        void add(const std::string &unlocalizedName, size_t requiredSize, impl::GeneratorFunction displayGeneratorFunction, std::optional<impl::EditingFunction> editingFunction) {
            log::debug("Registered new data inspector format: {}", unlocalizedName);

            impl::getEntries().push_back({ unlocalizedName, requiredSize, requiredSize, std::move(displayGeneratorFunction), std::move(editingFunction) });
        }

        void add(const std::string &unlocalizedName, size_t requiredSize, size_t maxSize, impl::GeneratorFunction displayGeneratorFunction, std::optional<impl::EditingFunction> editingFunction) {
            log::debug("Registered new data inspector format: {}", unlocalizedName);

            impl::getEntries().push_back({ unlocalizedName, requiredSize, maxSize, std::move(displayGeneratorFunction), std::move(editingFunction) });
        }

        namespace impl {

            std::vector<impl::Entry> &getEntries() {
                static std::vector<impl::Entry> entries;

                return entries;
            }

        }


    }

    namespace ContentRegistry::DataProcessorNode {

        void impl::add(const impl::Entry &entry) {
            log::debug("Registered new data processor node type: [{}]: {}", entry.category, entry.name);

            getEntries().push_back(entry);
        }

        void addSeparator() {
            impl::getEntries().push_back({ "", "", [] { return nullptr; } });
        }

        namespace impl {

            std::vector<impl::Entry> &getEntries() {
                static std::vector<impl::Entry> nodes;

                return nodes;
            }

        }

    }

    namespace ContentRegistry::Language {

        void addLocalization(const nlohmann::json &data) {
            if (!data.is_object())
                return;

            if (!data.contains("code") || !data.contains("country") || !data.contains("language") || !data.contains("translations")) {
                log::error("Localization data is missing required fields!");
                return;
            }

            const auto &code            = data["code"];
            const auto &country         = data["country"];
            const auto &language        = data["language"];
            const auto &translations    = data["translations"];

            if (!code.is_string() || !country.is_string() || !language.is_string() || !translations.is_object()) {
                log::error("Localization data has invalid fields!");
                return;
            }

            if (data.contains("fallback")) {
                const auto &fallback = data["fallback"];

                if (fallback.is_boolean() && fallback.get<bool>())
                    LangEntry::setFallbackLanguage(code.get<std::string>());
            }

            impl::getLanguages().insert({ code.get<std::string>(), hex::format("{} ({})", language.get<std::string>(), country.get<std::string>()) });

            std::map<std::string, std::string> translationDefinitions;
            for (auto &[key, value] : translations.items()) {
                if (!value.is_string()) {
                    log::error("Localization data has invalid fields!");
                    continue;
                }

                translationDefinitions[key] = value.get<std::string>();
            }

            impl::getLanguageDefinitions()[code.get<std::string>()].emplace_back(std::move(translationDefinitions));
        }

        namespace impl {

            std::map<std::string, std::string> &getLanguages() {
                static std::map<std::string, std::string> languages;

                return languages;
            }

            std::map<std::string, std::vector<LanguageDefinition>> &getLanguageDefinitions() {
                static std::map<std::string, std::vector<LanguageDefinition>> definitions;

                return definitions;
            }

        }


    }

    namespace ContentRegistry::Interface {

        void registerMainMenuItem(const std::string &unlocalizedName, u32 priority) {
            log::debug("Registered new main menu item: {}", unlocalizedName);

            impl::getMainMenuItems().insert({ priority, { unlocalizedName } });
        }

        void addMenuItem(const std::vector<std::string> &unlocalizedMainMenuNames, u32 priority, const Shortcut &shortcut, const impl::MenuCallback &function, const impl::EnabledCallback& enabledCallback, View *view) {
            log::debug("Added new menu item to menu {} with priority {}", wolv::util::combineStrings(unlocalizedMainMenuNames, " -> "), priority);

            impl::getMenuItems().insert({
                priority, { unlocalizedMainMenuNames, shortcut, function, enabledCallback }
            });

            if (shortcut.isLocal() && view != nullptr)
                ShortcutManager::addShortcut(view, shortcut, function);
            else
                ShortcutManager::addGlobalShortcut(shortcut, function);
        }

        void addMenuItemSubMenu(std::vector<std::string> unlocalizedMainMenuNames, u32 priority, const impl::MenuCallback &function, const impl::EnabledCallback& enabledCallback) {
            log::debug("Added new menu item sub menu to menu {} with priority {}", wolv::util::combineStrings(unlocalizedMainMenuNames, " -> "), priority);

            unlocalizedMainMenuNames.emplace_back(impl::SubMenuValue);
            impl::getMenuItems().insert({
                priority, { unlocalizedMainMenuNames, {}, function, enabledCallback }
            });
        }

        void addMenuItemSeparator(std::vector<std::string> unlocalizedMainMenuNames, u32 priority) {
            unlocalizedMainMenuNames.emplace_back(impl::SeparatorValue);
            impl::getMenuItems().insert({
                priority, { unlocalizedMainMenuNames, {}, []{}, []{ return true; } }
            });
        }

        void addWelcomeScreenEntry(const impl::DrawCallback &function) {
            impl::getWelcomeScreenEntries().push_back(function);
        }

        void addFooterItem(const impl::DrawCallback &function) {
            impl::getFooterItems().push_back(function);
        }

        void addToolbarItem(const impl::DrawCallback &function) {
            impl::getToolbarItems().push_back(function);
        }

        void addSidebarItem(const std::string &icon, const impl::DrawCallback &function) {
            impl::getSidebarItems().push_back({ icon, function });
        }

        void addTitleBarButton(const std::string &icon, const std::string &unlocalizedTooltip, const impl::ClickCallback &function) {
            impl::getTitleBarButtons().push_back({ icon, unlocalizedTooltip, function });
        }

        namespace impl {

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

        }

    }

    namespace ContentRegistry::Provider {

        void impl::addProviderName(const std::string &unlocalizedName) {
            log::debug("Registered new provider: {}", unlocalizedName);

            getEntries().push_back(unlocalizedName);
        }


        namespace impl {

            std::vector<std::string> &getEntries() {
                static std::vector<std::string> providerNames;

                return providerNames;
            }

        }


    }

    namespace ContentRegistry::DataFormatter {

        void add(const std::string &unlocalizedName, const impl::Callback &callback) {
            log::debug("Registered new data formatter: {}", unlocalizedName);

            impl::getEntries().push_back({ unlocalizedName, callback });
        }

        namespace impl {

            std::vector<impl::Entry> &getEntries() {
                static std::vector<impl::Entry> entries;

                return entries;
            }

        }

    }

    namespace ContentRegistry::FileHandler {

        void add(const std::vector<std::string> &extensions, const impl::Callback &callback) {
            for (const auto &extension : extensions)
                log::debug("Registered new data handler for extensions: {}", extension);

            impl::getEntries().push_back({ extensions, callback });
        }

        namespace impl {

            std::vector<impl::Entry> &getEntries() {
                static std::vector<impl::Entry> entries;

                return entries;
            }

        }

    }

    namespace ContentRegistry::HexEditor {

        const int DataVisualizer::TextInputFlags = ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_NoHorizontalScroll;

        bool DataVisualizer::drawDefaultScalarEditingTextBox(u64 address, const char *format, ImGuiDataType dataType, u8 *data, ImGuiInputTextFlags flags) const {
            struct UserData {
                u8 *data;
                i32 maxChars;

                bool editingDone;
            };

            UserData userData = {
                .data = data,
                .maxChars = this->getMaxCharsPerCell(),

                .editingDone = false
            };

            ImGui::PushID(reinterpret_cast<void*>(address));
            ImGui::InputScalarCallback("##editing_input", dataType, data, format, flags | TextInputFlags | ImGuiInputTextFlags_CallbackEdit, [](ImGuiInputTextCallbackData *data) -> int {
                auto &userData = *reinterpret_cast<UserData*>(data->UserData);

                if (data->BufTextLen >= userData.maxChars)
                    userData.editingDone = true;

                return 0;
            }, &userData);
            ImGui::PopID();

            return userData.editingDone || ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_Escape);
        }

        bool DataVisualizer::drawDefaultTextEditingTextBox(u64 address, std::string &data, ImGuiInputTextFlags flags) const {
            struct UserData {
                std::string *data;
                i32 maxChars;

                bool editingDone;
            };

            UserData userData = {
                    .data = &data,
                    .maxChars = this->getMaxCharsPerCell(),

                    .editingDone = false
            };

            ImGui::PushID(reinterpret_cast<void*>(address));
            ImGui::InputText("##editing_input", data.data(), data.size() + 1, flags | TextInputFlags | ImGuiInputTextFlags_CallbackEdit, [](ImGuiInputTextCallbackData *data) -> int {
                auto &userData = *reinterpret_cast<UserData*>(data->UserData);

                userData.data->resize(data->BufSize);

                if (data->BufTextLen >= userData.maxChars)
                    userData.editingDone = true;

                return 0;
            }, &userData);
            ImGui::PopID();

            return userData.editingDone || ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_Escape);
        }

        namespace impl {

            void addDataVisualizer(std::shared_ptr<DataVisualizer> &&visualizer) {
                getVisualizers().emplace_back(std::move(visualizer));

            }

            std::vector<std::shared_ptr<DataVisualizer>> &getVisualizers() {
                static std::vector<std::shared_ptr<DataVisualizer>> visualizers;

                return visualizers;
            }

        }

        std::shared_ptr<DataVisualizer> getVisualizerByName(const std::string &unlocalizedName) {
            for (const auto &visualizer : impl::getVisualizers()) {
                if (visualizer->getUnlocalizedName() == unlocalizedName)
                    return visualizer;
            }

            return nullptr;
        }


    }

    namespace ContentRegistry::Hashes {

        namespace impl {

            std::vector<std::unique_ptr<Hash>> &getHashes() {
                static std::vector<std::unique_ptr<Hash>> hashes;

                return hashes;
            }

            void add(std::unique_ptr<Hash> &&hash) {
                getHashes().emplace_back(std::move(hash));
            }

        }

    }

    namespace ContentRegistry::BackgroundServices {

        namespace impl {

            std::vector<Service> &getServices() {
                static std::vector<Service> services;

                return services;
            }

            void stopServices() {
                for (auto &service : getServices()) {
                    service.thread.request_stop();
                }

                for (auto &service : getServices()) {
                    service.thread.detach();
                }
            }

        }

        void registerService(const std::string &unlocalizedName, const impl::Callback &callback) {
            log::debug("Registered new background service: {}", unlocalizedName);

            impl::getServices().push_back(impl::Service {
                unlocalizedName,
                std::jthread([callback](const std::stop_token &stopToken){
                    while (!stopToken.stop_requested()) {
                        callback();
                        std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    }
                })
            });
        }

    }

    namespace ContentRegistry::CommunicationInterface {

        namespace impl {

            std::map<std::string, NetworkCallback> &getNetworkEndpoints() {
                static std::map<std::string, NetworkCallback> endpoints;

                return endpoints;
            }

        }

        void registerNetworkEndpoint(const std::string &endpoint, const impl::NetworkCallback &callback) {
            log::debug("Registered new network endpoint: {}", endpoint);

            impl::getNetworkEndpoints().insert({ endpoint, callback });
        }

    }

}
