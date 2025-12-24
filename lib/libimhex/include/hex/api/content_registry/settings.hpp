#pragma once

#include <hex.hpp>

#include <hex/api/localization_manager.hpp>
#include <hex/helpers/fs.hpp>

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <optional>

#include <nlohmann/json.hpp>
#include <imgui.h>

EXPORT_MODULE namespace hex {

    /* Settings Registry. Allows adding of new entries into the ImHex preferences window. */
    namespace ContentRegistry::Settings {

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

                    Interface& setTooltip(const UnlocalizedString &tooltip) {
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
                explicit DropDown(const std::vector<std::string> &items, const std::vector<nlohmann::json> &settingsValues, const nlohmann::json &defaultItem) : m_items(items.begin(), items.end()), m_settingsValues(settingsValues), m_defaultItem(defaultItem) { }
                explicit DropDown(const std::vector<UnlocalizedString> &items, const std::vector<nlohmann::json> &settingsValues, const nlohmann::json &defaultItem) : m_items(items), m_settingsValues(settingsValues), m_defaultItem(defaultItem) { }

                bool draw(const std::string &name) override;

                void load(const nlohmann::json &data) override;
                nlohmann::json store() override;

                [[nodiscard]]
                const nlohmann::json& getValue() const;

            protected:
                std::vector<UnlocalizedString> m_items;
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

            class Spacer : public Widget {
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
                } catch (const nlohmann::json::exception &) {
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

        using OnSaveCallback = std::function<void()>;
        u64 onSave(const OnSaveCallback &callback);

    }

}