#include <hex/api/content_registry.hpp>
#include <hex/api/shortcut_manager.hpp>

#include <hex/helpers/fs.hpp>
#include <hex/helpers/logger.hpp>

#include <hex/ui/view.hpp>
#include <hex/data_processor/node.hpp>

#include <filesystem>
#include <jthread.hpp>

#if defined(OS_WEB)
#include <emscripten.h>
#endif

#include <nlohmann/json.hpp>

#include <wolv/io/file.hpp>
#include <wolv/utils/string.hpp>

namespace hex {

    namespace ContentRegistry::Settings {

        [[maybe_unused]] constexpr auto SettingsFile = "settings.json";

        namespace impl {

            nlohmann::json& getSetting(const UnlocalizedString &unlocalizedCategory, const UnlocalizedString &unlocalizedName, const nlohmann::json &defaultValue) {
                auto &settings = getSettingsData();

                if (!settings.contains(unlocalizedCategory))
                    settings[unlocalizedCategory] = {};

                if (!settings[unlocalizedCategory].contains(unlocalizedName))
                    settings[unlocalizedCategory][unlocalizedName] = defaultValue;

                return settings[unlocalizedCategory][unlocalizedName];
            }

            nlohmann::json &getSettingsData() {
                static nlohmann::json settings;

                return settings;
            }

            #if defined(OS_WEB)
                void load() {
                    char *data = (char *) MAIN_THREAD_EM_ASM_INT({
                        let data = localStorage.getItem("config");
                        return data ? stringToNewUTF8(data) : null;
                    });

                    if (data == nullptr) {
                        store();
                    } else {
                        getSettingsData() = nlohmann::json::parse(data);
                    }
                }

                void store() {
                    auto data = getSettingsData().dump();
                    MAIN_THREAD_EM_ASM({
                        localStorage.setItem("config", UTF8ToString($0));
                    }, data.c_str());
                }

                void clear() {
                    MAIN_THREAD_EM_ASM({
                        localStorage.removeItem("config");
                    });
                }
            #else
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
                    auto settingsData = getSettingsData();

                    // During a crash settings can be empty, causing them to be overwritten.
                    if (settingsData.empty()) {
                        return;
                    }

                    for (const auto &dir : fs::getDefaultPaths(fs::ImHexPath::Config)) {
                        wolv::io::File file(dir / SettingsFile, wolv::io::File::Mode::Write);

                        if (file.isValid()) {
                            auto result = settingsData.dump(4);

                            file.setSize(0);
                            file.writeString(result);
                            break;
                        }
                    }
                }

                void clear() {
                    for (const auto &dir : fs::getDefaultPaths(fs::ImHexPath::Config)) {
                        wolv::io::fs::remove(dir / SettingsFile);
                    }
                }
            #endif

            template<typename T>
            static T* insertOrGetEntry(std::vector<T> &vector, const UnlocalizedString &unlocalizedName) {
                T *foundEntry = nullptr;
                for (auto &entry : vector) {
                    if (entry.unlocalizedName == unlocalizedName) {
                        foundEntry = &entry;
                        break;
                    }
                }

                if (foundEntry == nullptr) {
                    if (unlocalizedName.empty())
                        foundEntry = &*vector.emplace(vector.begin(), unlocalizedName);
                    else
                        foundEntry = &vector.emplace_back(unlocalizedName);
                }

                return foundEntry;
            }

            std::vector<Category> &getSettings() {
                static std::vector<Category> categories;

                return categories;
            }

            Widgets::Widget* add(const UnlocalizedString &unlocalizedCategory, const UnlocalizedString &unlocalizedSubCategory, const UnlocalizedString &unlocalizedName, std::unique_ptr<Widgets::Widget> &&widget) {
                const auto category    = insertOrGetEntry(getSettings(),     unlocalizedCategory);
                const auto subCategory = insertOrGetEntry(category->subCategories, unlocalizedSubCategory);
                const auto entry       = insertOrGetEntry(subCategory->entries,    unlocalizedName);

                entry->widget = std::move(widget);

                return entry->widget.get();
            }

        }

        void setCategoryDescription(const UnlocalizedString &unlocalizedCategory, const UnlocalizedString &unlocalizedDescription) {
            const auto category = insertOrGetEntry(impl::getSettings(),     unlocalizedCategory);

            category->unlocalizedDescription = unlocalizedDescription;
        }

        nlohmann::json read(const UnlocalizedString &unlocalizedCategory, const UnlocalizedString &unlocalizedName, const nlohmann::json &defaultValue) {
            auto setting = impl::getSetting(unlocalizedCategory, unlocalizedName, defaultValue);

            if (setting.is_number() && defaultValue.is_boolean())
                setting = setting.get<int>() != 0;
            if (setting.is_null())
                setting = defaultValue;

            return setting;
        }

        void write(const UnlocalizedString &unlocalizedCategory, const UnlocalizedString &unlocalizedName, const nlohmann::json &value) {
            impl::getSetting(unlocalizedCategory, unlocalizedName, value) = value;
        }

        namespace Widgets {

            bool Checkbox::draw(const std::string &name) {
                return ImGui::Checkbox(name.c_str(), &m_value);
            }

            void Checkbox::load(const nlohmann::json &data) {
                if (data.is_number()) {
                    m_value = data.get<int>() != 0;
                } else if (data.is_boolean()) {
                    m_value = data.get<bool>();
                } else {
                    log::warn("Invalid data type loaded from settings for checkbox!");
                }
            }

            nlohmann::json Checkbox::store() {
                return m_value;
            }


            bool SliderInteger::draw(const std::string &name) {
                return ImGui::SliderInt(name.c_str(), &m_value, m_min, m_max);
            }

            void SliderInteger::load(const nlohmann::json &data) {
                if (data.is_number_integer()) {
                    m_value = data.get<int>();
                } else {
                    log::warn("Invalid data type loaded from settings for slider!");
                }
            }

            nlohmann::json SliderInteger::store() {
                return m_value;
            }


            bool SliderFloat::draw(const std::string &name) {
                return ImGui::SliderFloat(name.c_str(), &m_value, m_min, m_max);
            }

            void SliderFloat::load(const nlohmann::json &data) {
                if (data.is_number()) {
                    m_value = data.get<float>();
                } else {
                    log::warn("Invalid data type loaded from settings for slider!");
                }
            }

            nlohmann::json SliderFloat::store() {
                return m_value;
            }


            ColorPicker::ColorPicker(ImColor defaultColor) {
                m_value = {
                        defaultColor.Value.x,
                        defaultColor.Value.y,
                        defaultColor.Value.z,
                        defaultColor.Value.w
                };
            }

            bool ColorPicker::draw(const std::string &name) {
                return ImGui::ColorEdit4(name.c_str(), m_value.data(), ImGuiColorEditFlags_NoInputs);
            }

            void ColorPicker::load(const nlohmann::json &data) {
                if (data.is_number()) {
                    ImColor color(data.get<u32>());
                    m_value = { color.Value.x, color.Value.y, color.Value.z, color.Value.w };
                } else {
                    log::warn("Invalid data type loaded from settings for color picker!");
                }
            }

            nlohmann::json ColorPicker::store() {
                const ImColor color(m_value[0], m_value[1], m_value[2], m_value[3]);

                return static_cast<ImU32>(color);
            }

            ImColor ColorPicker::getColor() const {
                return { m_value[0], m_value[1], m_value[2], m_value[3] };
            }


            bool DropDown::draw(const std::string &name) {
                const char *preview = "";
                if (static_cast<size_t>(m_value) < m_items.size())
                    preview = m_items[m_value].c_str();

                bool changed = false;
                if (ImGui::BeginCombo(name.c_str(), Lang(preview))) {

                    int index = 0;
                    for (const auto &item : m_items) {
                        const bool selected = index == m_value;

                        if (ImGui::Selectable(Lang(item), selected)) {
                            m_value = index;
                            changed = true;
                        }

                        if (selected)
                            ImGui::SetItemDefaultFocus();

                        index += 1;
                    }

                    ImGui::EndCombo();
                }

                return changed;
            }

            void DropDown::load(const nlohmann::json &data) {
                m_value = 0;

                int defaultItemIndex = 0;

                int index = 0;
                for (const auto &item : m_settingsValues) {
                    if (item == m_defaultItem)
                        defaultItemIndex = index;

                    if (item == data) {
                        m_value = index;
                        return;
                    }

                    index += 1;
                }

                m_value = defaultItemIndex;
            }

            nlohmann::json DropDown::store() {
                if (m_value == -1)
                    return m_defaultItem;
                if (static_cast<size_t>(m_value) >= m_items.size())
                    return m_defaultItem;

                return m_settingsValues[m_value];
            }

            const nlohmann::json& DropDown::getValue() const {
                return m_settingsValues[m_value];
            }


            bool TextBox::draw(const std::string &name) {
                return ImGui::InputText(name.c_str(), m_value);
            }

            void TextBox::load(const nlohmann::json &data) {
                if (data.is_string()) {
                    m_value = data.get<std::string>();
                } else {
                    log::warn("Invalid data type loaded from settings for text box!");
                }
            }

            nlohmann::json TextBox::store() {
                return m_value;
            }


            bool FilePicker::draw(const std::string &name) {
                bool changed = false;
                if (ImGui::InputText("##font_path", m_value)) {
                    changed = true;
                }

                ImGui::SameLine();

                if (ImGuiExt::IconButton(ICON_VS_FOLDER_OPENED, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
                    return fs::openFileBrowser(fs::DialogMode::Open, { { "TTF Font", "ttf" }, { "OTF Font", "otf" } },
                                               [&](const std::fs::path &path) {
                                                   m_value = wolv::util::toUTF8String(path);
                                               });
                }

                ImGui::SameLine();

                ImGuiExt::TextFormatted("{}", name);

                return changed;
            }

            void FilePicker::load(const nlohmann::json &data) {
                if (data.is_string()) {
                    m_value = data.get<std::string>();
                } else {
                    log::warn("Invalid data type loaded from settings for file picker!");
                }
            }

            nlohmann::json FilePicker::store() {
                return m_value;
            }

            bool Label::draw(const std::string& name) {
                ImGui::NewLine();
                ImGui::TextUnformatted(name.c_str());

                return false;
            }


        }

    }


    namespace ContentRegistry::CommandPaletteCommands {

        void add(Type type, const std::string &command, const UnlocalizedString &unlocalizedDescription, const impl::DisplayCallback &displayCallback, const impl::ExecuteCallback &executeCallback) {
            log::debug("Registered new command palette command: {}", command);

            impl::getEntries().push_back(impl::Entry { type, command, unlocalizedDescription, displayCallback, executeCallback });
        }

        void addHandler(Type type, const std::string &command, const impl::QueryCallback &queryCallback, const impl::DisplayCallback &displayCallback) {
            log::debug("Registered new command palette command handler: {}", command);

            impl::getHandlers().push_back(impl::Handler { type, command, queryCallback, displayCallback });
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

            for (const auto &[ns, name, paramCount, callback, dangerous] : impl::getFunctions()) {
                if (dangerous)
                    runtime.addDangerousFunction(ns, name, paramCount, callback);
                else
                    runtime.addFunction(ns, name, paramCount, callback);
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


        void addVisualizer(const std::string &name, const impl::VisualizerFunctionCallback &function, pl::api::FunctionParameterCount parameterCount) {
            log::debug("Registered new pattern visualizer function: {}", name);
            impl::getVisualizers()[name] = impl::Visualizer { parameterCount, function };
        }

        void addInlineVisualizer(const std::string &name, const impl::VisualizerFunctionCallback &function, pl::api::FunctionParameterCount parameterCount) {
            log::debug("Registered new inline pattern visualizer function: {}", name);
            impl::getInlineVisualizers()[name] = impl::Visualizer { parameterCount, function };
        }


        namespace impl {

            std::map<std::string, Visualizer> &getVisualizers() {
                static std::map<std::string, Visualizer> visualizers;

                return visualizers;
            }

            std::map<std::string, Visualizer> &getInlineVisualizers() {
                static std::map<std::string, Visualizer> visualizers;

                return visualizers;
            }

            std::map<std::string, pl::api::PragmaHandler> &getPragmas() {
                static std::map<std::string, pl::api::PragmaHandler> pragmas;

                return pragmas;
            }

            std::vector<FunctionDefinition> &getFunctions() {
                static std::vector<FunctionDefinition> functions;

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
            log::debug("Registered new view: {}", view->getUnlocalizedName().get());

            getEntries().insert({ view->getUnlocalizedName(), std::move(view) });
        }

        View* getViewByName(const UnlocalizedString &unlocalizedName) {
            auto &views = impl::getEntries();

            if (views.contains(unlocalizedName))
                return views[unlocalizedName].get();
            else
                return nullptr;
        }

    }

    namespace ContentRegistry::Tools {

        void add(const UnlocalizedString &unlocalizedName, const impl::Callback &function) {
            log::debug("Registered new tool: {}", unlocalizedName.get());

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

        void add(const UnlocalizedString &unlocalizedName, size_t requiredSize, impl::GeneratorFunction displayGeneratorFunction, std::optional<impl::EditingFunction> editingFunction) {
            log::debug("Registered new data inspector format: {}", unlocalizedName.get());

            impl::getEntries().push_back({ unlocalizedName, requiredSize, requiredSize, std::move(displayGeneratorFunction), std::move(editingFunction) });
        }

        void add(const UnlocalizedString &unlocalizedName, size_t requiredSize, size_t maxSize, impl::GeneratorFunction displayGeneratorFunction, std::optional<impl::EditingFunction> editingFunction) {
            log::debug("Registered new data inspector format: {}", unlocalizedName.get());

            impl::getEntries().push_back({ unlocalizedName, requiredSize, maxSize, std::move(displayGeneratorFunction), std::move(editingFunction) });
        }

        namespace impl {

            std::vector<Entry> &getEntries() {
                static std::vector<Entry> entries;

                return entries;
            }

        }


    }

    namespace ContentRegistry::DataProcessorNode {

        void impl::add(const Entry &entry) {
            log::debug("Registered new data processor node type: [{}]: {}", entry.unlocalizedCategory.get(), entry.unlocalizedName.get());

            getEntries().push_back(entry);
        }

        void addSeparator() {
            impl::getEntries().push_back({ "", "", [] { return nullptr; } });
        }

        namespace impl {

            std::vector<Entry> &getEntries() {
                static std::vector<Entry> nodes;

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
                    LocalizationManager::impl::setFallbackLanguage(code.get<std::string>());
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

            std::map<std::string, std::vector<LocalizationManager::LanguageDefinition>> &getLanguageDefinitions() {
                static std::map<std::string, std::vector<LocalizationManager::LanguageDefinition>> definitions;

                return definitions;
            }

        }


    }

    namespace ContentRegistry::Interface {

        void registerMainMenuItem(const UnlocalizedString &unlocalizedName, u32 priority) {
            log::debug("Registered new main menu item: {}", unlocalizedName.get());

            impl::getMainMenuItems().insert({ priority, { unlocalizedName } });
        }

        void addMenuItem(const std::vector<UnlocalizedString> &unlocalizedMainMenuNames, u32 priority, const Shortcut &shortcut, const impl::MenuCallback &function, const impl::EnabledCallback& enabledCallback, const impl::SelectedCallback &selectedCallback, View *view) {
            addMenuItem(unlocalizedMainMenuNames, "", priority, shortcut, function, enabledCallback, selectedCallback, view);
        }

        void addMenuItem(const std::vector<UnlocalizedString> &unlocalizedMainMenuNames, const Icon &icon, u32 priority, const Shortcut &shortcut, const impl::MenuCallback &function, const impl::EnabledCallback& enabledCallback, View *view) {
            addMenuItem(unlocalizedMainMenuNames, icon, priority, shortcut, function, enabledCallback, []{ return false; }, view);
        }

        void addMenuItem(const std::vector<UnlocalizedString> &unlocalizedMainMenuNames, u32 priority, const Shortcut &shortcut, const impl::MenuCallback &function, const impl::EnabledCallback& enabledCallback, View *view) {
            addMenuItem(unlocalizedMainMenuNames, "", priority, shortcut, function, enabledCallback, []{ return false; }, view);
        }

        void addMenuItem(const std::vector<UnlocalizedString> &unlocalizedMainMenuNames, const Icon &icon, u32 priority, const Shortcut &shortcut, const impl::MenuCallback &function, const impl::EnabledCallback& enabledCallback, const impl::SelectedCallback &selectedCallback, View *view) {
            log::debug("Added new menu item to menu {} with priority {}", unlocalizedMainMenuNames[0].get(), priority);

            Icon coloredIcon = icon;
            if (coloredIcon.color == 0x00)
                coloredIcon.color = ImGuiCustomCol_ToolbarGray;

            impl::getMenuItems().insert({
                priority, impl::MenuItem { unlocalizedMainMenuNames, coloredIcon, std::make_unique<Shortcut>(shortcut), view, function, enabledCallback, selectedCallback, -1 }
            });

            if (shortcut != Shortcut::None) {
                if (shortcut.isLocal() && view != nullptr)
                    ShortcutManager::addShortcut(view, shortcut, unlocalizedMainMenuNames.back(), function);
                else
                    ShortcutManager::addGlobalShortcut(shortcut, unlocalizedMainMenuNames.back(), function);
            }
        }

        void addMenuItemSubMenu(std::vector<UnlocalizedString> unlocalizedMainMenuNames, u32 priority, const impl::MenuCallback &function, const impl::EnabledCallback& enabledCallback) {
            addMenuItemSubMenu(std::move(unlocalizedMainMenuNames), "", priority, function, enabledCallback);
        }

        void addMenuItemSubMenu(std::vector<UnlocalizedString> unlocalizedMainMenuNames, const char *icon, u32 priority, const impl::MenuCallback &function, const impl::EnabledCallback& enabledCallback) {
            log::debug("Added new menu item sub menu to menu {} with priority {}", unlocalizedMainMenuNames[0].get(), priority);

            unlocalizedMainMenuNames.emplace_back(impl::SubMenuValue);
            impl::getMenuItems().insert({
                priority, impl::MenuItem { unlocalizedMainMenuNames, icon, std::make_unique<Shortcut>(), nullptr, function, enabledCallback, []{ return false; }, -1 }
            });
        }

        void addMenuItemSeparator(std::vector<UnlocalizedString> unlocalizedMainMenuNames, u32 priority) {
            unlocalizedMainMenuNames.emplace_back(impl::SeparatorValue);
            impl::getMenuItems().insert({
                priority, impl::MenuItem { unlocalizedMainMenuNames, "", std::make_unique<Shortcut>(), nullptr, []{}, []{ return true; }, []{ return false; }, -1 }
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

        void addMenuItemToToolbar(const UnlocalizedString& unlocalizedName, ImGuiCustomCol color) {
            auto maxIndex = std::ranges::max_element(impl::getMenuItems(), [](const auto &a, const auto &b) {
                return a.second.toolbarIndex < b.second.toolbarIndex;
            })->second.toolbarIndex;

            for (auto &[priority, menuItem] : impl::getMenuItems()) {
                if (menuItem.unlocalizedNames.back() == unlocalizedName) {
                    menuItem.toolbarIndex = maxIndex + 1;
                    menuItem.icon.color = color;
                    break;
                }
            }
        }


        void addSidebarItem(const std::string &icon, const impl::DrawCallback &function, const impl::EnabledCallback &enabledCallback) {
            impl::getSidebarItems().push_back({ icon, function, enabledCallback });
        }

        void addTitleBarButton(const std::string &icon, const UnlocalizedString &unlocalizedTooltip, const impl::ClickCallback &function) {
            impl::getTitleBarButtons().push_back({ icon, unlocalizedTooltip, function });
        }

        namespace impl {

            std::multimap<u32, MainMenuItem> &getMainMenuItems() {
                static std::multimap<u32, MainMenuItem> items;

                return items;
            }
            std::multimap<u32, MenuItem> &getMenuItems() {
                static std::multimap<u32, MenuItem> items;

                return items;
            }

            std::vector<DrawCallback> &getWelcomeScreenEntries() {
                static std::vector<DrawCallback> entries;

                return entries;
            }
            std::vector<DrawCallback> &getFooterItems() {
                static std::vector<DrawCallback> items;

                return items;
            }
            std::vector<DrawCallback> &getToolbarItems() {
                static std::vector<DrawCallback> items;

                return items;
            }
            std::vector<SidebarItem> &getSidebarItems() {
                static std::vector<SidebarItem> items;

                return items;
            }
            std::vector<TitleBarButton> &getTitleBarButtons() {
                static std::vector<TitleBarButton> buttons;

                return buttons;
            }

        }

    }

    namespace ContentRegistry::Provider {

        namespace impl {

            void add(const std::string &typeName, ProviderCreationFunction creationFunction) {
                (void)RequestCreateProvider::subscribe([expectedName = typeName, creationFunction](const std::string &name, bool skipLoadInterface, bool selectProvider, prv::Provider **provider) {
                    if (name != expectedName) return;

                    prv::Provider *newProvider = creationFunction();

                    ImHexApi::Provider::add(newProvider, skipLoadInterface, selectProvider);

                    if (provider != nullptr)
                        *provider = newProvider;
                });
            }

            std::vector<std::string> &getEntries() {
                static std::vector<std::string> providerNames;

                return providerNames;
            }

            void addProviderName(const UnlocalizedString &unlocalizedName) {
                log::debug("Registered new provider: {}", unlocalizedName.get());

                getEntries().push_back(unlocalizedName);
            }

        }


    }

    namespace ContentRegistry::DataFormatter {

        void add(const UnlocalizedString &unlocalizedName, const impl::Callback &callback) {
            log::debug("Registered new data formatter: {}", unlocalizedName.get());

            impl::getEntries().push_back({ unlocalizedName, callback });
        }

        namespace impl {

            std::vector<Entry> &getEntries() {
                static std::vector<Entry> entries;

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

            std::vector<Entry> &getEntries() {
                static std::vector<Entry> entries;

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
            ImGuiExt::InputScalarCallback("##editing_input", dataType, data, format, flags | TextInputFlags | ImGuiInputTextFlags_CallbackEdit, [](ImGuiInputTextCallbackData *data) -> int {
                auto &userData = *static_cast<UserData*>(data->UserData);

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
                auto &userData = *static_cast<UserData*>(data->UserData);

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

        std::shared_ptr<DataVisualizer> getVisualizerByName(const UnlocalizedString &unlocalizedName) {
            for (const auto &visualizer : impl::getVisualizers()) {
                if (visualizer->getUnlocalizedName() == unlocalizedName)
                    return visualizer;
            }

            return nullptr;
        }

    }

    namespace ContentRegistry::Diffing {

        namespace impl {

            std::vector<std::unique_ptr<Algorithm>>& getAlgorithms() {
                static std::vector<std::unique_ptr<Algorithm>> algorithms;

                return algorithms;
            }

            void addAlgorithm(std::unique_ptr<Algorithm> &&hash) {
                getAlgorithms().emplace_back(std::move(hash));
            }

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

            struct Service {
                std::string name;
                std::jthread thread;
            };

            std::vector<Service> &getServices() {
                static std::vector<Service> services;

                return services;
            }

            void stopServices() {
                auto &services = getServices();

                for (auto &service : services) {
                    service.thread.request_stop();
                }

                for (auto &service : services) {
                    if (service.thread.joinable())
                        service.thread.join();
                }

                services.clear();
            }

        }

        void registerService(const UnlocalizedString &unlocalizedName, const impl::Callback &callback) {
            log::debug("Registered new background service: {}", unlocalizedName.get());

            impl::getServices().push_back(impl::Service {
                unlocalizedName,
                std::jthread([callback = auto(callback)](const std::stop_token &stopToken){
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

    namespace ContentRegistry::Experiments {

        namespace impl {

            std::map<std::string, Experiment> &getExperiments() {
                static std::map<std::string, Experiment> experiments;

                return experiments;
            }

        }

        void addExperiment(const std::string &experimentName, const UnlocalizedString &unlocalizedName, const UnlocalizedString &unlocalizedDescription) {
            auto &experiments = impl::getExperiments();

            if (experiments.contains(experimentName)) {
                log::error("Experiment with name '{}' already exists!", experimentName);
                return;
            }

            experiments[experimentName] = impl::Experiment {
                .unlocalizedName = unlocalizedName,
                .unlocalizedDescription = unlocalizedDescription,
                .enabled = false
            };
        }

        void enableExperiement(const std::string &experimentName, bool enabled) {
            auto &experiments = impl::getExperiments();

            if (!experiments.contains(experimentName)) {
                log::error("Experiment with name '{}' does not exist!", experimentName);
                return;
            }

            experiments[experimentName].enabled = enabled;
        }

        [[nodiscard]] bool isExperimentEnabled(const std::string &experimentName) {
            auto &experiments = impl::getExperiments();

            if (!experiments.contains(experimentName)) {
                log::error("Experiment with name '{}' does not exist!", experimentName);
                return false;
            }

            return experiments[experimentName].enabled;
        }

    }

    namespace ContentRegistry::Reports {

        namespace impl {

            std::vector<ReportGenerator> &getGenerators() {
                static std::vector<ReportGenerator> generators;

                return generators;
            }

        }

        void addReportProvider(impl::Callback callback) {
            impl::getGenerators().push_back(impl::ReportGenerator { std::move(callback ) });
        }

    }

}
