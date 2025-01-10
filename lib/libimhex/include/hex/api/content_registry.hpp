#pragma once

#include <hex.hpp>
#include <hex/api/localization_manager.hpp>
#include <hex/api/shortcut_manager.hpp>
#include <hex/helpers/concepts.hpp>

#include <functional>
#include <mutex>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <pl/pattern_language.hpp>
#include <nlohmann/json.hpp>
#include <wolv/io/fs.hpp>

using ImGuiDataType = int;
using ImGuiInputTextFlags = int;
struct ImColor;
enum ImGuiCustomCol : int;
typedef int ImGuiColorEditFlags;

namespace hex {

    class View;
    class Task;

    namespace dp {
        class Node;
    }
    namespace prv {
        class Provider;
    }

    namespace LocalizationManager {
        class LanguageDefinition;
    }

    /*
        The Content Registry is the heart of all features in ImHex that are in some way extendable by Plugins.
        It allows you to add/register new content that will be picked up and used by the ImHex core or by other
        plugins when needed.
    */
    namespace ContentRegistry {
        /* Settings Registry. Allows adding of new entries into the ImHex preferences window. */
        namespace Settings {

            namespace Widgets {

                class Widget {
                public:
                    virtual ~Widget() = default;

                    virtual bool draw(const std::string &name) = 0;

                    virtual void load(const nlohmann::json &data) = 0;
                    virtual nlohmann::json store() = 0;

                    class Interface {
                    public:
                        friend class Widget;

                        Interface& requiresRestart() {
                            m_requiresRestart = true;

                            return *this;
                        }

                        Interface& setEnabledCallback(std::function<bool()> callback) {
                            m_enabledCallback = std::move(callback);

                            return *this;
                        }

                        Interface& setChangedCallback(std::function<void(Widget&)> callback) {
                            m_changedCallback = std::move(callback);

                            return *this;
                        }

                        Interface& setTooltip(const std::string &tooltip) {
                            m_tooltip = tooltip;

                            return *this;
                        }

                        [[nodiscard]]
                        Widget& getWidget() const {
                            return *m_widget;
                        }

                    private:
                        explicit Interface(Widget *widget) : m_widget(widget) {}
                        Widget *m_widget;

                        bool m_requiresRestart = false;
                        std::function<bool()> m_enabledCallback;
                        std::function<void(Widget&)> m_changedCallback;
                        std::optional<UnlocalizedString> m_tooltip;
                    };

                    [[nodiscard]]
                    bool doesRequireRestart() const {
                        return m_interface.m_requiresRestart;
                    }

                    [[nodiscard]]
                    bool isEnabled() const {
                        return !m_interface.m_enabledCallback || m_interface.m_enabledCallback();
                    }

                    [[nodiscard]]
                    const std::optional<UnlocalizedString>& getTooltip() const {
                        return m_interface.m_tooltip;
                    }

                    void onChanged() {
                        if (m_interface.m_changedCallback)
                            m_interface.m_changedCallback(*this);
                    }

                    [[nodiscard]]
                    Interface& getInterface() {
                        return m_interface;
                    }

                private:
                    Interface m_interface = Interface(this);
                };

                class Checkbox : public Widget {
                public:
                    explicit Checkbox(bool defaultValue) : m_value(defaultValue) { }

                    bool draw(const std::string &name) override;

                    void load(const nlohmann::json &data) override;
                    nlohmann::json store() override;

                    [[nodiscard]] bool isChecked() const { return m_value; }

                protected:
                    bool m_value;
                };

                class SliderInteger : public Widget {
                public:
                    SliderInteger(i32 defaultValue, i32 min, i32 max) : m_value(defaultValue), m_min(min), m_max(max) { }
                    bool draw(const std::string &name) override;

                    void load(const nlohmann::json &data) override;
                    nlohmann::json store() override;

                    [[nodiscard]] i32 getValue() const { return m_value; }

                protected:
                    int m_value;
                    i32 m_min, m_max;
                };

                class SliderFloat : public Widget {
                public:
                    SliderFloat(float defaultValue, float min, float max) : m_value(defaultValue), m_min(min), m_max(max) { }
                    bool draw(const std::string &name) override;

                    void load(const nlohmann::json &data) override;
                    nlohmann::json store() override;

                    [[nodiscard]] float getValue() const { return m_value; }

                protected:
                    float m_value;
                    float m_min, m_max;
                };

                class SliderDataSize : public Widget {
                public:
                    SliderDataSize(u64 defaultValue, u64 min, u64 max, u64 stepSize) : m_value(defaultValue), m_min(min), m_max(max), m_stepSize(stepSize) { }
                    bool draw(const std::string &name) override;

                    void load(const nlohmann::json &data) override;
                    nlohmann::json store() override;

                    [[nodiscard]] i32 getValue() const { return m_value; }

                protected:
                    u64 m_value;
                    u64 m_min, m_max;
                    u64 m_stepSize;
                };

                class ColorPicker : public Widget {
                public:
                    explicit ColorPicker(ImColor defaultColor, ImGuiColorEditFlags flags = 0);

                    bool draw(const std::string &name) override;

                    void load(const nlohmann::json &data) override;
                    nlohmann::json store() override;

                    [[nodiscard]] ImColor getColor() const;

                protected:
                    std::array<float, 4> m_value = {}, m_defaultValue = {};
                    ImGuiColorEditFlags m_flags;
                };

                class DropDown : public Widget {
                public:
                    explicit DropDown(const std::vector<std::string> &items, const std::vector<nlohmann::json> &settingsValues, const nlohmann::json &defaultItem) : m_items(items), m_settingsValues(settingsValues), m_defaultItem(defaultItem) { }

                    bool draw(const std::string &name) override;

                    void load(const nlohmann::json &data) override;
                    nlohmann::json store() override;

                    [[nodiscard]]
                    const nlohmann::json& getValue() const;

                protected:
                    std::vector<std::string> m_items;
                    std::vector<nlohmann::json> m_settingsValues;
                    nlohmann::json m_defaultItem;

                    int m_value = -1;
                };

                class TextBox : public Widget {
                public:
                    explicit TextBox(std::string defaultValue) : m_value(std::move(defaultValue)) { }

                    bool draw(const std::string &name) override;

                    void load(const nlohmann::json &data) override;
                    nlohmann::json store() override;

                    [[nodiscard]]
                    const std::string& getValue() const { return m_value; }

                protected:
                    std::string m_value;
                };

                class FilePicker : public Widget {
                public:
                    bool draw(const std::string &name) override;

                    void load(const nlohmann::json &data) override;
                    nlohmann::json store() override;

                    [[nodiscard]] const std::fs::path& getPath() const {
                        return m_path;
                    }

                protected:
                    std::fs::path m_path;
                };

                class Label : public Widget {
                public:
                    bool draw(const std::string &name) override;

                    void load(const nlohmann::json &) override {}
                    nlohmann::json store() override { return {}; }
                };

            }

            namespace impl {

                struct Entry {
                    UnlocalizedString unlocalizedName;
                    std::unique_ptr<Widgets::Widget> widget;
                };

                struct SubCategory {
                    UnlocalizedString unlocalizedName;
                    std::vector<Entry> entries;
                };

                struct Category {
                    UnlocalizedString unlocalizedName;
                    UnlocalizedString unlocalizedDescription;
                    std::vector<SubCategory> subCategories;
                };

                void load();
                void store();
                void clear();

                const std::vector<Category>& getSettings();
                nlohmann::json& getSetting(const UnlocalizedString &unlocalizedCategory, const UnlocalizedString &unlocalizedName, const nlohmann::json &defaultValue);
                const nlohmann::json& getSettingsData();

                Widgets::Widget* add(const UnlocalizedString &unlocalizedCategory, const UnlocalizedString &unlocalizedSubCategory, const UnlocalizedString &unlocalizedName, std::unique_ptr<Widgets::Widget> &&widget);

                void printSettingReadError(const UnlocalizedString &unlocalizedCategory, const UnlocalizedString &unlocalizedName, const nlohmann::json::exception &e);

                void runOnChangeHandlers(const UnlocalizedString &unlocalizedCategory, const UnlocalizedString &unlocalizedName, const nlohmann::json &value);
            }

            template<std::derived_from<Widgets::Widget> T>
            Widgets::Widget::Interface& add(const UnlocalizedString &unlocalizedCategory, const UnlocalizedString &unlocalizedSubCategory, const UnlocalizedString &unlocalizedName, auto && ... args) {
                return impl::add(
                        unlocalizedCategory,
                        unlocalizedSubCategory,
                        unlocalizedName,
                        std::make_unique<T>(std::forward<decltype(args)>(args)...)
                    )->getInterface();
            }

            void setCategoryDescription(const UnlocalizedString &unlocalizedCategory, const UnlocalizedString &unlocalizedDescription);

            class SettingsValue {
            public:
                SettingsValue(nlohmann::json value) : m_value(std::move(value)) {}

                template<typename T>
                T get(std::common_type_t<T> defaultValue) const {
                    try {
                        auto result = m_value;
                        if (result.is_number() && std::same_as<T, bool>)
                            result = m_value.get<int>() != 0;
                        if (m_value.is_null())
                            result = defaultValue;

                        return result.get<T>();
                    } catch (const nlohmann::json::exception &e) {
                        return defaultValue;
                    }
                }
            private:
                nlohmann::json m_value;
            };

            template<typename T>
            [[nodiscard]] T read(const UnlocalizedString &unlocalizedCategory, const UnlocalizedString &unlocalizedName, const std::common_type_t<T> &defaultValue) {
                auto setting = impl::getSetting(unlocalizedCategory, unlocalizedName, defaultValue);

                try {
                    if (setting.is_number() && std::same_as<T, bool>)
                        setting = setting.template get<int>() != 0;
                    if (setting.is_null())
                        setting = defaultValue;

                    return setting.template get<T>();
                } catch (const nlohmann::json::exception &e) {
                    impl::printSettingReadError(unlocalizedCategory, unlocalizedName, e);

                    return defaultValue;
                }
            }

            template<typename T>
            void write(const UnlocalizedString &unlocalizedCategory, const UnlocalizedString &unlocalizedName, const std::common_type_t<T> &value) {
                impl::getSetting(unlocalizedCategory, unlocalizedName, value) = value;
                impl::runOnChangeHandlers(unlocalizedCategory, unlocalizedName, value);

                impl::store();
            }

            using OnChangeCallback = std::function<void(const SettingsValue &)>;
            u64 onChange(const UnlocalizedString &unlocalizedCategory, const UnlocalizedString &unlocalizedName, const OnChangeCallback &callback);

        }

        /* Command Palette Command Registry. Allows adding of new commands to the command palette */
        namespace CommandPaletteCommands {

            enum class Type : u32 {
                SymbolCommand,
                KeywordCommand
            };

            namespace impl {

                struct QueryResult {
                    std::string name;
                    std::function<void(std::string)> callback;
                };

                using DisplayCallback = std::function<std::string(std::string)>;
                using ExecuteCallback = std::function<std::optional<std::string>(std::string)>;
                using QueryCallback   = std::function<std::vector<QueryResult>(std::string)>;

                struct Entry {
                    Type type;
                    std::string command;
                    UnlocalizedString unlocalizedDescription;
                    DisplayCallback displayCallback;
                    ExecuteCallback executeCallback;
                };

                struct Handler {
                    Type type;
                    std::string command;
                    QueryCallback queryCallback;
                    DisplayCallback displayCallback;
                };

                const std::vector<Entry>& getEntries();
                const std::vector<Handler>& getHandlers();

            }

            /**
             * @brief Adds a new command to the command palette
             * @param type The type of the command
             * @param command The command to add
             * @param unlocalizedDescription The description of the command
             * @param displayCallback The callback that will be called when the command is displayed in the command palette
             * @param executeCallback The callback that will be called when the command is executed
             */
            void add(
                Type type,
                const std::string &command,
                const UnlocalizedString &unlocalizedDescription,
                const impl::DisplayCallback &displayCallback,
                const impl::ExecuteCallback &executeCallback = [](auto) { return std::nullopt; });

            /**
             * @brief Adds a new command handler to the command palette
             * @param type The type of the command
             * @param command The command to add
             * @param queryCallback The callback that will be called when the command palette wants to load the name and callback items
             * @param displayCallback The callback that will be called when the command is displayed in the command palette
             */
            void addHandler(
                Type type,
                const std::string &command,
                const impl::QueryCallback &queryCallback,
                const impl::DisplayCallback &displayCallback);
        }

        /* Pattern Language Function Registry. Allows adding of new functions that may be used inside the pattern language */
        namespace PatternLanguage {

            namespace impl {

                using VisualizerFunctionCallback = std::function<void(pl::ptrn::Pattern&, bool, std::span<const pl::core::Token::Literal>)>;

                struct FunctionDefinition {
                    pl::api::Namespace ns;
                    std::string name;

                    pl::api::FunctionParameterCount parameterCount;
                    pl::api::FunctionCallback callback;

                    bool dangerous;
                };

                struct TypeDefinition {
                    pl::api::Namespace ns;
                    std::string name;

                    pl::api::FunctionParameterCount parameterCount;
                    pl::api::TypeCallback callback;
                };

                struct Visualizer {
                    pl::api::FunctionParameterCount parameterCount;
                    VisualizerFunctionCallback callback;
                };

                const std::map<std::string, Visualizer>& getVisualizers();
                const std::map<std::string, Visualizer>& getInlineVisualizers();
                const std::map<std::string, pl::api::PragmaHandler>& getPragmas();
                const std::vector<FunctionDefinition>& getFunctions();
                const std::vector<TypeDefinition>& getTypes();

            }

            /**
             * @brief Provides access to the current provider's pattern language runtime
             * @return Runtime
             */
            pl::PatternLanguage& getRuntime();

            /**
             * @brief Provides access to the current provider's pattern language runtime's lock
             * @return Lock
             */
            std::mutex& getRuntimeLock();

            /**
             * @brief Configures the pattern language runtime using ImHex's default settings
             * @param runtime The pattern language runtime to configure
             * @param provider The provider to use for data access
             */
            void configureRuntime(pl::PatternLanguage &runtime, prv::Provider *provider);

            /**
             * @brief Adds a new pragma to the pattern language
             * @param name The name of the pragma
             * @param handler The handler that will be called when the pragma is encountered
             */
            void addPragma(const std::string &name, const pl::api::PragmaHandler &handler);

            /**
             * @brief Adds a new function to the pattern language
             * @param ns The namespace of the function
             * @param name The name of the function
             * @param parameterCount The amount of parameters the function takes
             * @param func The function callback
             */
            void addFunction(
                const pl::api::Namespace &ns,
                const std::string &name,
                pl::api::FunctionParameterCount parameterCount,
                const pl::api::FunctionCallback &func
            );

            /**
             * @brief Adds a new dangerous function to the pattern language
             * @note Dangerous functions are functions that require the user to explicitly allow them to be used
             * @param ns The namespace of the function
             * @param name The name of the function
             * @param parameterCount The amount of parameters the function takes
             * @param func The function callback
             */
            void addDangerousFunction(
                const pl::api::Namespace &ns,
                const std::string &name,
                pl::api::FunctionParameterCount parameterCount,
                const pl::api::FunctionCallback &func
            );

            /**
             * @brief Adds a new type to the pattern language
             * @param ns The namespace of the type
             * @param name The name of the type
             * @param parameterCount The amount of non-type template parameters the type takes
             * @param func The type callback
             */
            void addType(
                const pl::api::Namespace &ns,
                const std::string &name,
                pl::api::FunctionParameterCount parameterCount,
                const pl::api::TypeCallback &func
            );

            /**
             * @brief Adds a new visualizer to the pattern language
             * @note Visualizers are extensions to the [[hex::visualize]] attribute, used to visualize data
             * @param name The name of the visualizer
             * @param function The function callback
             * @param parameterCount The amount of parameters the function takes
             */
            void addVisualizer(
                const std::string &name,
                const impl::VisualizerFunctionCallback &function,
                pl::api::FunctionParameterCount parameterCount
            );

            /**
             * @brief Adds a new inline visualizer to the pattern language
             * @note Inline visualizers are extensions to the [[hex::inline_visualize]] attribute, used to visualize data
             * @param name The name of the visualizer
             * @param function The function callback
             * @param parameterCount The amount of parameters the function takes
             */
            void addInlineVisualizer(
                const std::string &name,
                const impl::VisualizerFunctionCallback &function,
                pl::api::FunctionParameterCount parameterCount
            );

        }

        /* View Registry. Allows adding of new windows */
        namespace Views {

            namespace impl {

                void add(std::unique_ptr<View> &&view);
                const std::map<std::string, std::unique_ptr<View>>& getEntries();

            }

            /**
             * @brief Adds a new view to ImHex
             * @tparam T The custom view class that extends View
             * @tparam Args Arguments types
             * @param args Arguments passed to the constructor of the view
             */
            template<std::derived_from<View> T, typename... Args>
            void add(Args &&...args) {
                return impl::add(std::make_unique<T>(std::forward<Args>(args)...));
            }

            /**
             * @brief Gets a view by its unlocalized name
             * @param unlocalizedName The unlocalized name of the view
             * @return The view if it exists, nullptr otherwise
             */
            View* getViewByName(const UnlocalizedString &unlocalizedName);
        }

        /* Tools Registry. Allows adding new entries to the tools window */
        namespace Tools {

            namespace impl {

                using Callback = std::function<void()>;

                struct Entry {
                    UnlocalizedString unlocalizedName;
                    Callback function;
                };

                const std::vector<Entry>& getEntries();

            }

            /**
             * @brief Adds a new tool to the tools window
             * @param unlocalizedName The unlocalized name of the tool
             * @param function The function that will be called to draw the tool
             */
            void add(const UnlocalizedString &unlocalizedName, const impl::Callback &function);
        }

        /* Data Inspector Registry. Allows adding of new types to the data inspector */
        namespace DataInspector {

            enum class NumberDisplayStyle : u8 {
                Decimal,
                Hexadecimal,
                Octal
            };

            namespace impl {

                using DisplayFunction   = std::function<std::string()>;
                using EditingFunction   = std::function<std::vector<u8>(std::string, std::endian)>;
                using GeneratorFunction = std::function<DisplayFunction(const std::vector<u8> &, std::endian, NumberDisplayStyle)>;

                struct Entry {
                    UnlocalizedString unlocalizedName;
                    size_t requiredSize;
                    size_t maxSize;
                    GeneratorFunction generatorFunction;
                    std::optional<EditingFunction> editingFunction;
                };

                const std::vector<Entry>& getEntries();

            }

            /**
             * @brief Adds a new entry to the data inspector
             * @param unlocalizedName The unlocalized name of the entry
             * @param requiredSize The minimum required number of bytes available for the entry to appear
             * @param displayGeneratorFunction The function that will be called to generate the display function
             * @param editingFunction The function that will be called to edit the data
             */
            void add(
                const UnlocalizedString &unlocalizedName,
                size_t requiredSize,
                impl::GeneratorFunction displayGeneratorFunction,
                std::optional<impl::EditingFunction> editingFunction = std::nullopt
            );

            /**
             * @brief Adds a new entry to the data inspector
             * @param unlocalizedName The unlocalized name of the entry
             * @param requiredSize The minimum required number of bytes available for the entry to appear
             * @param maxSize The maximum number of bytes to read from the data
             * @param displayGeneratorFunction The function that will be called to generate the display function
             * @param editingFunction The function that will be called to edit the data
             */
            void add(
                const UnlocalizedString &unlocalizedName,
                size_t requiredSize,
                size_t maxSize,
                impl::GeneratorFunction displayGeneratorFunction,
                std::optional<impl::EditingFunction> editingFunction = std::nullopt
            );

            /**
             * @brief Allows adding new menu items to data inspector row context menus. Call this function inside the
             * draw function of the data inspector row definition.
             * @param function Callback that will draw menu items
             */
            void drawMenuItems(const std::function<void()> &function);

        }

        /* Data Processor Node Registry. Allows adding new processor nodes to be used in the data processor */
        namespace DataProcessorNode {

            namespace impl {

                using CreatorFunction = std::function<std::unique_ptr<dp::Node>()>;

                struct Entry {
                    UnlocalizedString unlocalizedCategory;
                    UnlocalizedString unlocalizedName;
                    CreatorFunction creatorFunction;
                };

                void add(const Entry &entry);

                const std::vector<Entry>& getEntries();
            }


            /**
             * @brief Adds a new node to the data processor
             * @tparam T The custom node class that extends dp::Node
             * @tparam Args Arguments types
             * @param unlocalizedCategory The unlocalized category name of the node
             * @param unlocalizedName The unlocalized name of the node
             * @param args Arguments passed to the constructor of the node
             */
            template<std::derived_from<dp::Node> T, typename... Args>
            void add(const UnlocalizedString &unlocalizedCategory, const UnlocalizedString &unlocalizedName, Args &&...args) {
                add(impl::Entry {
                    unlocalizedCategory,
                    unlocalizedName,
                    [=, ...args = std::forward<Args>(args)] mutable {
                        auto node = std::make_unique<T>(std::forward<Args>(args)...);
                        node->setUnlocalizedName(unlocalizedName);
                        return node;
                    }
                });
            }

            /**
             * @brief Adds a separator to the data processor right click menu
             */
            void addSeparator();

        }

        /* Language Registry. Allows together with the Lang class and the _lang user defined literal to add new languages */
        namespace Language {

            /**
             * @brief Loads localization information from json data
             * @param data The language data
             */
            void addLocalization(const nlohmann::json &data);

            namespace impl {

                const std::map<std::string, std::string>& getLanguages();
                const std::map<std::string, std::vector<LocalizationManager::LanguageDefinition>>& getLanguageDefinitions();

            }

        }

        /* Interface Registry. Allows adding new items to various interfaces */
        namespace Interface {

            struct Icon {
                Icon(const char *glyph, ImGuiCustomCol color = ImGuiCustomCol(0x00)) : glyph(glyph), color(color) {}

                std::string glyph;
                ImGuiCustomCol color;
            };

            namespace impl {

                using DrawCallback      = std::function<void()>;
                using MenuCallback      = std::function<void()>;
                using EnabledCallback   = std::function<bool()>;
                using SelectedCallback  = std::function<bool()>;
                using ClickCallback     = std::function<void()>;

                struct MainMenuItem {
                    UnlocalizedString unlocalizedName;
                };

                struct MenuItem {
                    std::vector<UnlocalizedString> unlocalizedNames;
                    Icon icon;
                    Shortcut shortcut;
                    View *view;
                    MenuCallback callback;
                    EnabledCallback enabledCallback;
                    SelectedCallback selectedCallback;
                    i32 toolbarIndex;
                };

                struct SidebarItem {
                    std::string icon;
                    DrawCallback callback;
                    EnabledCallback enabledCallback;
                };

                struct TitleBarButton {
                    std::string icon;
                    UnlocalizedString unlocalizedTooltip;
                    ClickCallback callback;
                };

                constexpr static auto SeparatorValue = "$SEPARATOR$";
                constexpr static auto SubMenuValue = "$SUBMENU$";

                const std::multimap<u32, MainMenuItem>& getMainMenuItems();

                const std::multimap<u32, MenuItem>& getMenuItems();
                const std::vector<MenuItem*>& getToolbarMenuItems();
                std::multimap<u32, MenuItem>& getMenuItemsMutable();

                const std::vector<DrawCallback>& getWelcomeScreenEntries();
                const std::vector<DrawCallback>& getFooterItems();
                const std::vector<DrawCallback>& getToolbarItems();
                const std::vector<SidebarItem>& getSidebarItems();
                const std::vector<TitleBarButton>& getTitlebarButtons();

            }

            /**
             * @brief Adds a new top-level main menu entry
             * @param unlocalizedName The unlocalized name of the entry
             * @param priority The priority of the entry. Lower values are displayed first
             */
            void registerMainMenuItem(const UnlocalizedString &unlocalizedName, u32 priority);

            /**
             * @brief Adds a new main menu entry
             * @param unlocalizedMainMenuNames The unlocalized names of the main menu entries
             * @param icon The icon to use for the entry
             * @param priority The priority of the entry. Lower values are displayed first
             * @param shortcut The shortcut to use for the entry
             * @param function The function to call when the entry is clicked
             * @param enabledCallback The function to call to determine if the entry is enabled
             * @param view The view to use for the entry. If nullptr, the shortcut will work globally
             */
            void addMenuItem(
                const std::vector<UnlocalizedString> &unlocalizedMainMenuNames,
                const Icon &icon,
                u32 priority,
                const Shortcut &shortcut,
                const impl::MenuCallback &function,
                const impl::EnabledCallback& enabledCallback, View *view
            );

            /**
             * @brief Adds a new main menu entry
             * @param unlocalizedMainMenuNames The unlocalized names of the main menu entries
             * @param icon The icon to use for the entry
             * @param priority The priority of the entry. Lower values are displayed first
             * @param shortcut The shortcut to use for the entry
             * @param function The function to call when the entry is clicked
             * @param enabledCallback The function to call to determine if the entry is enabled
             * @param selectedCallback The function to call to determine if the entry is selected
             * @param view The view to use for the entry. If nullptr, the shortcut will work globally
             */
            void addMenuItem(
                const std::vector<UnlocalizedString> &unlocalizedMainMenuNames,
                const Icon &icon,
                u32 priority,
                const Shortcut &shortcut,
                const impl::MenuCallback &function,
                const impl::EnabledCallback& enabledCallback = []{ return true; },
                const impl::SelectedCallback &selectedCallback = []{ return false; },
                View *view = nullptr
            );

            /**
             * @brief Adds a new main menu entry
             * @param unlocalizedMainMenuNames The unlocalized names of the main menu entries
             * @param priority The priority of the entry. Lower values are displayed first
             * @param shortcut The shortcut to use for the entry
             * @param function The function to call when the entry is clicked
             * @param enabledCallback The function to call to determine if the entry is enabled
             * @param selectedCallback The function to call to determine if the entry is selected
             * @param view The view to use for the entry. If nullptr, the shortcut will work globally
             */
            void addMenuItem(
                const std::vector<UnlocalizedString> &unlocalizedMainMenuNames,
                u32 priority,
                const Shortcut &shortcut,
                const impl::MenuCallback &function,
                const impl::EnabledCallback& enabledCallback = []{ return true; },
                const impl::SelectedCallback &selectedCallback = []{ return false; },
                View *view = nullptr
            );

            /**
             * @brief Adds a new main menu sub-menu entry
             * @param unlocalizedMainMenuNames The unlocalized names of the main menu entries
             * @param priority The priority of the entry. Lower values are displayed first
             * @param function The function to call when the entry is clicked
             * @param enabledCallback The function to call to determine if the entry is enabled
             */
            void addMenuItemSubMenu(
                std::vector<UnlocalizedString> unlocalizedMainMenuNames,
                u32 priority,
                const impl::MenuCallback &function,
                const impl::EnabledCallback& enabledCallback = []{ return true; }
            );

            /**
             * @brief Adds a new main menu sub-menu entry
             * @param unlocalizedMainMenuNames The unlocalized names of the main menu entries
             * @param icon The icon to use for the entry
             * @param priority The priority of the entry. Lower values are displayed first
             * @param function The function to call when the entry is clicked
             * @param enabledCallback The function to call to determine if the entry is enabled
             */
            void addMenuItemSubMenu(
                std::vector<UnlocalizedString> unlocalizedMainMenuNames,
                const char *icon,
                u32 priority,
                const impl::MenuCallback &function,
                const impl::EnabledCallback& enabledCallback = []{ return true; }
            );


            /**
             * @brief Adds a new main menu separator
             * @param unlocalizedMainMenuNames The unlocalized names of the main menu entries
             * @param priority The priority of the entry. Lower values are displayed first
             */
            void addMenuItemSeparator(std::vector<UnlocalizedString> unlocalizedMainMenuNames, u32 priority);


            /**
             * @brief Adds a new welcome screen entry
             * @param function The function to call to draw the entry
             */
            void addWelcomeScreenEntry(const impl::DrawCallback &function);

            /**
             * @brief Adds a new footer item
             * @param function The function to call to draw the item
             */
            void addFooterItem(const impl::DrawCallback &function);

            /**
             * @brief Adds a new toolbar item
             * @param function The function to call to draw the item
             */
            void addToolbarItem(const impl::DrawCallback &function);

            /**
             * @brief Adds a menu item to the toolbar
             * @param unlocalizedName Unlocalized name of the menu item
             * @param color Color of the toolbar icon
             */
            void addMenuItemToToolbar(const UnlocalizedString &unlocalizedName, ImGuiCustomCol color);

            /**
             * @brief Reconstructs the toolbar items list after they have been modified
             */
            void updateToolbarItems();

            /**
             * @brief Adds a new sidebar item
             * @param icon The icon to use for the item
             * @param function The function to call to draw the item
             * @param enabledCallback The function
             */
            void addSidebarItem(
                const std::string &icon,
                const impl::DrawCallback &function,
                const impl::EnabledCallback &enabledCallback = []{ return true; }
            );

            /**
             * @brief Adds a new title bar button
             * @param icon The icon to use for the button
             * @param unlocalizedTooltip The unlocalized tooltip to use for the button
             * @param function The function to call when the button is clicked
             */
            void addTitleBarButton(
                const std::string &icon,
                const UnlocalizedString &unlocalizedTooltip,
                const impl::ClickCallback &function
            );

        }

        /* Provider Registry. Allows adding new data providers to be created from the UI */
        namespace Provider {

            namespace impl {

                void addProviderName(const UnlocalizedString &unlocalizedName);

                using ProviderCreationFunction = std::function<std::unique_ptr<prv::Provider>()>;
                void add(const std::string &typeName, ProviderCreationFunction creationFunction);

                const std::vector<std::string>& getEntries();

            }

            /**
             * @brief Adds a new provider to the list of providers
             * @tparam T The provider type that extends hex::prv::Provider
             * @param addToList Whether to display the provider in the Other Providers list in the welcome screen and File menu
             */
            template<std::derived_from<prv::Provider> T>
            void add(bool addToList = true) {
                auto typeName = T().getTypeName();

                impl::add(typeName, [] -> std::unique_ptr<prv::Provider> {
                    return std::make_unique<T>();
                });

                if (addToList)
                    impl::addProviderName(typeName);
            }

        }

        /* Data Formatter Registry. Allows adding formatters that are used in the Copy-As menu for example */
        namespace DataFormatter {

            namespace impl {

                using Callback = std::function<std::string(prv::Provider *provider, u64 address, size_t size, bool preview)>;
                struct ExportMenuEntry {
                    UnlocalizedString unlocalizedName;
                    Callback callback;
                };

                struct FindOccurrence {
                    Region region;
                    enum class DecodeType { ASCII, UTF8, Binary, UTF16, Unsigned, Signed, Float, Double } decodeType;
                    std::endian endian = std::endian::native;
                    bool selected;
                };

                using FindExporterCallback = std::function<std::vector<u8>(const std::vector<FindOccurrence>&, std::function<std::string(FindOccurrence)>)>;
                struct FindExporterEntry {
                    UnlocalizedString unlocalizedName;
                    std::string fileExtension;
                    FindExporterCallback callback;
                };

                /**
                 * @brief Retrieves a list of all registered data formatters used by the 'File -> Export' menu
                 */
                const std::vector<ExportMenuEntry>& getExportMenuEntries();

                /**
                 * @brief Retrieves a list of all registered data formatters used in the Results section of the 'Find' view
                 */
                const std::vector<FindExporterEntry>& getFindExporterEntries();

            }


            /**
             * @brief Adds a new data formatter
             * @param unlocalizedName The unlocalized name of the formatter
             * @param callback The function to call to format the data
             */
            void addExportMenuEntry(const UnlocalizedString &unlocalizedName, const impl::Callback &callback);

            /**
             * @brief Adds a new data exporter for Find results
             * @param unlocalizedName The unlocalized name of the formatter
             * @param callback The function to call to format the data
             */
            void addFindExportFormatter(const UnlocalizedString &unlocalizedName, const std::string fileExtension, const impl::FindExporterCallback &callback);

        }

        /* File Handler Registry. Allows adding handlers for opening files specific file types */
        namespace FileHandler {

            namespace impl {

                using Callback = std::function<bool(std::fs::path)>;
                struct Entry {
                    std::vector<std::string> extensions;
                    Callback callback;
                };

                const std::vector<Entry>& getEntries();

            }

            /**
             * @brief Adds a new file handler
             * @param extensions The file extensions to handle
             * @param callback The function to call to handle the file
             */
            void add(const std::vector<std::string> &extensions, const impl::Callback &callback);

        }

        /* Hex Editor Registry. Allows adding new functionality to the hex editor */
        namespace HexEditor {

            class DataVisualizer {
            public:
                DataVisualizer(UnlocalizedString unlocalizedName, u16 bytesPerCell, u16 maxCharsPerCell)
                    : m_unlocalizedName(std::move(unlocalizedName)),
                      m_bytesPerCell(bytesPerCell),
                      m_maxCharsPerCell(maxCharsPerCell) { }

                virtual ~DataVisualizer() = default;

                virtual void draw(u64 address, const u8 *data, size_t size, bool upperCase) = 0;
                virtual bool drawEditing(u64 address, u8 *data, size_t size, bool upperCase, bool startedEditing) = 0;

                [[nodiscard]] u16 getBytesPerCell() const { return m_bytesPerCell; }
                [[nodiscard]] u16 getMaxCharsPerCell() const { return m_maxCharsPerCell; }

                [[nodiscard]] const UnlocalizedString& getUnlocalizedName() const { return m_unlocalizedName; }

            protected:
                const static int TextInputFlags;

                bool drawDefaultScalarEditingTextBox(u64 address, const char *format, ImGuiDataType dataType, u8 *data, ImGuiInputTextFlags flags) const;
                bool drawDefaultTextEditingTextBox(u64 address, std::string &data, ImGuiInputTextFlags flags) const;

            private:
                UnlocalizedString m_unlocalizedName;
                u16 m_bytesPerCell;
                u16 m_maxCharsPerCell;
            };

            struct MiniMapVisualizer {
                using Callback = std::function<void(u64, std::span<const u8>, std::vector<ImColor>&)>;

                UnlocalizedString unlocalizedName;
                Callback callback;
            };

            namespace impl {

                void addDataVisualizer(std::shared_ptr<DataVisualizer> &&visualizer);

                const std::vector<std::shared_ptr<DataVisualizer>>& getVisualizers();
                const std::vector<std::shared_ptr<MiniMapVisualizer>>& getMiniMapVisualizers();

            }

            /**
             * @brief Adds a new cell data visualizer
             * @tparam T The data visualizer type that extends hex::DataVisualizer
             * @param args The arguments to pass to the constructor of the data visualizer
             */
            template<std::derived_from<DataVisualizer> T, typename... Args>
            void addDataVisualizer(Args &&...args) {
                return impl::addDataVisualizer(std::make_shared<T>(std::forward<Args>(args)...));
            }

            /**
             * @brief Gets a data visualizer by its unlocalized name
             * @param unlocalizedName Unlocalized name of the data visualizer
             * @return The data visualizer, or nullptr if it doesn't exist
             */
            std::shared_ptr<DataVisualizer> getVisualizerByName(const UnlocalizedString &unlocalizedName);

            /**
             * @brief Adds a new minimap visualizer
             * @param unlocalizedName Unlocalized name of the minimap visualizer
             * @param callback The callback that will be called to get the color of a line
             */
            void addMiniMapVisualizer(UnlocalizedString unlocalizedName, MiniMapVisualizer::Callback callback);

        }

        /* Diffing Registry. Allows adding new diffing algorithms */
        namespace Diffing {

            enum class DifferenceType : u8 {
                Match       = 0,
                Insertion   = 1,
                Deletion    = 2,
                Mismatch    = 3
            };

            using DiffTree = wolv::container::IntervalTree<DifferenceType>;

            class Algorithm {
            public:
                explicit Algorithm(UnlocalizedString unlocalizedName, UnlocalizedString unlocalizedDescription)
                    : m_unlocalizedName(std::move(unlocalizedName)),
                      m_unlocalizedDescription(std::move(unlocalizedDescription)) { }

                virtual ~Algorithm() = default;

                virtual std::vector<DiffTree> analyze(prv::Provider *providerA, prv::Provider *providerB) const = 0;
                virtual void drawSettings() { }

                const UnlocalizedString& getUnlocalizedName() const { return m_unlocalizedName; }
                const UnlocalizedString& getUnlocalizedDescription() const { return m_unlocalizedDescription; }

            private:
                UnlocalizedString m_unlocalizedName, m_unlocalizedDescription;
            };

            namespace impl {

                const std::vector<std::unique_ptr<Algorithm>>& getAlgorithms();

                void addAlgorithm(std::unique_ptr<Algorithm> &&hash);

            }

            /**
             * @brief Adds a new hash
             * @tparam T The hash type that extends hex::Hash
             * @param args The arguments to pass to the constructor of the hash
             */
            template<typename T, typename ... Args>
            void addAlgorithm(Args && ... args) {
                impl::addAlgorithm(std::make_unique<T>(std::forward<Args>(args)...));
            }

        }

        /* Hash Registry. Allows adding new hashes to the Hash view */
        namespace Hashes {

            class Hash {
            public:
                explicit Hash(UnlocalizedString unlocalizedName) : m_unlocalizedName(std::move(unlocalizedName)) {}
                virtual ~Hash() = default;

                class Function {
                public:
                    using Callback = std::function<std::vector<u8>(const Region&, prv::Provider *)>;

                    Function(Hash *type, std::string name, Callback callback)
                        : m_type(type), m_name(std::move(name)), m_callback(std::move(callback)) {

                    }

                    [[nodiscard]] Hash *getType() { return m_type; }
                    [[nodiscard]] const Hash *getType() const { return m_type; }
                    [[nodiscard]] const std::string& getName() const { return m_name; }

                    const std::vector<u8>& get(const Region& region, prv::Provider *provider) {
                        if (m_cache.empty()) {
                            m_cache = m_callback(region, provider);
                        }

                        return m_cache;
                    }

                    void reset() {
                        m_cache.clear();
                    }

                private:
                    Hash *m_type;
                    std::string m_name;
                    Callback m_callback;

                    std::vector<u8> m_cache;
                };

                virtual void draw() { }
                [[nodiscard]] virtual Function create(std::string name) = 0;

                [[nodiscard]] virtual nlohmann::json store() const = 0;
                virtual void load(const nlohmann::json &json) = 0;

                [[nodiscard]] const UnlocalizedString& getUnlocalizedName() const {
                    return m_unlocalizedName;
                }

            protected:
                [[nodiscard]] Function create(const std::string &name, const Function::Callback &callback) {
                    return { this, name, callback };
                }

            private:
                UnlocalizedString m_unlocalizedName;
            };

            namespace impl {

                const std::vector<std::unique_ptr<Hash>>& getHashes();

                void add(std::unique_ptr<Hash> &&hash);

            }


            /**
             * @brief Adds a new hash
             * @tparam T The hash type that extends hex::Hash
             * @param args The arguments to pass to the constructor of the hash
             */
            template<typename T, typename ... Args>
            void add(Args && ... args) {
                impl::add(std::make_unique<T>(std::forward<Args>(args)...));
            }

        }

        /* Background Service Registry. Allows adding new background services */
        namespace BackgroundServices {

            namespace impl {
                using Callback = std::function<void()>;

                void stopServices();
            }

            void registerService(const UnlocalizedString &unlocalizedString, const impl::Callback &callback);
        }

        /* Network Communication Interface Registry. Allows adding new communication interface endpoints */
        namespace CommunicationInterface {

            namespace impl {
                using NetworkCallback = std::function<nlohmann::json(const nlohmann::json &)>;

                const std::map<std::string, NetworkCallback>& getNetworkEndpoints();
            }

            void registerNetworkEndpoint(const std::string &endpoint, const impl::NetworkCallback &callback);

        }

        /* Experiments Registry. Allows adding new experiments */
        namespace Experiments {

            namespace impl {

                struct Experiment {
                    UnlocalizedString unlocalizedName, unlocalizedDescription;
                    bool enabled;
                };

                const std::map<std::string, Experiment>& getExperiments();
            }

            void addExperiment(
                const std::string &experimentName,
                const UnlocalizedString &unlocalizedName,
                const UnlocalizedString &unlocalizedDescription = ""
            );

            void enableExperiement(const std::string &experimentName, bool enabled);

            [[nodiscard]] bool isExperimentEnabled(const std::string &experimentName);
        }

        /* Reports Registry. Allows adding new sections to exported reports */
        namespace Reports {

            namespace impl {

                using Callback = std::function<std::string(prv::Provider*)>;

                struct ReportGenerator {
                    Callback callback;
                };

                const std::vector<ReportGenerator>& getGenerators();

            }

            void addReportProvider(impl::Callback callback);

        }

        /* Data Information Registry. Allows adding new analyzers to the data information view */
        namespace DataInformation {

            class InformationSection {
            public:
                InformationSection(const UnlocalizedString &unlocalizedName, const UnlocalizedString &unlocalizedDescription = "", bool hasSettings = false)
                    : m_unlocalizedName(unlocalizedName), m_unlocalizedDescription(unlocalizedDescription),
                      m_hasSettings(hasSettings) { }
                virtual ~InformationSection() = default;

                [[nodiscard]] const UnlocalizedString& getUnlocalizedName() const { return m_unlocalizedName; }
                [[nodiscard]] const UnlocalizedString& getUnlocalizedDescription() const { return m_unlocalizedDescription; }

                virtual void process(Task &task, prv::Provider *provider, Region region) = 0;
                virtual void reset() = 0;

                virtual void drawSettings() { }
                virtual void drawContent() = 0;

                [[nodiscard]] bool isValid() const { return m_valid; }
                void markValid(bool valid = true) { m_valid = valid; }

                [[nodiscard]] bool isEnabled() const { return m_enabled; }
                void setEnabled(bool enabled) { m_enabled = enabled; }

                [[nodiscard]] bool isAnalyzing() const { return m_analyzing; }
                void setAnalyzing(bool analyzing) { m_analyzing = analyzing; }

                virtual void load(const nlohmann::json &data) {
                    m_enabled = data.value<bool>("enabled", true);
                }
                [[nodiscard]] virtual nlohmann::json store() {
                    nlohmann::json data;
                    data["enabled"] = m_enabled.load();

                    return data;
                }

                [[nodiscard]] bool hasSettings() const { return m_hasSettings; }

            private:
                UnlocalizedString m_unlocalizedName, m_unlocalizedDescription;
                bool m_hasSettings;

                std::atomic<bool> m_analyzing = false;
                std::atomic<bool> m_valid     = false;
                std::atomic<bool> m_enabled   = true;
            };

            namespace impl {

                using CreateCallback = std::function<std::unique_ptr<InformationSection>()>;

                const std::vector<CreateCallback>& getInformationSectionConstructors();
                void addInformationSectionCreator(const CreateCallback &callback);

            }

            template<typename T>
            void addInformationSection(auto && ...args) {
                impl::addInformationSectionCreator([args...] {
                    return std::make_unique<T>(std::forward<decltype(args)>(args)...);
                });
            }

        }

        /* Disassembler Registry. Allows adding new disassembler architectures */
        namespace Disassembler {

            struct Instruction {
                u64 address;
                u64 offset;
                size_t size;
                std::string bytes;
                std::string mnemonic;
                std::string operators;
            };

            class Architecture {
            public:
                explicit Architecture(std::string name) : m_name(std::move(name)) {}
                virtual ~Architecture() = default;

                virtual bool start() = 0;
                virtual void end() = 0;

                virtual std::optional<Instruction> disassemble(u64 imageBaseAddress, u64 instructionLoadAddress, u64 instructionDataAddress, std::span<const u8> code) = 0;
                virtual void drawSettings() = 0;

                [[nodiscard]] const std::string& getName() const { return m_name; }

            private:
                std::string m_name;
            };

            namespace impl {

                using CreatorFunction = std::function<std::unique_ptr<Architecture>()>;

                void addArchitectureCreator(CreatorFunction function);

                const std::map<std::string, CreatorFunction>& getArchitectures();

            }

            template<std::derived_from<Architecture> T>
            void add(auto && ...args) {
                impl::addArchitectureCreator([...args = std::move(args)] {
                    return std::make_unique<T>(args...);
                });
            }

        }

    }

}
