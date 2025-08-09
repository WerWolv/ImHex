#include <hex/api/content_registry.hpp>
#include <hex/api/shortcut_manager.hpp>
#include <hex/api/events/requests_provider.hpp>

#include <hex/helpers/fs.hpp>
#include <hex/helpers/logger.hpp>
#include <hex/helpers/auto_reset.hpp>
#include <hex/helpers/default_paths.hpp>

#include <hex/ui/view.hpp>
#include <hex/data_processor/node.hpp>

#include <algorithm>
#include <filesystem>
#include <jthread.hpp>

#if defined(OS_WEB)
#include <emscripten.h>
#endif

#include <hex/api/task_manager.hpp>
#include <nlohmann/json.hpp>

#include <wolv/io/file.hpp>
#include <wolv/utils/string.hpp>

namespace hex {

    namespace ContentRegistry::Settings {

        [[maybe_unused]] constexpr auto SettingsFile = "settings.json";

        namespace impl {

            struct OnChange {
                u64 id;
                OnChangeCallback callback;
            };

            static AutoReset<std::map<std::string, std::map<std::string, std::vector<OnChange>>>> s_onChangeCallbacks;

            static void runAllOnChangeCallbacks() {
                for (const auto &[category, rest] : *impl::s_onChangeCallbacks) {
                    for (const auto &[name, callbacks] : rest) {
                        for (const auto &[id, callback] : callbacks) {
                            try {
                                callback(getSetting(category, name, {}));
                            } catch (const std::exception &e) {
                                log::error("Failed to load setting [{} / {}]: {}", category, name, e.what());
                            }
                        }
                    }
                }
            }

            void runOnChangeHandlers(const UnlocalizedString &unlocalizedCategory, const UnlocalizedString &unlocalizedName, const nlohmann::json &value) {
                if (auto categoryIt = s_onChangeCallbacks->find(unlocalizedCategory); categoryIt != s_onChangeCallbacks->end()) {
                    if (auto nameIt = categoryIt->second.find(unlocalizedName); nameIt != categoryIt->second.end()) {
                        for (const auto &[id, callback] : nameIt->second) {
                            try {
                                callback(value);
                            } catch (const nlohmann::json::exception &e) {
                                log::error("Failed to run onChange handler for setting {}/{}: {}", unlocalizedCategory.get(), unlocalizedName.get(), e.what());
                            }
                        }
                    }
                }
            }

            static AutoReset<nlohmann::json> s_settings;
            const nlohmann::json& getSettingsData() {
                return s_settings;
            }

            nlohmann::json& getSetting(const UnlocalizedString &unlocalizedCategory, const UnlocalizedString &unlocalizedName, const nlohmann::json &defaultValue) {
                auto &settings = *s_settings;

                if (!settings.contains(unlocalizedCategory))
                    settings[unlocalizedCategory] = {};

                if (!settings[unlocalizedCategory].contains(unlocalizedName))
                    settings[unlocalizedCategory][unlocalizedName] = defaultValue;

                if (settings[unlocalizedCategory][unlocalizedName].is_null())
                    settings[unlocalizedCategory][unlocalizedName] = defaultValue;

                return settings[unlocalizedCategory][unlocalizedName];
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
                        s_settings = nlohmann::json::parse(data);
                    }

                    runAllOnChangeCallbacks();
                }

                void store() {
                    if (!s_settings.isValid())
                        return;

                    const auto &settingsData = *s_settings;

                    // During a crash settings can be empty, causing them to be overwritten.
                    if (settingsData.empty()) {
                        return;
                    }

                    const auto result = settingsData.dump(4);
                    if (result.empty()) {
                        return;
                    }

                    MAIN_THREAD_EM_ASM({
                        localStorage.setItem("config", UTF8ToString($0));
                    }, result.c_str());
                }

                void clear() {
                    MAIN_THREAD_EM_ASM({
                        localStorage.removeItem("config");
                    });
                }

            #else

                void load() {
                    bool loaded = false;
                    for (const auto &dir : paths::Config.read()) {
                        wolv::io::File file(dir / SettingsFile, wolv::io::File::Mode::Read);

                        if (file.isValid()) {
                            s_settings = nlohmann::json::parse(file.readString());
                            loaded = true;
                            break;
                        }
                    }

                    if (!loaded)
                        store();

                    runAllOnChangeCallbacks();
                }

                void store() {
                    if (!s_settings.isValid())
                        return;

                    const auto &settingsData = *s_settings;

                    // During a crash settings can be empty, causing them to be overwritten.
                    if (s_settings->empty()) {
                        return;
                    }

                    const auto result = settingsData.dump(4);
                    if (result.empty()) {
                        return;
                    }
                    for (const auto &dir : paths::Config.write()) {
                        wolv::io::File file(dir / SettingsFile, wolv::io::File::Mode::Create);

                        if (file.isValid()) {
                            file.writeString(result);
                            break;
                        }
                    }
                }

                void clear() {
                    for (const auto &dir : paths::Config.write()) {
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

            static AutoReset<std::vector<Category>> s_categories;
            const std::vector<Category>& getSettings() {
                return *s_categories;
            }

            Widgets::Widget* add(const UnlocalizedString &unlocalizedCategory, const UnlocalizedString &unlocalizedSubCategory, const UnlocalizedString &unlocalizedName, std::unique_ptr<Widgets::Widget> &&widget) {
                const auto category    = insertOrGetEntry(*s_categories,           unlocalizedCategory);
                const auto subCategory = insertOrGetEntry(category->subCategories, unlocalizedSubCategory);
                const auto entry       = insertOrGetEntry(subCategory->entries,    unlocalizedName);

                entry->widget = std::move(widget);
                if (entry->widget != nullptr) {
                    onChange(unlocalizedCategory, unlocalizedName, [widget = entry->widget.get(), unlocalizedCategory, unlocalizedName](const SettingsValue &) {
                        auto defaultValue = widget->store();
                        try {
                            widget->load(ContentRegistry::Settings::impl::getSetting(unlocalizedCategory, unlocalizedName, defaultValue));
                            widget->onChanged();
                        } catch (const std::exception &e) {
                            log::error("Failed to load setting [{} / {}]: {}", unlocalizedCategory.get(), unlocalizedName.get(), e.what());
                            ContentRegistry::Settings::impl::getSetting(unlocalizedCategory, unlocalizedName, defaultValue) = defaultValue;
                        }
                    });

                    runOnChangeHandlers(unlocalizedCategory, unlocalizedName, getSetting(unlocalizedCategory, unlocalizedName, entry->widget->store()));
                }

                return entry->widget.get();
            }

            void printSettingReadError(const UnlocalizedString &unlocalizedCategory, const UnlocalizedString &unlocalizedName, const nlohmann::json::exception& e) {
                hex::log::error("Failed to read setting {}/{}: {}", unlocalizedCategory.get(), unlocalizedName.get(), e.what());
            }

        }

        void setCategoryDescription(const UnlocalizedString &unlocalizedCategory, const UnlocalizedString &unlocalizedDescription) {
            const auto category = insertOrGetEntry(*impl::s_categories, unlocalizedCategory);

            category->unlocalizedDescription = unlocalizedDescription;
        }

        u64 onChange(const UnlocalizedString &unlocalizedCategory, const UnlocalizedString &unlocalizedName, const OnChangeCallback &callback) {
            static u64 id = 1;
            (*impl::s_onChangeCallbacks)[unlocalizedCategory][unlocalizedName].emplace_back(id, callback);

            auto result = id;
            id += 1;

            return result;
        }

        void removeOnChangeHandler(u64 id) {
            bool done = false;
            auto categoryIt = impl::s_onChangeCallbacks->begin();
            for (; categoryIt != impl::s_onChangeCallbacks->end(); ++categoryIt) {
                auto nameIt = categoryIt->second.begin();
                for (; nameIt != categoryIt->second.end(); ++nameIt) {
                    done = std::erase_if(nameIt->second, [id](const impl::OnChange &entry) {
                        return entry.id == id;
                    }) > 0;

                    if (done) break;
                }

                if (done) {
                    if (nameIt->second.empty())
                        categoryIt->second.erase(nameIt);

                    break;
                }
            }

            if (done) {
                if (categoryIt->second.empty())
                    impl::s_onChangeCallbacks->erase(categoryIt);
            }
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


            bool SliderDataSize::draw(const std::string &name) {
                return ImGuiExt::SliderBytes(name.c_str(), &m_value, m_min, m_max, m_stepSize);
            }

            void SliderDataSize::load(const nlohmann::json &data) {
                if (data.is_number_integer()) {
                    m_value = data.get<u64>();
                } else {
                    log::warn("Invalid data type loaded from settings for slider!");
                }
            }

            nlohmann::json SliderDataSize::store() {
                return m_value;
            }


            ColorPicker::ColorPicker(ImColor defaultColor, ImGuiColorEditFlags flags) {
                m_defaultValue = m_value = {
                    defaultColor.Value.x,
                    defaultColor.Value.y,
                    defaultColor.Value.z,
                    defaultColor.Value.w
                };
                m_flags = flags;
            }

            static bool areColorsEquals(const std::array<float, 4> &a, const std::array<float, 4> &b) {
                return std::abs(a[0] - b[0]) < 0.005f &&
                       std::abs(a[1] - b[1]) < 0.005f &&
                       std::abs(a[2] - b[2]) < 0.005f &&
                       std::abs(a[3] - b[3]) < 0.005f;
            }

            bool ColorPicker::draw(const std::string &name) {
                ImGui::PushID(name.c_str());
                auto result = ImGui::ColorEdit4("##color_picker", m_value.data(), ImGuiColorEditFlags_NoInputs | m_flags);

                ImGui::SameLine();

                ImGui::BeginDisabled(areColorsEquals(m_value, m_defaultValue));
                if (ImGuiExt::DimmedButton("\xee\xab\xa2", ImGui::GetStyle().FramePadding * 2 + ImVec2(ImGui::GetTextLineHeight(), ImGui::GetTextLineHeight()))) {
                    m_value = m_defaultValue;
                    result = true;
                }
                ImGui::EndDisabled();

                ImGui::SameLine();

                ImGui::TextUnformatted(name.c_str());

                ImGui::PopID();

                return result;
            }

            void ColorPicker::load(const nlohmann::json &data) {
                if (data.is_number()) {
                    const ImColor color(data.get<u32>());
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
                auto preview = "";
                if (static_cast<size_t>(m_value) < m_items.size())
                    preview = m_items[m_value].get().c_str();

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

                auto pathString = wolv::util::toUTF8String(m_path);
                if (ImGui::InputText("##font_path", pathString)) {
                    changed = true;
                }

                ImGui::SameLine();

                if (ImGuiExt::IconButton("...", ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
                    changed = fs::openFileBrowser(fs::DialogMode::Open, { { "TTF Font", "ttf" }, { "OTF Font", "otf" } },
                                               [&](const std::fs::path &path) {
                                                   pathString = wolv::util::toUTF8String(path);
                                               });
                }

                ImGui::SameLine();

                ImGuiExt::TextFormatted("{}", name);

                if (changed) {
                    m_path = pathString;
                }

                return changed;
            }

            void FilePicker::load(const nlohmann::json &data) {
                if (data.is_string()) {
                    m_path = data.get<std::fs::path>();
                } else {
                    log::warn("Invalid data type loaded from settings for file picker!");
                }
            }

            nlohmann::json FilePicker::store() {
                return m_path;
            }

            bool Label::draw(const std::string& name) {
                ImGui::NewLine();
                ImGui::TextUnformatted(name.c_str());

                return false;
            }


        }

    }


    namespace ContentRegistry::CommandPaletteCommands {

        namespace impl {

            static AutoReset<std::vector<Entry>> s_entries;
            const std::vector<Entry>& getEntries() {
                return *s_entries;
            }

            static AutoReset<std::vector<Handler>> s_handlers;
            const std::vector<Handler>& getHandlers() {
                return *s_handlers;
            }

        }

        void add(Type type, const std::string &command, const UnlocalizedString &unlocalizedDescription, const impl::DisplayCallback &displayCallback, const impl::ExecuteCallback &executeCallback) {
            log::debug("Registered new command palette command: {}", command);

            impl::s_entries->push_back(impl::Entry { type, command, unlocalizedDescription, displayCallback, executeCallback });
        }

        void addHandler(Type type, const std::string &command, const impl::QueryCallback &queryCallback, const impl::DisplayCallback &displayCallback) {
            log::debug("Registered new command palette command handler: {}", command);

            impl::s_handlers->push_back(impl::Handler { type, command, queryCallback, displayCallback });
        }

    }


    namespace ContentRegistry::PatternLanguage {

        namespace impl {

            static AutoReset<std::map<std::string, Visualizer>> s_visualizers;
            const std::map<std::string, Visualizer>& getVisualizers() {
                return *s_visualizers;
            }

            static AutoReset<std::map<std::string, Visualizer>> s_inlineVisualizers;
            const std::map<std::string, Visualizer>& getInlineVisualizers() {
                return *s_inlineVisualizers;
            }

            static AutoReset<std::map<std::string, pl::api::PragmaHandler>> s_pragmas;
            const std::map<std::string, pl::api::PragmaHandler>& getPragmas() {
                return *s_pragmas;
            }

            static AutoReset<std::vector<FunctionDefinition>> s_functions;
            const std::vector<FunctionDefinition>& getFunctions() {
                return *s_functions;
            }

            static AutoReset<std::vector<TypeDefinition>> s_types;
            const std::vector<TypeDefinition>& getTypes() {
                return *s_types;
            }

        }

        static std::string getFunctionName(const pl::api::Namespace &ns, const std::string &name) {
            std::string functionName;
            for (auto &scope : ns)
                functionName += scope + "::";

            functionName += name;

            return functionName;
        }

        pl::PatternLanguage& getRuntime() {
            static PerProvider<pl::PatternLanguage> runtime;
            AT_FIRST_TIME {
                runtime.setOnCreateCallback([](prv::Provider *provider, pl::PatternLanguage &runtime) {
                    configureRuntime(runtime, provider);
                });
            };

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

            runtime.setIncludePaths(paths::PatternsInclude.read() | paths::Patterns.read());

            for (const auto &[ns, name, paramCount, callback, dangerous] : impl::getFunctions()) {
                if (dangerous)
                    runtime.addDangerousFunction(ns, name, paramCount, callback);
                else
                    runtime.addFunction(ns, name, paramCount, callback);
            }

            for (const auto &[ns, name, paramCount, callback] : impl::getTypes()) {
                runtime.addType(ns, name, paramCount, callback);
            }

            for (const auto &[name, callback] : impl::getPragmas()) {
                runtime.addPragma(name, callback);
            }

            runtime.addDefine("__IMHEX__");
            runtime.addDefine("__IMHEX_VERSION__", ImHexApi::System::getImHexVersion().get());
        }

        void addPragma(const std::string &name, const pl::api::PragmaHandler &handler) {
            log::debug("Registered new pattern language pragma: {}", name);

            (*impl::s_pragmas)[name] = handler;
        }

        void addFunction(const pl::api::Namespace &ns, const std::string &name, pl::api::FunctionParameterCount parameterCount, const pl::api::FunctionCallback &func) {
            log::debug("Registered new pattern language function: {}", getFunctionName(ns, name));

            impl::s_functions->push_back({
                ns, name,
                parameterCount, func,
                false
            });
        }

        void addDangerousFunction(const pl::api::Namespace &ns, const std::string &name, pl::api::FunctionParameterCount parameterCount, const pl::api::FunctionCallback &func) {
            log::debug("Registered new dangerous pattern language function: {}", getFunctionName(ns, name));

            impl::s_functions->push_back({
                ns, name,
                parameterCount, func,
                true
            });
        }

        void addType(const pl::api::Namespace &ns, const std::string &name, pl::api::FunctionParameterCount parameterCount, const pl::api::TypeCallback &func) {
            log::debug("Registered new pattern language type: {}", getFunctionName(ns, name));

            impl::s_types->push_back({
                ns, name,
                parameterCount, func
            });
        }


        void addVisualizer(const std::string &name, const impl::VisualizerFunctionCallback &function, pl::api::FunctionParameterCount parameterCount) {
            log::debug("Registered new pattern visualizer function: {}", name);
            (*impl::s_visualizers)[name] = impl::Visualizer { parameterCount, function };
        }

        void addInlineVisualizer(const std::string &name, const impl::VisualizerFunctionCallback &function, pl::api::FunctionParameterCount parameterCount) {
            log::debug("Registered new inline pattern visualizer function: {}", name);
            (*impl::s_inlineVisualizers)[name] = impl::Visualizer { parameterCount, function };
        }

    }


    namespace ContentRegistry::Views {

        namespace impl {

            static AutoReset<std::map<UnlocalizedString, std::unique_ptr<View>>> s_views;
            const std::map<UnlocalizedString, std::unique_ptr<View>>& getEntries() {
                return *s_views;
            }

            void add(std::unique_ptr<View> &&view) {
                log::debug("Registered new view: {}", view->getUnlocalizedName().get());

                s_views->emplace(view->getUnlocalizedName(), std::move(view));
            }

            static AutoReset<std::unique_ptr<View>> s_fullscreenView;
            const std::unique_ptr<View>& getFullScreenView() {
                return *s_fullscreenView;
            }

            void setFullScreenView(std::unique_ptr<View> &&view) {
                s_fullscreenView = std::move(view);
            }

        }

        View* getViewByName(const UnlocalizedString &unlocalizedName) {
            auto &views = *impl::s_views;

            if (views.contains(unlocalizedName))
                return views[unlocalizedName].get();
            else
                return nullptr;
        }

        View* getFocusedView() {
            for (const auto &[unlocalizedName, view] : *impl::s_views) {
                if (view->isFocused())
                    return view.get();
            }

            return nullptr;
        }

    }

    namespace ContentRegistry::Tools {

        namespace impl {

            static AutoReset<std::vector<Entry>> s_tools;
            const std::vector<Entry>& getEntries() {
                return *s_tools;
            }

        }

        void add(const UnlocalizedString &unlocalizedName, const char *icon, const impl::Callback &function) {
            log::debug("Registered new tool: {}", unlocalizedName.get());

            impl::s_tools->emplace_back(impl::Entry { unlocalizedName, icon, function });
        }

    }

    namespace ContentRegistry::DataInspector {

        namespace impl {

            static AutoReset<std::vector<Entry>> s_entries;
            const std::vector<Entry>& getEntries() {
                return *s_entries;
            }

        }

        void add(const UnlocalizedString &unlocalizedName, size_t requiredSize, impl::GeneratorFunction displayGeneratorFunction, std::optional<impl::EditingFunction> editingFunction) {
            log::debug("Registered new data inspector format: {}", unlocalizedName.get());

            impl::s_entries->push_back({ unlocalizedName, requiredSize, requiredSize, std::move(displayGeneratorFunction), std::move(editingFunction) });
        }

        void add(const UnlocalizedString &unlocalizedName, size_t requiredSize, size_t maxSize, impl::GeneratorFunction displayGeneratorFunction, std::optional<impl::EditingFunction> editingFunction) {
            log::debug("Registered new data inspector format: {}", unlocalizedName.get());

            impl::s_entries->push_back({ unlocalizedName, requiredSize, maxSize, std::move(displayGeneratorFunction), std::move(editingFunction) });
        }

        void drawMenuItems(const std::function<void()> &function) {
            if (ImGui::BeginPopup("##DataInspectorRowContextMenu")) {
                function();
                ImGui::Separator();
                ImGui::EndPopup();
            }
        }

    }

    namespace ContentRegistry::DataProcessorNode {

        namespace impl {

            static AutoReset<std::vector<Entry>> s_nodes;
            const std::vector<Entry>& getEntries() {
                return *s_nodes;
            }

            void add(const Entry &entry) {
                log::debug("Registered new data processor node type: [{}]: {}", entry.unlocalizedCategory.get(), entry.unlocalizedName.get());

                s_nodes->push_back(entry);
            }

        }

        void addSeparator() {
            impl::s_nodes->push_back({ "", "", [] { return nullptr; } });
        }

    }

    namespace ContentRegistry::Language {

        namespace impl {

            static AutoReset<std::map<std::string, std::string>> s_languages;
            const std::map<std::string, std::string>& getLanguages() {
                return *s_languages;
            }

            static AutoReset<std::map<std::string, std::vector<LocalizationManager::LanguageDefinition>>> s_definitions;
            const std::map<std::string, std::vector<LocalizationManager::LanguageDefinition>>& getLanguageDefinitions() {
                return *s_definitions;
            }

        }

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

            impl::s_languages->emplace(code.get<std::string>(), fmt::format("{} ({})", language.get<std::string>(), country.get<std::string>()));

            std::map<std::string, std::string> translationDefinitions;
            for (auto &[key, value] : translations.items()) {
                if (!value.is_string()) [[unlikely]] {
                    log::error("Localization data has invalid fields!");
                    continue;
                }

                translationDefinitions.emplace(std::move(key), value.get<std::string>());
            }

            (*impl::s_definitions)[code.get<std::string>()].emplace_back(std::move(translationDefinitions));
        }

    }

    namespace ContentRegistry::Interface {

        namespace impl {

            static AutoReset<std::multimap<u32, MainMenuItem>> s_mainMenuItems;
            const std::multimap<u32, MainMenuItem>& getMainMenuItems() {
                return *s_mainMenuItems;
            }

            static AutoReset<std::multimap<u32, MenuItem>> s_menuItems;
            const std::multimap<u32, MenuItem>& getMenuItems() {
                return *s_menuItems;
            }

            static AutoReset<std::vector<MenuItem*>> s_toolbarMenuItems;
            const std::vector<MenuItem*>& getToolbarMenuItems() {
                return s_toolbarMenuItems;
            }


            std::multimap<u32, MenuItem>& getMenuItemsMutable() {
                return *s_menuItems;
            }

            static AutoReset<std::vector<DrawCallback>> s_welcomeScreenEntries;
            const std::vector<DrawCallback>& getWelcomeScreenEntries() {
                return *s_welcomeScreenEntries;
            }

            static AutoReset<std::vector<DrawCallback>> s_footerItems;
            const std::vector<DrawCallback>& getFooterItems() {
                return *s_footerItems;
            }

            static AutoReset<std::vector<DrawCallback>> s_toolbarItems;
            const std::vector<DrawCallback>& getToolbarItems() {
                return *s_toolbarItems;
            }

            static AutoReset<std::vector<SidebarItem>> s_sidebarItems;
            const std::vector<SidebarItem>& getSidebarItems() {
                return *s_sidebarItems;
            }

            static AutoReset<std::vector<TitleBarButton>> s_titlebarButtons;
            const std::vector<TitleBarButton>& getTitlebarButtons() {
                return *s_titlebarButtons;
            }

            static AutoReset<std::vector<WelcomeScreenQuickSettingsToggle>> s_welcomeScreenQuickSettingsToggles;
            const std::vector<WelcomeScreenQuickSettingsToggle>& getWelcomeScreenQuickSettingsToggles() {
                return *s_welcomeScreenQuickSettingsToggles;
            }

        }

        void registerMainMenuItem(const UnlocalizedString &unlocalizedName, u32 priority) {
            log::debug("Registered new main menu item: {}", unlocalizedName.get());

            impl::s_mainMenuItems->insert({ priority, { unlocalizedName } });
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

        void addMenuItem(const std::vector<UnlocalizedString> &unlocalizedMainMenuNames, const Icon &icon, u32 priority, Shortcut shortcut, const impl::MenuCallback &function, const impl::EnabledCallback& enabledCallback, const impl::SelectedCallback &selectedCallback, View *view) {
            log::debug("Added new menu item to menu {} with priority {}", unlocalizedMainMenuNames[0].get(), priority);

            Icon coloredIcon = icon;
            if (coloredIcon.color == 0x00)
                coloredIcon.color = ImGuiCustomCol_ToolbarGray;

            impl::s_menuItems->insert({
                priority, impl::MenuItem { unlocalizedMainMenuNames, coloredIcon, shortcut, view, function, enabledCallback, selectedCallback, -1 }
            });

            if (shortcut != Shortcut::None) {
                if (view != nullptr && !shortcut.isLocal()) {
                    shortcut += CurrentView;
                }

                if (shortcut.isLocal() && view != nullptr)
                    ShortcutManager::addShortcut(view, shortcut, unlocalizedMainMenuNames, function, enabledCallback);
                else
                    ShortcutManager::addGlobalShortcut(shortcut, unlocalizedMainMenuNames, function, enabledCallback);
            }
        }

        void addMenuItemSubMenu(std::vector<UnlocalizedString> unlocalizedMainMenuNames, u32 priority, const impl::MenuCallback &function, const impl::EnabledCallback& enabledCallback, View *view) {
            addMenuItemSubMenu(std::move(unlocalizedMainMenuNames), "", priority, function, enabledCallback, view);
        }

        void addMenuItemSubMenu(std::vector<UnlocalizedString> unlocalizedMainMenuNames, const char *icon, u32 priority, const impl::MenuCallback &function, const impl::EnabledCallback& enabledCallback, View *view) {
            log::debug("Added new menu item sub menu to menu {} with priority {}", unlocalizedMainMenuNames[0].get(), priority);

            unlocalizedMainMenuNames.emplace_back(impl::SubMenuValue);
            impl::s_menuItems->insert({
                priority, impl::MenuItem { unlocalizedMainMenuNames, icon, Shortcut::None, view, function, enabledCallback, []{ return false; }, -1 }
            });
        }

        void addMenuItemSeparator(std::vector<UnlocalizedString> unlocalizedMainMenuNames, u32 priority, View *view) {
            unlocalizedMainMenuNames.emplace_back(impl::SeparatorValue);
            impl::s_menuItems->insert({
                priority, impl::MenuItem { unlocalizedMainMenuNames, "", Shortcut::None, view, []{}, []{ return true; }, []{ return false; }, -1 }
            });
        }

        void addWelcomeScreenEntry(const impl::DrawCallback &function) {
            impl::s_welcomeScreenEntries->push_back(function);
        }

        void addFooterItem(const impl::DrawCallback &function) {
            impl::s_footerItems->push_back(function);
        }

        void addToolbarItem(const impl::DrawCallback &function) {
            impl::s_toolbarItems->push_back(function);
        }

        void addMenuItemToToolbar(const UnlocalizedString& unlocalizedName, ImGuiCustomCol color) {
            const auto maxIndex = std::ranges::max_element(impl::getMenuItems(), [](const auto &a, const auto &b) {
                return a.second.toolbarIndex < b.second.toolbarIndex;
            })->second.toolbarIndex;

            for (auto &[priority, menuItem] : *impl::s_menuItems) {
                if (menuItem.unlocalizedNames.back() == unlocalizedName) {
                    menuItem.toolbarIndex = maxIndex + 1;
                    menuItem.icon.color = color;
                    updateToolbarItems();

                    break;
                }
            }
        }

        struct MenuItemSorter {
            bool operator()(const auto *a, const auto *b) const {
                return a->toolbarIndex < b->toolbarIndex;
            }
        };

        void updateToolbarItems() {
            std::set<ContentRegistry::Interface::impl::MenuItem*, MenuItemSorter> menuItems;

            for (auto &[priority, menuItem] : impl::getMenuItemsMutable()) {
                if (menuItem.toolbarIndex != -1) {
                    menuItems.insert(&menuItem);
                }
            }

            impl::s_toolbarMenuItems->clear();
            for (auto menuItem : menuItems) {
                impl::s_toolbarMenuItems->push_back(menuItem);
            }
        }



        void addSidebarItem(const std::string &icon, const impl::DrawCallback &function, const impl::EnabledCallback &enabledCallback) {
            impl::s_sidebarItems->push_back({ icon, function, enabledCallback });
        }

        void addTitleBarButton(const std::string &icon, ImGuiCustomCol color, const UnlocalizedString &unlocalizedTooltip, const impl::ClickCallback &function) {
            impl::s_titlebarButtons->push_back({ icon, color, unlocalizedTooltip, function });
        }

        void addWelcomeScreenQuickSettingsToggle(const std::string &icon, const UnlocalizedString &unlocalizedTooltip, bool defaultState, const impl::ToggleCallback &function) {
            impl::s_welcomeScreenQuickSettingsToggles->push_back({ icon, icon, unlocalizedTooltip, function, defaultState });
        }

        void addWelcomeScreenQuickSettingsToggle(const std::string &onIcon, const std::string &offIcon, const UnlocalizedString &unlocalizedTooltip, bool defaultState, const impl::ToggleCallback &function) {
            impl::s_welcomeScreenQuickSettingsToggles->push_back({ onIcon, offIcon, unlocalizedTooltip, function, defaultState });

        }

    }

    namespace ContentRegistry::Provider {

        namespace impl {

            void add(const std::string &typeName, ProviderCreationFunction creationFunction) {
                (void)RequestCreateProvider::subscribe([expectedName = typeName, creationFunction](const std::string &name, bool skipLoadInterface, bool selectProvider, prv::Provider **provider) {
                    if (name != expectedName) return;

                    auto newProvider = creationFunction();

                    if (provider != nullptr) {
                        *provider = newProvider.get();
                        ImHexApi::Provider::add(std::move(newProvider), skipLoadInterface, selectProvider);
                    }
                });
            }

            static AutoReset<std::vector<Entry>> s_providerNames;
            const std::vector<Entry>& getEntries() {
                return *s_providerNames;
            }

            void addProviderName(const UnlocalizedString &unlocalizedName, const char *icon) {
                log::debug("Registered new provider: {}", unlocalizedName.get());

                s_providerNames->emplace_back(unlocalizedName, icon);
            }

        }


    }

    namespace ContentRegistry::DataFormatter {

        namespace impl {

            static AutoReset<std::vector<ExportMenuEntry>> s_exportMenuEntries;
            const std::vector<ExportMenuEntry>& getExportMenuEntries() {
                return *s_exportMenuEntries;
            }

            static AutoReset<std::vector<FindExporterEntry>> s_findExportEntries;
            const std::vector<FindExporterEntry>& getFindExporterEntries() {
                return *s_findExportEntries;
            }

        }

        void addExportMenuEntry(const UnlocalizedString &unlocalizedName, const impl::Callback &callback) {
            log::debug("Registered new data formatter: {}", unlocalizedName.get());

            impl::s_exportMenuEntries->push_back({ unlocalizedName, callback });
        }

        void addFindExportFormatter(const UnlocalizedString &unlocalizedName, const std::string &fileExtension, const impl::FindExporterCallback &callback) {
            log::debug("Registered new export formatter: {}", unlocalizedName.get());

            impl::s_findExportEntries->push_back({ unlocalizedName, fileExtension, callback });
        }

    }

    namespace ContentRegistry::FileHandler {

        namespace impl {

            static AutoReset<std::vector<Entry>> s_entries;
            const std::vector<Entry>& getEntries() {
                return *s_entries;
            }

        }

        void add(const std::vector<std::string> &extensions, const impl::Callback &callback) {
            for (const auto &extension : extensions)
                log::debug("Registered new data handler for extensions: {}", extension);

            impl::s_entries->push_back({ extensions, callback });
        }

    }

    namespace ContentRegistry::HexEditor {

        int DataVisualizer::DefaultTextInputFlags() {
            return ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_NoHorizontalScroll | ImGuiInputTextFlags_AlwaysOverwrite;
        }

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
            ImGuiExt::InputScalarCallback("##editing_input", dataType, data, format, flags | DefaultTextInputFlags() | ImGuiInputTextFlags_CallbackEdit, [](ImGuiInputTextCallbackData *data) -> int {
                auto &userData = *static_cast<UserData*>(data->UserData);

                if (data->CursorPos >= userData.maxChars)
                    userData.editingDone = true;

                data->Buf[userData.maxChars] = 0x00;

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
            ImGui::InputText("##editing_input", data.data(), data.size() + 1, flags | DefaultTextInputFlags() | ImGuiInputTextFlags_CallbackEdit, [](ImGuiInputTextCallbackData *data) -> int {
                auto &userData = *static_cast<UserData*>(data->UserData);

                userData.data->resize(data->BufSize);

                if (data->BufTextLen >= userData.maxChars)
                    userData.editingDone = true;

                data->Buf[userData.maxChars] = 0x00;

                return 0;
            }, &userData);
            ImGui::PopID();

            return userData.editingDone || ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_Escape);
        }

        namespace impl {

            static AutoReset<std::vector<std::shared_ptr<DataVisualizer>>> s_visualizers;
            const std::vector<std::shared_ptr<DataVisualizer>>& getVisualizers() {
                return *s_visualizers;
            }

            static AutoReset<std::vector<std::shared_ptr<MiniMapVisualizer>>> s_miniMapVisualizers;
            const std::vector<std::shared_ptr<MiniMapVisualizer>>& getMiniMapVisualizers() {
                return *s_miniMapVisualizers;
            }

            void addDataVisualizer(std::shared_ptr<DataVisualizer> &&visualizer) {
                s_visualizers->emplace_back(std::move(visualizer));

            }

        }

        std::shared_ptr<DataVisualizer> getVisualizerByName(const UnlocalizedString &unlocalizedName) {
            for (const auto &visualizer : impl::getVisualizers()) {
                if (visualizer->getUnlocalizedName() == unlocalizedName)
                    return visualizer;
            }

            return nullptr;
        }

        void addMiniMapVisualizer(UnlocalizedString unlocalizedName, MiniMapVisualizer::Callback callback) {
            impl::s_miniMapVisualizers->emplace_back(std::make_shared<MiniMapVisualizer>(std::move(unlocalizedName), std::move(callback)));
        }

    }

    namespace ContentRegistry::Diffing {

        namespace impl {

            static AutoReset<std::vector<std::unique_ptr<Algorithm>>> s_algorithms;
            const std::vector<std::unique_ptr<Algorithm>>& getAlgorithms() {
                return *s_algorithms;
            }

            void addAlgorithm(std::unique_ptr<Algorithm> &&hash) {
                s_algorithms->emplace_back(std::move(hash));
            }

        }

    }

    namespace ContentRegistry::Hashes {

        namespace impl {

            static AutoReset<std::vector<std::unique_ptr<Hash>>> s_hashes;
            const std::vector<std::unique_ptr<Hash>>& getHashes() {
                return *s_hashes;
            }

            void add(std::unique_ptr<Hash> &&hash) {
                s_hashes->emplace_back(std::move(hash));
            }

        }

    }

    namespace ContentRegistry::BackgroundServices {

        namespace impl {

            class Service {
            public:
                Service(const UnlocalizedString &unlocalizedName, std::jthread thread) : m_unlocalizedName(std::move(unlocalizedName)), m_thread(std::move(thread)) { }
                Service(const Service&) = delete;
                Service(Service &&) = default;
                ~Service() {
                    m_thread.request_stop();
                    if (m_thread.joinable())
                        m_thread.join();
                }

                Service& operator=(const Service&) = delete;
                Service& operator=(Service &&) = default;

                [[nodiscard]] const UnlocalizedString& getUnlocalizedName() const {
                    return m_unlocalizedName;
                }

                [[nodiscard]] const std::jthread& getThread() const {
                    return m_thread;
                }

            private:
                UnlocalizedString m_unlocalizedName;
                std::jthread m_thread;
            };

            static AutoReset<std::vector<Service>> s_services;
            const std::vector<Service>& getServices() {
                return *s_services;
            }

            void stopServices() {
                s_services->clear();
            }

        }

        void registerService(const UnlocalizedString &unlocalizedName, const impl::Callback &callback) {
            log::debug("Registered new background service: {}", unlocalizedName.get());

            impl::s_services->emplace_back(
                unlocalizedName,
                std::jthread([=](const std::stop_token &stopToken){
                    TaskManager::setCurrentThreadName(Lang(unlocalizedName));
                    while (!stopToken.stop_requested()) {
                        callback();
                        std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    }
                })
            );
        }

    }

    namespace ContentRegistry::CommunicationInterface {

        namespace impl {

            static AutoReset<std::map<std::string, NetworkCallback>> s_endpoints;
            const std::map<std::string, NetworkCallback>& getNetworkEndpoints() {
                return *s_endpoints;
            }

        }

        void registerNetworkEndpoint(const std::string &endpoint, const impl::NetworkCallback &callback) {
            log::debug("Registered new network endpoint: {}", endpoint);

            impl::s_endpoints->insert({ endpoint, callback });
        }

    }

    namespace ContentRegistry::Experiments {

        namespace impl {

            static AutoReset<std::map<std::string, Experiment>> s_experiments;
            const std::map<std::string, Experiment>& getExperiments() {
                return *s_experiments;
            }

        }

        void addExperiment(const std::string &experimentName, const UnlocalizedString &unlocalizedName, const UnlocalizedString &unlocalizedDescription) {
            auto &experiments = *impl::s_experiments;

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
            auto &experiments = *impl::s_experiments;

            if (!experiments.contains(experimentName)) {
                log::error("Experiment with name '{}' does not exist!", experimentName);
                return;
            }

            experiments[experimentName].enabled = enabled;
        }

        [[nodiscard]] bool isExperimentEnabled(const std::string &experimentName) {
            auto &experiments = *impl::s_experiments;

            if (!experiments.contains(experimentName)) {
                log::error("Experiment with name '{}' does not exist!", experimentName);
                return false;
            }

            return experiments[experimentName].enabled;
        }

    }

    namespace ContentRegistry::Reports {

        namespace impl {

            static AutoReset<std::vector<ReportGenerator>> s_generators;
            const std::vector<ReportGenerator>& getGenerators() {
                return *s_generators;
            }

        }

        void addReportProvider(impl::Callback callback) {
            impl::s_generators->push_back(impl::ReportGenerator { std::move(callback ) });
        }

    }

    namespace ContentRegistry::DataInformation {

        namespace impl {

            static AutoReset<std::vector<CreateCallback>> s_informationSectionConstructors;
            const std::vector<CreateCallback>& getInformationSectionConstructors() {
                return *s_informationSectionConstructors;
            }

            void addInformationSectionCreator(const CreateCallback &callback) {
                s_informationSectionConstructors->emplace_back(callback);
            }

        }

    }

    namespace ContentRegistry::Disassembler {

        namespace impl {

            static AutoReset<std::map<std::string, impl::CreatorFunction>> s_architectures;

            void addArchitectureCreator(impl::CreatorFunction function) {
                const auto arch = function();
                (*s_architectures)[arch->getName()] = std::move(function);
            }

            const std::map<std::string, impl::CreatorFunction>& getArchitectures() {
                return *s_architectures;
            }

        }

    }

}
