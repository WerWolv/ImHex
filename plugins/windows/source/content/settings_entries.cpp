#include <hex/api/content_registry.hpp>

#include <hex/helpers/fmt.hpp>

#include <imgui.h>

#include <nlohmann/json.hpp>
#include <windows.h>

namespace hex::plugin::windows {

    namespace {

        constexpr auto ImHexContextMenuKey = R"(Software\Classes\*\shell\ImHex)";

        void addImHexContextMenuEntry() {
            // Create ImHex Root Key
            HKEY imHexRootKey;
            RegCreateKeyExA(HKEY_CURRENT_USER, ImHexContextMenuKey, 0x00, nullptr, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &imHexRootKey, nullptr);
            RegSetValueA(imHexRootKey, nullptr, REG_SZ, "Open with ImHex", 0x00);

            // Add 'Icon' key to use first icon embedded in exe
            std::array<char, MAX_PATH> imHexPath = { 0 };
            GetModuleFileNameA(nullptr, imHexPath.data(), imHexPath.size());
            auto iconValue = hex::format(R"("{}",0)", imHexPath.data());
            RegSetKeyValueA(imHexRootKey, nullptr, "Icon", REG_SZ, iconValue.c_str(), iconValue.size() + 1);

            // Add 'command' key to pass the right-clicked file path as first argument to ImHex
            auto commandValue = hex::format(R"("{}" "%1")", imHexPath.data());
            RegSetValueA(imHexRootKey, "command", REG_SZ, commandValue.c_str(), commandValue.size() + 1);
            RegCloseKey(imHexRootKey);
        }

        void removeImHexContextMenuEntry() {
            RegDeleteTreeA(HKEY_CURRENT_USER, ImHexContextMenuKey);
        }

        bool hasImHexContextMenuEntry() {
            HKEY key;
            bool keyExists = (RegOpenKeyExA(HKEY_CURRENT_USER, ImHexContextMenuKey, 0x00, KEY_SET_VALUE, &key) == ERROR_SUCCESS);
            RegCloseKey(key);

            return keyExists;
        }

    }

    void registerSettings() {

        /* General */

        namespace Widgets = ContentRegistry::Settings::Widgets;

        ContentRegistry::Settings::add<Widgets::Checkbox>("hex.builtin.setting.general", "", "hex.builtin.setting.general.context_menu_entry", false)
                .setChangedCallback([](auto &widget) {
                    auto checked = static_cast<Widgets::Checkbox &>(widget).isChecked();

                    if (checked)
                        addImHexContextMenuEntry();
                    else
                        removeImHexContextMenuEntry();

                    widget.load(hasImHexContextMenuEntry());
                });

        ContentRegistry::Settings::add<Widgets::Checkbox>("hex.builtin.setting.interface", "hex.builtin.setting.interface.window", "hex.builtin.setting.interface.show_resource_usage", false);
    }

}