#include <hex/api/imhex_api.hpp>
#include <hex/api/content_registry.hpp>
#include <hex/api/localization_manager.hpp>
#include <hex/api/theme_manager.hpp>
#include <hex/api/shortcut_manager.hpp>
#include <hex/api/events/events_lifecycle.hpp>
#include <hex/api/layout_manager.hpp>
#include <hex/ui/view.hpp>

#include <hex/helpers/http_requests.hpp>
#include <hex/helpers/utils.hpp>

#include <imgui.h>
#include <hex/ui/imgui_imhex_extensions.h>
#include <fonts/vscode_icons.hpp>

#include <wolv/literals.hpp>
#include <wolv/utils/string.hpp>

#include <nlohmann/json.hpp>

#include <utility>
#include <hex/api/plugin_manager.hpp>
#include <romfs/romfs.hpp>

#if defined(OS_WEB)
    #include <emscripten.h>
#endif

namespace hex::plugin::builtin {

    using namespace wolv::literals;

    namespace {

        /*
            Values of this setting:
            0 - do not check for updates on startup
            1 - check for updates on startup
            2 - default value - ask the user if he wants to check for updates. This value should only be encountered on the first startup.
        */
        class ServerContactWidget : public ContentRegistry::Settings::Widgets::Widget {
        public:
            bool draw(const std::string &name) override {
                bool enabled = m_value == 1;

                if (ImGui::Checkbox(name.data(), &enabled)) {
                    m_value = enabled ? 1 : 0;
                    return true;
                }

                return false;
            }

            void load(const nlohmann::json &data) override {
                if (data.is_number())
                    m_value = data.get<int>();
            }

            nlohmann::json store() override {
                return m_value;
            }

        private:
            u32 m_value = 2;
        };

        class FPSWidget : public ContentRegistry::Settings::Widgets::Widget {
        public:
            bool draw(const std::string &name) override {
                auto format = [this]() -> std::string {
                    if (m_value > 200)
                        return "hex.builtin.setting.interface.fps.unlocked"_lang;
                    else if (m_value < 15)
                        return "hex.builtin.setting.interface.fps.native"_lang;
                    else
                        return "%d FPS";
                }();

                if (ImGui::SliderInt(name.data(), &m_value, 14, 201, format.c_str(), ImGuiSliderFlags_AlwaysClamp)) {
                    return true;
                }

                return false;
            }

            void load(const nlohmann::json &data) override {
                if (data.is_number())
                    m_value = data.get<int>();
            }

            nlohmann::json store() override {
                return m_value;
            }

        private:
            int m_value = 60;
        };

        class UserFolderWidget : public ContentRegistry::Settings::Widgets::Widget {
        public:
            bool draw(const std::string &) override {
                bool result = false;

                if (!ImGui::BeginListBox("##UserFolders", ImVec2(-(ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.x * 2), 300_scaled))) {
                    return false;
                } else {
                    for (size_t n = 0; n < m_paths.size(); n++) {
                        ImGui::PushID(n + 1);
                        const bool isSelected = (m_itemIndex == n);
                        if (ImGui::Selectable(wolv::util::toUTF8String(m_paths[n]).c_str(), isSelected)) {
                            m_itemIndex = n;
                        }

                        if (isSelected) {
                            ImGui::SetItemDefaultFocus();
                        }

                        ImGui::PopID();
                    }
                    ImGui::EndListBox();
                }
                ImGui::SameLine();
                ImGui::BeginGroup();

                if (ImGuiExt::DimmedIconButton(ICON_VS_NEW_FOLDER, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
                    fs::openFileBrowser(fs::DialogMode::Folder, {}, [&](const std::fs::path &path) {
                        if (std::find(m_paths.begin(), m_paths.end(), path) == m_paths.end()) {
                            m_paths.emplace_back(path);
                            ImHexApi::System::setAdditionalFolderPaths(m_paths);

                            result = true;
                        }
                    });
                }
                ImGuiExt::InfoTooltip("hex.builtin.setting.folders.add_folder"_lang);

                if (ImGuiExt::DimmedIconButton(ICON_VS_REMOVE, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
                    if (!m_paths.empty()) {
                        m_paths.erase(std::next(m_paths.begin(), m_itemIndex));
                        ImHexApi::System::setAdditionalFolderPaths(m_paths);

                        result = true;
                    }
                }
                ImGuiExt::InfoTooltip("hex.builtin.setting.folders.remove_folder"_lang);

                ImGui::EndGroup();

                return result;
            }

            void load(const nlohmann::json &data) override {
                if (data.is_array()) {
                    std::vector<std::string> pathStrings = data;

                    m_paths.clear();
                    for (const auto &pathString : pathStrings) {
                        m_paths.emplace_back(pathString);
                    }

                    ImHexApi::System::setAdditionalFolderPaths(m_paths);
                }
            }

            nlohmann::json store() override {
                std::vector<std::string> pathStrings;

                for (const auto &path : m_paths) {
                    pathStrings.push_back(wolv::io::fs::toNormalizedPathString(path));
                }

                return pathStrings;
            }

        private:
            u32 m_itemIndex = 0;
            std::vector<std::fs::path> m_paths;
        };

        class ScalingWidget : public ContentRegistry::Settings::Widgets::Widget {
        public:
            bool draw(const std::string &name) override {
                auto format = [this]() -> std::string {
                    if (m_value == 0)
                        return fmt::format("{} (x{:.1f})", "hex.builtin.setting.interface.scaling.native"_lang, ImHexApi::System::getNativeScale());
                    else
                        return "x%.1f";
                }();

                bool changed = ImGui::SliderFloat(name.data(), &m_value, 0, 4, format.c_str());

                if (m_value < 0)
                    m_value = 0;
                else if (m_value > 10)
                    m_value = 10;

                return changed;
            }

            void load(const nlohmann::json &data) override {
                if (data.is_number())
                    m_value = data.get<float>();
            }

            nlohmann::json store() override {
                return m_value;
            }

            float getValue() const {
                return m_value;
            }

        private:
            float m_value = 0.0F;
        };

        class AutoBackupWidget : public ContentRegistry::Settings::Widgets::Widget {
        public:
            bool draw(const std::string &name) override {
                auto format = [this]() -> std::string {
                    auto value = m_value * 30;
                    if (value == 0)
                        return "hex.ui.common.off"_lang;
                    else if (value < 60)
                        return fmt::format("hex.builtin.setting.general.auto_backup_time.format.simple"_lang, value);
                    else
                        return fmt::format("hex.builtin.setting.general.auto_backup_time.format.extended"_lang, value / 60, value % 60);
                }();

                if (ImGui::SliderInt(name.data(), &m_value, 0, (30 * 60) / 30, format.c_str(), ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_NoInput)) {
                    return true;
                }

                return false;
            }

            void load(const nlohmann::json &data) override {
                if (data.is_number())
                    m_value = data.get<int>();
            }

            nlohmann::json store() override {
                return m_value;
            }

        private:
            int m_value = 5 * 2;
        };

        class KeybindingWidget : public ContentRegistry::Settings::Widgets::Widget {
        public:
            KeybindingWidget(View *view, const Shortcut &shortcut, const std::vector<UnlocalizedString> &fullName)
                : m_view(view), m_shortcut(shortcut), m_drawShortcut(shortcut), m_defaultShortcut(shortcut), m_fullName(fullName) {}

            bool draw(const std::string &name) override {
                std::ignore = name;

                std::string label;

                if (!m_editing)
                    label = m_drawShortcut.toString();
                else
                    label = "...";

                if (label.empty())
                    label = "???";


                if (m_hasDuplicate)
                    ImGui::PushStyleColor(ImGuiCol_Text, ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_LoggerError));

                ImGui::PushID(this);
                if (ImGui::Button(label.c_str(), ImVec2(250_scaled, 0))) {
                    m_editing = !m_editing;

                    if (m_editing)
                        ShortcutManager::pauseShortcuts();
                    else
                        ShortcutManager::resumeShortcuts();
                }

                ImGui::SameLine();

                if (m_hasDuplicate)
                    ImGui::PopStyleColor();

                bool settingChanged = false;

                ImGui::BeginDisabled(m_drawShortcut.matches(m_defaultShortcut));
                if (ImGuiExt::DimmedButton(ICON_VS_DISCARD, ImGui::GetStyle().FramePadding * 2 + ImVec2(ImGui::GetTextLineHeight(), ImGui::GetTextLineHeight()))) {
                    this->reset();
                    if (!m_hasDuplicate) {
                        m_shortcut = m_defaultShortcut;
                        settingChanged = true;
                    }

                }
                ImGui::EndDisabled();

                if (!ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    m_editing = false;
                    ShortcutManager::resumeShortcuts();
                }

                ImGui::SameLine();

                std::string fullName;
                for (u32 i = m_fullName.size() == 1 ? 0 : 1; i < m_fullName.size(); i += 1) {
                    const auto &part = m_fullName[i];
                    fullName += Lang(part).get();
                    fullName += " -> ";
                }
                if (fullName.size() >= 4)
                    fullName = fullName.substr(0, fullName.size() - 4);

                ImGuiExt::TextFormatted("{}", fullName);

                ImGui::PopID();

                if (m_editing) {
                    if (this->detectShortcut()) {
                        m_editing = false;
                        ShortcutManager::resumeShortcuts();

                        settingChanged = true;
                        if (!m_hasDuplicate) {

                        }
                    }
                }

                return settingChanged;
            }

            void load(const nlohmann::json &data) override {
                std::set<Key> keys;

                if (data.empty())
                    return;

                for (const auto &key : data.get<std::vector<u32>>())
                    keys.insert(Key(scanCodeToKey(key)));

                if (keys.empty())
                    return;

                auto newShortcut = Shortcut(keys);
                m_hasDuplicate = !ShortcutManager::updateShortcut(m_shortcut, newShortcut, m_view);
                m_shortcut = std::move(newShortcut);
                m_drawShortcut = m_shortcut;
            }

            nlohmann::json store() override {
                // Don't store shortcuts that were not changed by the user
                if (m_shortcut == m_defaultShortcut)
                    return {};

                std::vector<u32> keys;

                for (const auto &key : m_shortcut.getKeys()) {
                    if (key != CurrentView)
                        keys.push_back(keyToScanCode(Keys(key.getKeyCode())));
                }

                return keys;
            }

            void reset() {
                m_hasDuplicate = !ShortcutManager::updateShortcut(m_shortcut, m_defaultShortcut, m_view);

                m_drawShortcut = m_defaultShortcut;
                m_shortcut = m_defaultShortcut;
            }

        private:
            bool detectShortcut() {
                if (const auto &shortcut = ShortcutManager::getPreviousShortcut(); shortcut.has_value()) {
                    auto keys = m_shortcut.getKeys();
                    std::erase_if(keys, [](Key key) {
                        return key != AllowWhileTyping && key != CurrentView;
                    });

                    for (const auto &key : shortcut->getKeys()) {
                        keys.insert(key);
                    }

                    auto newShortcut = Shortcut(std::move(keys));
                    m_hasDuplicate = !ShortcutManager::updateShortcut(m_shortcut, newShortcut, m_view);
                    m_drawShortcut = std::move(newShortcut);

                    if (!m_hasDuplicate) {
                        m_shortcut = m_drawShortcut;
                        log::info("Changed shortcut to {}", shortcut->toString());
                    } else {
                        log::warn("Changing shortcut failed as it overlapped with another one", shortcut->toString());
                    }

                    return true;
                }

                return false;
            }

        private:
            View *m_view = nullptr;
            Shortcut m_shortcut, m_drawShortcut, m_defaultShortcut;
            std::vector<UnlocalizedString> m_fullName;
            bool m_editing = false;
            bool m_hasDuplicate = false;
        };

        class ToolbarIconsWidget : public ContentRegistry::Settings::Widgets::Widget {
        public:
            bool draw(const std::string &) override {
                bool changed = false;

                const auto currIndex = std::ranges::count_if(ContentRegistry::Interface::impl::getMenuItems(), [](const auto &entry) {
                    return entry.second.toolbarIndex != -1;
                });

                // Top level layout table
                if (ImGui::BeginTable("##top_level", 2, ImGuiTableFlags_None, ImGui::GetContentRegionAvail())) {
                    ImGui::TableSetupColumn("##left", ImGuiTableColumnFlags_WidthStretch, 0.4F);
                    ImGui::TableSetupColumn("##right", ImGuiTableColumnFlags_WidthStretch, 0.6F);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    // Draw list of menu items that can be added to the toolbar
                    if (ImGui::BeginTable("##all_icons", 1, ImGuiTableFlags_BordersOuter | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, 280_scaled))) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();

                        // Loop over all available menu items
                        for (auto &[priority, menuItem] : ContentRegistry::Interface::impl::getMenuItems()) {
                            ImGui::PushID(&menuItem);
                            ON_SCOPE_EXIT { ImGui::PopID(); };

                            // Filter out items without icon, separators, submenus and items that are already in the toolbar

                            if (menuItem.icon.glyph.empty())
                                continue;

                            const auto &unlocalizedName = menuItem.unlocalizedNames.back();
                            if (menuItem.unlocalizedNames.size() > 2)
                                continue;
                            if (unlocalizedName.get() == ContentRegistry::Interface::impl::SeparatorValue)
                                continue;
                            if (unlocalizedName.get() == ContentRegistry::Interface::impl::SubMenuValue)
                                continue;
                            if (menuItem.toolbarIndex != -1)
                                continue;

                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();

                            std::string name = Lang(unlocalizedName);
                            if (menuItem.view != nullptr) {
                                name += fmt::format(" ({})", Lang(menuItem.view->getUnlocalizedName()));
                            }

                            // Draw the menu item
                            ImGui::Selectable(fmt::format("{} {}", menuItem.icon.glyph, name).c_str(), false, ImGuiSelectableFlags_SpanAllColumns);
                            ImGui::SetItemTooltip("%s", name.c_str());

                            // Handle dragging the menu item to the toolbar box
                            if (ImGui::BeginDragDropSource()) {
                                static const void* ptr;
                                ptr = &menuItem;
                                ImGui::SetDragDropPayload("MENU_ITEM_PAYLOAD", &ptr, sizeof(void*));

                                ImGuiExt::TextFormatted("{} {}", menuItem.icon.glyph, name);

                                ImGui::EndDragDropSource();
                            }
                        }

                        ImGui::EndTable();
                    }

                    // Handle dropping menu items from the toolbar box
                    if (ImGui::BeginDragDropTarget()) {
                        if (auto payload = ImGui::AcceptDragDropPayload("TOOLBAR_ITEM_PAYLOAD"); payload != nullptr) {
                            auto &menuItem = *static_cast<ContentRegistry::Interface::impl::MenuItem **>(payload->Data);

                            menuItem->toolbarIndex = -1;
                            changed = true;
                        }

                        ImGui::EndDragDropTarget();
                    }

                    ImGui::TableNextColumn();

                    // Draw toolbar icon box
                    if (ImGuiExt::BeginSubWindow("hex.builtin.setting.toolbar.icons"_lang, nullptr, ImGui::GetContentRegionAvail())) {
                        const auto itemWidth = 60_scaled;
                        const auto columnCount = int(ImGui::GetContentRegionAvail().x / itemWidth);
                        if (ImGui::BeginTable("##icons", columnCount, ImGuiTableFlags_SizingStretchSame, ImGui::GetContentRegionAvail())) {
                            ImGui::TableNextRow();

                            // Find all menu items that are in the toolbar and sort them by their toolbar index
                            std::vector<ContentRegistry::Interface::impl::MenuItem*> toolbarItems;
                            for (auto &[priority, menuItem] : ContentRegistry::Interface::impl::getMenuItemsMutable()) {
                                if (menuItem.toolbarIndex == -1)
                                    continue;

                                toolbarItems.emplace_back(&menuItem);
                            }
                            std::ranges::sort(toolbarItems, [](auto *a, auto *b) {
                                return a->toolbarIndex < b->toolbarIndex;
                            });

                            // Loop over all toolbar items
                            for (auto &menuItem : toolbarItems) {
                                ImGui::PushID(menuItem);
                                ON_SCOPE_EXIT { ImGui::PopID(); };

                                // Filter out items without icon, separators, submenus and items that are not in the toolbar
                                if (menuItem->icon.glyph.empty())
                                    continue;

                                const auto &unlocalizedName = menuItem->unlocalizedNames.back();
                                if (menuItem->unlocalizedNames.size() > 2)
                                    continue;
                                if (unlocalizedName.get() == ContentRegistry::Interface::impl::SubMenuValue)
                                    continue;
                                if (menuItem->toolbarIndex == -1)
                                    continue;

                                ImGui::TableNextColumn();

                                // Draw the toolbar item
                                ImGui::InvisibleButton(unlocalizedName.get().c_str(), ImVec2(itemWidth, itemWidth));

                                // Handle dragging the toolbar item around
                                if (ImGui::BeginDragDropSource()) {
                                    ImGui::SetDragDropPayload("TOOLBAR_ITEM_PAYLOAD", &menuItem, sizeof(void*));

                                    ImGuiExt::TextFormatted("{} {}", menuItem->icon.glyph, Lang(unlocalizedName));

                                    ImGui::EndDragDropSource();
                                }

                                // Handle dropping toolbar items onto each other to reorder them
                                if (ImGui::BeginDragDropTarget()) {
                                    if (auto payload = ImGui::AcceptDragDropPayload("TOOLBAR_ITEM_PAYLOAD"); payload != nullptr) {
                                        auto &otherMenuItem = *static_cast<ContentRegistry::Interface::impl::MenuItem **>(payload->Data);

                                        // Get all toolbar items sorted by toolbarIndex
                                        std::vector<ContentRegistry::Interface::impl::MenuItem*> newToolbarItems = toolbarItems;

                                        // Remove otherMenuItem from its current position
                                        auto it = std::ranges::find(newToolbarItems, otherMenuItem);
                                        if (it != newToolbarItems.end())
                                            newToolbarItems.erase(it);

                                        // Find current menuItem position
                                        auto insertPos = std::ranges::find(newToolbarItems, menuItem);

                                        if (menuItem->toolbarIndex > otherMenuItem->toolbarIndex)
                                            ++insertPos;

                                        // Insert otherMenuItem before menuItem
                                        newToolbarItems.insert(insertPos, otherMenuItem);

                                        // Update toolbarIndex for all items
                                        for (size_t i = 0; i < newToolbarItems.size(); ++i)
                                            newToolbarItems[i]->toolbarIndex = static_cast<int>(i);

                                        changed = true;
                                    }

                                    ImGui::EndDragDropTarget();
                                }

                                // Handle right clicking toolbar items to open the color selection popup
                                if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
                                    ImGui::OpenPopup(unlocalizedName.get().c_str());

                                // Draw the color selection popup
                                if (ImGui::BeginPopup(unlocalizedName.get().c_str())) {
                                    constexpr static std::array Colors = {
                                        ImGuiCustomCol_ToolbarGray,
                                        ImGuiCustomCol_ToolbarRed,
                                        ImGuiCustomCol_ToolbarYellow,
                                        ImGuiCustomCol_ToolbarGreen,
                                        ImGuiCustomCol_ToolbarBlue,
                                        ImGuiCustomCol_ToolbarPurple,
                                        ImGuiCustomCol_ToolbarBrown
                                    };

                                    // Draw all the color buttons
                                    for (auto color : Colors) {
                                        ImGui::PushID(&color);
                                        if (ImGui::ColorButton(fmt::format("##color{}", u32(color)).c_str(), ImGuiExt::GetCustomColorVec4(color), ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoPicker, ImVec2(20, 20))) {
                                            menuItem->icon.color = color;
                                            ImGui::CloseCurrentPopup();
                                            changed = true;
                                        }
                                        ImGui::PopID();
                                        ImGui::SameLine();
                                    }

                                    ImGui::EndPopup();
                                }

                                auto min = ImGui::GetItemRectMin();
                                auto max = ImGui::GetItemRectMax();
                                auto iconSize = ImGui::CalcTextSize(menuItem->icon.glyph.c_str());

                                // Draw icon and text of the toolbar item
                                auto drawList = ImGui::GetWindowDrawList();
                                drawList->AddText((min + ((max - min) - iconSize) / 2) - ImVec2(0, 10_scaled), ImGuiExt::GetCustomColorU32(ImGuiCustomCol(menuItem->icon.color)), menuItem->icon.glyph.c_str());

                                ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 0.65F);
                                {
                                    std::string name = Lang(unlocalizedName);
                                    if (name.ends_with("..."))
                                        name = name.substr(0, name.size() - 3);

                                    if (menuItem->view != nullptr) {
                                        name += fmt::format(" ({})", Lang(menuItem->view->getUnlocalizedName()));
                                    }

                                    auto textSize = ImGui::CalcTextSize(name.c_str(), nullptr, false, max.x - min.x);

                                    drawList->AddText(nullptr, 0.0F, (min + ((max - min)) / 2) + ImVec2(-textSize.x / 2, 5_scaled), ImGui::GetColorU32(ImGuiCol_Text), name.c_str(), nullptr, max.x - min.x);
                                }
                                ImGui::PopFont();
                            }

                            ImGui::EndTable();
                        }

                    }
                    ImGuiExt::EndSubWindow();

                    // Handle dropping menu items onto the toolbar box
                    if (ImGui::BeginDragDropTarget()) {
                        if (auto payload = ImGui::AcceptDragDropPayload("MENU_ITEM_PAYLOAD"); payload != nullptr) {
                            auto &menuItem = *static_cast<ContentRegistry::Interface::impl::MenuItem **>(payload->Data);

                            menuItem->toolbarIndex = currIndex;
                            changed = true;
                        }

                        ImGui::EndDragDropTarget();
                    }

                    ImGui::EndTable();
                }

                if (changed) {
                    ContentRegistry::Interface::updateToolbarItems();
                }

                return changed;
            }

            nlohmann::json store() override {
                std::map<i32, std::pair<std::string, u32>> items;

                for (const auto &[priority, menuItem] : ContentRegistry::Interface::impl::getMenuItems()) {
                    if (menuItem.toolbarIndex != -1)
                        items.emplace(menuItem.toolbarIndex, std::make_pair(menuItem.unlocalizedNames.back().get(), menuItem.icon.color));
                }

                return items;
            }

            void load(const nlohmann::json &data) override {
                if (data.is_null())
                    return;

                if (data.is_array() && data.empty())
                    return;

                for (auto &[priority, menuItem] : ContentRegistry::Interface::impl::getMenuItemsMutable())
                    menuItem.toolbarIndex = -1;

                auto toolbarItems = data.get<std::map<i32, std::pair<std::string, u32>>>();
                if (toolbarItems.empty())
                    return;

                for (auto &[priority, menuItem] : ContentRegistry::Interface::impl::getMenuItemsMutable()) {
                    for (const auto &[index, value] : toolbarItems) {
                        const auto &[name, color] = value;
                        if (menuItem.unlocalizedNames.back().get() == name) {
                            menuItem.toolbarIndex = index;
                            menuItem.icon.color   = ImGuiCustomCol(color);
                            break;
                        }
                    }
                }

                ContentRegistry::Interface::updateToolbarItems();
            }
        };

        bool getDefaultBorderlessWindowMode() {
            bool result;

            #if defined (OS_WINDOWS)
                result = true;

                // Intel's OpenGL driver is extremely buggy. Its issues can manifest in lots of different ways
                // such as "colorful snow" appearing on the screen or, the most annoying issue,
                // it might draw the window content slightly offset to the bottom right as seen in issue #1625

                // The latter issue can be circumvented by using the native OS decorations or by using the software renderer.
                // This code here tries to detect if the user has a problematic Intel GPU and if so, it will default to the native OS decorations.
                // This doesn't actually solve the issue at all but at least it makes ImHex usable on these GPUs.
                const bool isIntelGPU = hex::containsIgnoreCase(ImHexApi::System::getGPUVendor(), "Intel");
                if (isIntelGPU) {
                    log::warn("Intel GPU detected! Intel's OpenGL GPU drivers are extremely buggy and can cause issues when using ImHex. If you experience any rendering bugs, please enable the Native OS Decoration setting or try the software rendererd -NoGPU release.");

                    // Non-exhaustive list of bad GPUs.
                    // If more GPUs are found to be problematic, they can be added here.
                    constexpr static std::array BadGPUs = {
                        // Sandy Bridge Series, Second Gen HD Graphics
                        "HD Graphics 2000",
                        "HD Graphics 3000"
                    };

                    const auto &glRenderer = ImHexApi::System::getGLRenderer();
                    for (const auto &badGPU : BadGPUs) {
                        if (hex::containsIgnoreCase(glRenderer, badGPU)) {
                            result = false;
                            break;
                        }
                    }
                }

            #elif defined(OS_MACOS)
                result = true;
            #elif defined(OS_LINUX)
                // On Linux, things like Window snapping and moving is hard to implement
                // without hacking e.g. libdecor, therefor we default to the native window decorations.
                result = false;
            #else
                result = false;
            #endif

            return result;
        }

    }

    void registerSettings() {
        namespace Widgets = ContentRegistry::Settings::Widgets;

        /* General */
        {

            ContentRegistry::Settings::add<Widgets::Checkbox>("hex.builtin.setting.general", "", "hex.builtin.setting.general.show_tips", false);
            ContentRegistry::Settings::add<Widgets::Checkbox>("hex.builtin.setting.general", "", "hex.builtin.setting.general.save_recent_providers", true);
            ContentRegistry::Settings::add<AutoBackupWidget>("hex.builtin.setting.general", "", "hex.builtin.setting.general.auto_backup_time");
            ContentRegistry::Settings::add<Widgets::SliderDataSize>("hex.builtin.setting.general", "", "hex.builtin.setting.general.max_mem_file_size", 512_MiB, 0_bytes, 32_GiB, 1_MiB)
                .setTooltip("hex.builtin.setting.general.max_mem_file_size.desc");
            ContentRegistry::Settings::add<Widgets::SliderInteger>("hex.builtin.setting.general", "hex.builtin.setting.general.patterns", "hex.builtin.setting.general.pattern_data_max_filter_items", 128, 32, 1024);

            ContentRegistry::Settings::add<Widgets::Checkbox>("hex.builtin.setting.general", "hex.builtin.setting.general.patterns", "hex.builtin.setting.general.auto_load_patterns", true);
            ContentRegistry::Settings::add<Widgets::Checkbox>("hex.builtin.setting.general", "hex.builtin.setting.general.patterns", "hex.builtin.setting.general.sync_pattern_source", false);

            ContentRegistry::Settings::add<Widgets::Checkbox>("hex.builtin.setting.general", "hex.builtin.setting.general.network", "hex.builtin.setting.general.network_interface", false);

            #if !defined(OS_WEB)
                ContentRegistry::Settings::add<ServerContactWidget>("hex.builtin.setting.general", "hex.builtin.setting.general.network", "hex.builtin.setting.general.server_contact");
                ContentRegistry::Settings::add<Widgets::Checkbox>("hex.builtin.setting.general", "hex.builtin.setting.general.network", "hex.builtin.setting.general.upload_crash_logs", true);
            #endif
        }

        /* Interface */
        {
            auto themeNames = ThemeManager::getThemeNames();
            std::vector<nlohmann::json> themeJsons = { };
            for (const auto &themeName : themeNames)
                themeJsons.emplace_back(themeName);

            themeNames.emplace(themeNames.begin(), ThemeManager::NativeTheme);
            themeJsons.emplace(themeJsons.begin(), ThemeManager::NativeTheme);

            ContentRegistry::Settings::add<Widgets::DropDown>("hex.builtin.setting.interface", "hex.builtin.setting.interface.style", "hex.builtin.setting.interface.color",
                                                              themeNames,
                                                              themeJsons,
                                                              "Dark").setChangedCallback([](auto &widget) {
                                                                  auto dropDown = static_cast<Widgets::DropDown *>(&widget);

                                                                  if (dropDown->getValue() == ThemeManager::NativeTheme)
                                                                      ImHexApi::System::enableSystemThemeDetection(true);
                                                                  else {
                                                                      ImHexApi::System::enableSystemThemeDetection(false);
                                                                      ThemeManager::changeTheme(dropDown->getValue());
                                                                  }
                                                              });

            ContentRegistry::Settings::add<Widgets::ColorPicker>(
                "hex.builtin.setting.interface", "hex.builtin.setting.interface.style", "hex.builtin.setting.interface.accent",
                ImGui::GetStyleColorVec4(ImGuiCol_Button),
                ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_NoInputs
            )
            .setChangedCallback([](auto &widget) {
                auto colorPicker = static_cast<Widgets::ColorPicker *>(&widget);
                ThemeManager::setAccentColor(colorPicker->getColor());
            });

            ContentRegistry::Settings::add<Widgets::Checkbox>("hex.builtin.setting.interface", "hex.builtin.setting.interface.style", "hex.builtin.setting.interface.show_titlebar_backdrop", true);

            ContentRegistry::Settings::add<ScalingWidget>("hex.builtin.setting.interface", "hex.builtin.setting.interface.style", "hex.builtin.setting.interface.scaling_factor")
            .requiresRestart();

            #if defined (OS_WEB)
                ContentRegistry::Settings::add<Widgets::Checkbox>("hex.builtin.setting.interface", "hex.builtin.setting.interface.style", "hex.builtin.setting.interface.crisp_scaling", false)
                .setChangedCallback([](Widgets::Widget &widget) {
                    auto checkBox = static_cast<Widgets::Checkbox *>(&widget);

                    EM_ASM({
                        var canvas = document.getElementById('canvas');
                        if ($0)
                            canvas.style.imageRendering = 'pixelated';
                        else
                            canvas.style.imageRendering = 'smooth';
                    }, checkBox->isChecked());
                });
            #endif

            ContentRegistry::Settings::add<Widgets::Checkbox>("hex.builtin.setting.interface", "hex.builtin.setting.interface.style", "hex.builtin.setting.interface.pattern_data_row_bg", false);
            ContentRegistry::Settings::add<Widgets::Checkbox>("hex.builtin.setting.interface", "hex.builtin.setting.interface.style", "hex.builtin.setting.interface.always_show_provider_tabs", false);
            ContentRegistry::Settings::add<Widgets::Checkbox>("hex.builtin.setting.interface", "hex.builtin.setting.interface.style", "hex.builtin.setting.interface.show_header_command_palette", true);
            ContentRegistry::Settings::add<Widgets::Checkbox>("hex.builtin.setting.interface", "hex.builtin.setting.interface.style", "hex.builtin.setting.interface.display_shortcut_highlights", true);

            std::vector<std::string> languageNames;
            std::vector<nlohmann::json> languageCodes;

            for (auto &[languageCode, languageName] : LocalizationManager::getSupportedLanguages()) {
                languageNames.emplace_back(languageName);
                languageCodes.emplace_back(languageCode);
            }

            ContentRegistry::Settings::add<Widgets::DropDown>("hex.builtin.setting.interface", "hex.builtin.setting.interface.language", "hex.builtin.setting.interface.language", languageNames, languageCodes, "en-US");

            ContentRegistry::Settings::add<Widgets::TextBox>("hex.builtin.setting.interface", "hex.builtin.setting.interface.language", "hex.builtin.setting.interface.wiki_explain_language", "en");
            ContentRegistry::Settings::add<FPSWidget>("hex.builtin.setting.interface", "hex.builtin.setting.interface.window", "hex.builtin.setting.interface.fps");

            #if defined (OS_LINUX)
                constexpr static auto MultiWindowSupportEnabledDefault = 0;
            #else
                constexpr static auto MultiWindowSupportEnabledDefault = 1;
            #endif

            ContentRegistry::Settings::add<Widgets::Checkbox>("hex.builtin.setting.interface", "hex.builtin.setting.interface.window", "hex.builtin.setting.interface.multi_windows", MultiWindowSupportEnabledDefault).requiresRestart();

            #if !defined(OS_WEB)
                ContentRegistry::Settings::add<Widgets::Checkbox>("hex.builtin.setting.interface", "hex.builtin.setting.interface.window", "hex.builtin.setting.interface.native_window_decorations", !getDefaultBorderlessWindowMode()).requiresRestart();
            #endif

            #if defined (OS_MACOS)
                ContentRegistry::Settings::add<Widgets::Checkbox>("hex.builtin.setting.interface", "hex.builtin.setting.interface.window", "hex.builtin.setting.interface.use_native_menu_bar", true);
            #endif

            ContentRegistry::Settings::add<Widgets::Checkbox>("hex.builtin.setting.interface", "hex.builtin.setting.interface.window", "hex.builtin.setting.interface.randomize_window_title", false);

            ContentRegistry::Settings::add<Widgets::Checkbox>("hex.builtin.setting.interface", "hex.builtin.setting.interface.window", "hex.builtin.setting.interface.restore_window_pos", false);

            ContentRegistry::Settings::add<Widgets::ColorPicker>("hex.builtin.setting.hex_editor", "", "hex.builtin.setting.hex_editor.highlight_color", ImColor(0x80, 0x80, 0xC0, 0x60));
            ContentRegistry::Settings::add<Widgets::Checkbox>("hex.builtin.setting.hex_editor", "", "hex.builtin.setting.hex_editor.sync_scrolling", false);
            ContentRegistry::Settings::add<Widgets::Checkbox>("hex.builtin.setting.hex_editor", "", "hex.builtin.setting.hex_editor.show_selection", false);
            ContentRegistry::Settings::add<Widgets::Checkbox>("hex.builtin.setting.hex_editor", "", "hex.builtin.setting.hex_editor.show_highlights", true);
            ContentRegistry::Settings::add<Widgets::SliderInteger>("hex.builtin.setting.hex_editor", "", "hex.builtin.setting.hex_editor.byte_padding", 0, 0, 50);
            ContentRegistry::Settings::add<Widgets::SliderInteger>("hex.builtin.setting.hex_editor", "", "hex.builtin.setting.hex_editor.char_padding", 0, 0, 50);

            ContentRegistry::Settings::add<Widgets::Checkbox>("hex.builtin.setting.hex_editor", "", "hex.builtin.setting.hex_editor.pattern_parent_highlighting", true);

            std::vector<std::string> pasteBehaviourNames = { "hex.builtin.setting.hex_editor.paste_behaviour.none", "hex.builtin.setting.hex_editor.paste_behaviour.everything", "hex.builtin.setting.hex_editor.paste_behaviour.selection" };
            std::vector<nlohmann::json> pasteBehaviourValues = { "none", "everything", "selection" };
            ContentRegistry::Settings::add<Widgets::DropDown>("hex.builtin.setting.hex_editor", "", "hex.builtin.setting.hex_editor.paste_behaviour",
                                                              pasteBehaviourNames,
                                                              pasteBehaviourValues,
                                                              "none");
        }

        /* Folders */
        {
            ContentRegistry::Settings::setCategoryDescription("hex.builtin.setting.folders", "hex.builtin.setting.folders.description");
            ContentRegistry::Settings::add<UserFolderWidget>("hex.builtin.setting.folders", "", "hex.builtin.setting.folders.description");
        }

        /* Proxy */
        {
            HttpRequest::setProxyUrl(ContentRegistry::Settings::read<std::string>("hex.builtin.setting.proxy", "hex.builtin.setting.proxy.url", ""));

            ContentRegistry::Settings::setCategoryDescription("hex.builtin.setting.proxy", "hex.builtin.setting.proxy.description");

            auto proxyEnabledSetting = ContentRegistry::Settings::add<Widgets::Checkbox>("hex.builtin.setting.proxy", "", "hex.builtin.setting.proxy.enable", false)
            .setChangedCallback([](Widgets::Widget &widget) {
                auto checkBox = static_cast<Widgets::Checkbox *>(&widget);

                HttpRequest::setProxyState(checkBox->isChecked());
            });

            ContentRegistry::Settings::add<Widgets::TextBox>("hex.builtin.setting.proxy", "", "hex.builtin.setting.proxy.url", "")
            .setEnabledCallback([proxyEnabledSetting] {
                auto &checkBox = static_cast<Widgets::Checkbox &>(proxyEnabledSetting.getWidget());

                return checkBox.isChecked();
            })
            .setChangedCallback([](Widgets::Widget &widget) {
                auto textBox = static_cast<Widgets::TextBox *>(&widget);

                HttpRequest::setProxyUrl(textBox->getValue());
            });
        }

        /* Experiments */
        {
            ContentRegistry::Settings::setCategoryDescription("hex.builtin.setting.experiments", "hex.builtin.setting.experiments.description");
            EventImHexStartupFinished::subscribe([]{
                for (const auto &[name, experiment] : ContentRegistry::Experiments::impl::getExperiments()) {
                    ContentRegistry::Settings::add<Widgets::Checkbox>("hex.builtin.setting.experiments", "", experiment.unlocalizedName, false)
                        .setTooltip(experiment.unlocalizedDescription)
                        .setChangedCallback([name](Widgets::Widget &widget) {
                            auto checkBox = static_cast<Widgets::Checkbox *>(&widget);

                            ContentRegistry::Experiments::enableExperiement(name, checkBox->isChecked());
                        });
                }
            });
        }

        /* Shorcuts */
        {
            EventImHexStartupFinished::subscribe([] {
                for (const auto &shortcutEntry : ShortcutManager::getGlobalShortcuts()) {
                    ContentRegistry::Settings::add<KeybindingWidget>(
                        "hex.builtin.setting.shortcuts",
                        "hex.builtin.setting.shortcuts.global",
                        shortcutEntry.unlocalizedName.back(),
                        nullptr,
                        shortcutEntry.shortcut,
                        shortcutEntry.unlocalizedName
                    );
                }

                for (auto &[viewName, view] : ContentRegistry::Views::impl::getEntries()) {
                    for (const auto &shortcutEntry : ShortcutManager::getViewShortcuts(view.get())) {
                        ContentRegistry::Settings::add<KeybindingWidget>(
                            "hex.builtin.setting.shortcuts",
                            viewName,
                            shortcutEntry.unlocalizedName.back(),
                            view.get(),
                            shortcutEntry.shortcut,
                            shortcutEntry.unlocalizedName
                        );
                    }
                }
           });
        }

        /* Toolbar icons */
        {
            ContentRegistry::Settings::setCategoryDescription("hex.builtin.setting.toolbar", "hex.builtin.setting.toolbar.description");

            ContentRegistry::Settings::add<ToolbarIconsWidget>("hex.builtin.setting.toolbar", "", "hex.builtin.setting.toolbar.icons");
        }

        ImHexApi::System::addMigrationRoutine("v1.37.0", [] {
            log::warn("Resetting shortcut key settings for them to work with this version of ImHex");

            for (const auto &category : ContentRegistry::Settings::impl::getSettings()) {
                for (const auto &subcategory : category.subCategories) {
                    for (const auto &entry : subcategory.entries) {
                        if (auto keybindingWidget = dynamic_cast<KeybindingWidget*>(entry.widget.get())) {
                            keybindingWidget->reset();
                            ContentRegistry::Settings::write<nlohmann::json>(category.unlocalizedName, entry.unlocalizedName, keybindingWidget->store());
                        }
                    }
                }
            }

            ContentRegistry::Settings::impl::store();
        });


        /* Plugins */
        {
            for (const auto &plugin : PluginManager::getPlugins()) {
                if (plugin.isLibraryPlugin())
                    continue;
                if (plugin.isBuiltinPlugin())
                    continue;

                auto interface = ContentRegistry::Settings::add<Widgets::Checkbox>("hex.builtin.setting.plugins", "hex.builtin.setting.loaded_plugins", plugin.getPluginName(), true)
                    .setTooltip(plugin.getPluginDescription())
                    .setChangedCallback([&plugin](Widgets::Widget &widget) {
                        auto checkBox = static_cast<Widgets::Checkbox *>(&widget);
                        PluginManager::setPluginEnabled(plugin, checkBox->isChecked());
                    })
                    .requiresRestart();

                PluginManager::setPluginEnabled(plugin, static_cast<Widgets::Checkbox *>(&interface.getWidget())->isChecked());
            }
        }
    }

    static void loadLayoutSettings() {
        const bool locked = ContentRegistry::Settings::read<bool>("hex.builtin.setting.interface", "hex.builtin.setting.interface.layout_locked", false);
        LayoutManager::lockLayout(locked);
    }

    static void loadThemeSettings() {
        auto theme = ContentRegistry::Settings::read<std::string>("hex.builtin.setting.interface", "hex.builtin.setting.interface.color", ThemeManager::NativeTheme);

        if (theme == ThemeManager::NativeTheme) {
            ImHexApi::System::enableSystemThemeDetection(true);
        } else {
            ImHexApi::System::enableSystemThemeDetection(false);
            ThemeManager::changeTheme(theme);
        }

        auto borderlessWindowMode = !ContentRegistry::Settings::read<bool>("hex.builtin.setting.interface", "hex.builtin.setting.interface.native_window_decorations", !getDefaultBorderlessWindowMode());
        ImHexApi::System::impl::setBorderlessWindowMode(borderlessWindowMode);
    }

    static void loadFolderSettings() {
        auto folderPathStrings = ContentRegistry::Settings::read<std::vector<std::string>>("hex.builtin.setting.folders", "hex.builtin.setting.folders", { });

        std::vector<std::fs::path> paths;
        for (const auto &pathString : folderPathStrings) {
            paths.emplace_back(pathString);
        }

        ImHexApi::System::setAdditionalFolderPaths(paths);
    }

    void loadSettings() {
        loadLayoutSettings();
        loadThemeSettings();
        loadFolderSettings();
    }

}
