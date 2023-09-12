#include "content/views/view_bookmarks.hpp"

#include <hex/api/content_registry.hpp>
#include <hex/api/project_file_manager.hpp>
#include <hex/api/achievement_manager.hpp>
#include <hex/api/task.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/helpers/utils.hpp>

#include <hex/providers/provider.hpp>
#include <content/providers/view_provider.hpp>

#include <nlohmann/json.hpp>

#include <wolv/io/file.hpp>
#include <wolv/utils/guards.hpp>

namespace hex::plugin::builtin {

    ViewBookmarks::ViewBookmarks() : View("hex.builtin.view.bookmarks.name") {

        // Handle bookmark add requests sent by the API
        EventManager::subscribe<RequestAddBookmark>(this, [this](Region region, std::string name, std::string comment, color_t color) {
            if (name.empty()) {
                name = hex::format("hex.builtin.view.bookmarks.default_title"_lang, region.address, region.address + region.size - 1);
            }

            if (color == 0x00)
                color = ImGui::GetColorU32(ImGuiCol_Header);

            this->m_bookmarks->push_back({
                region,
                name,
                std::move(comment),
                color,
                false
            });

            ImHexApi::Provider::markDirty();

            EventManager::post<EventBookmarkCreated>(this->m_bookmarks->back());
            EventManager::post<EventHighlightingChanged>();
        });

        // Draw hex editor background highlights for bookmarks
        ImHexApi::HexEditor::addBackgroundHighlightingProvider([this](u64 address, const u8* data, size_t size, bool) -> std::optional<color_t> {
            hex::unused(data);

            // Check all bookmarks for potential overlaps with the current address
            for (const auto &bookmark : *this->m_bookmarks) {
                if (Region { address, size }.isWithin(bookmark.region))
                    return bookmark.color;
            }

            return std::nullopt;
        });

        // Draw hex editor tooltips for bookmarks
        ImHexApi::HexEditor::addTooltipProvider([this](u64 address, const u8 *data, size_t size) {
            hex::unused(data);

            // Loop over all bookmarks
            for (const auto &bookmark : *this->m_bookmarks) {
                // Make sure the bookmark overlaps the currently hovered address
                if (!Region { address, size }.isWithin(bookmark.region))
                    continue;

                // Draw tooltip
                ImGui::BeginTooltip();

                ImGui::PushID(&bookmark);
                if (ImGui::BeginTable("##tooltips", 1, ImGuiTableFlags_RowBg | ImGuiTableFlags_NoClip)) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    {
                        // Draw bookmark header
                        ImGui::ColorButton("##color", ImColor(bookmark.color));
                        ImGui::SameLine(0, 10);
                        ImGui::TextFormatted("{} ", bookmark.name);

                        // Draw extra information table when holding down shift
                        if (ImGui::GetIO().KeyShift) {
                            ImGui::Indent();
                            if (ImGui::BeginTable("##extra_info", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_NoClip)) {

                                ImGui::TableNextRow();
                                ImGui::TableNextColumn();

                                // Draw region
                                ImGui::TableNextRow();
                                ImGui::TableNextColumn();
                                ImGui::TextFormatted("{}: ", "hex.builtin.common.region"_lang.get());
                                ImGui::TableNextColumn();
                                ImGui::TextFormatted("[ 0x{:08X} - 0x{:08X} ] ", bookmark.region.getStartAddress(), bookmark.region.getEndAddress());

                                // Draw comment if it's not empty
                                if (!bookmark.comment.empty() && bookmark.comment[0] != '\x00') {
                                    ImGui::TableNextRow();
                                    ImGui::TableNextColumn();
                                    ImGui::TextFormatted("{}: ", "hex.builtin.view.bookmarks.header.comment"_lang.get());
                                    ImGui::TableNextColumn();
                                    ImGui::PushTextWrapPos(ImGui::CalcTextSize("X").x * 40);
                                    ImGui::TextFormattedWrapped("{}", bookmark.comment);
                                    ImGui::PopTextWrapPos();
                                }

                                ImGui::EndTable();
                            }
                            ImGui::Unindent();
                        }
                    }


                    ImGui::PushStyleColor(ImGuiCol_TableRowBg, bookmark.color);
                    ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, bookmark.color);
                    ImGui::EndTable();
                    ImGui::PopStyleColor(2);
                }
                ImGui::PopID();

                ImGui::EndTooltip();
            }
        });

        // Handle saving / restoring of bookmarks in projects
        ProjectFile::registerPerProviderHandler({
            .basePath = "bookmarks.json",
            .required = false,
            .load = [this](prv::Provider *provider, const std::fs::path &basePath, Tar &tar) -> bool {
                auto fileContent = tar.readString(basePath);
                if (fileContent.empty())
                    return true;

                auto data = nlohmann::json::parse(fileContent.begin(), fileContent.end());
                this->m_bookmarks.get(provider).clear();
                return this->importBookmarks(provider, data);
            },
            .store = [this](prv::Provider *provider, const std::fs::path &basePath, Tar &tar) -> bool {
                nlohmann::json data;

                bool result = this->exportBookmarks(provider, data);
                tar.writeString(basePath, data.dump(4));

                return result;
            }
        });

        this->registerMenuItems();
    }

    ViewBookmarks::~ViewBookmarks() {
        EventManager::unsubscribe<RequestAddBookmark>(this);
        EventManager::unsubscribe<EventProviderDeleted>(this);
    }

    static void drawColorPopup(ImColor &color) {
        // Generate color picker palette
        static auto Palette = [] {
            constexpr static auto ColorCount = 36;
            std::array<ImColor, ColorCount> result = { 0 };

            u32 counter = 0;
            for (auto &color : result) {
                ImGui::ColorConvertHSVtoRGB(float(counter) / float(ColorCount - 1), 0.8F, 0.8F, color.Value.x, color.Value.y, color.Value.z);
                color.Value.w = 0.7F;

                counter++;
            }

            return result;
        }();

        // Draw default color picker
        ImGui::ColorPicker4("##picker", (float*)&color, ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoOptions | ImGuiColorEditFlags_NoSmallPreview);

        ImGui::Separator();

        // Draw color palette
        int id = 0;
        for (const auto &paletteColor : Palette) {
            ImGui::PushID(id);
            if ((id % 9) != 0)
                ImGui::SameLine(0.0F, ImGui::GetStyle().ItemSpacing.y);

            constexpr static ImGuiColorEditFlags flags = ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_NoDragDrop;
            if (ImGui::ColorButton("##palette", paletteColor.Value, flags, ImVec2(20, 20))) {
                color = paletteColor;
            }

            ImGui::PopID();
            id++;
        }
    }

    void ViewBookmarks::drawContent() {
        if (ImGui::Begin(View::toWindowName("hex.builtin.view.bookmarks.name").c_str(), &this->getWindowOpenState())) {
            auto provider = ImHexApi::Provider::get();

            // Draw filter input
            ImGui::PushItemWidth(-1);
            ImGui::InputTextIcon("##filter", ICON_VS_FILTER, this->m_currFilter);
            ImGui::PopItemWidth();

            ImGui::NewLine();

            if (ImGui::BeginChild("##bookmarks")) {
                if (this->m_bookmarks->empty()) {
                    ImGui::TextFormattedCentered("hex.builtin.view.bookmarks.no_bookmarks"_lang);
                }

                int id = 1;
                auto bookmarkToRemove = this->m_bookmarks->end();

                // Draw all bookmarks
                for (auto iter = this->m_bookmarks->begin(); iter != this->m_bookmarks->end(); iter++) {
                    auto &[region, name, comment, color, locked] = *iter;

                    // Apply filter
                    if (!this->m_currFilter.empty()) {
                        if (!name.contains(this->m_currFilter) && !comment.contains(this->m_currFilter))
                            continue;
                    }

                    auto headerColor = ImColor(color);
                    auto hoverColor  = ImColor(color);
                    hoverColor.Value.w *= 1.3F;

                    // Draw bookmark header in the same color as the bookmark was set to
                    ImGui::PushID(id);
                    ImGui::PushStyleColor(ImGuiCol_Header, color);
                    ImGui::PushStyleColor(ImGuiCol_HeaderActive, color);
                    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, u32(hoverColor));

                    ON_SCOPE_EXIT {
                        ImGui::PopID();
                        ImGui::PopStyleColor(3);
                        id++;
                    };

                    bool open = true;
                    if (!ImGui::CollapsingHeader(hex::format("{}###bookmark", name).c_str(), locked ? nullptr : &open)) {
                        // Handle dragging bookmarks up and down when they're collapsed

                        // Set the currently held bookmark as the one being dragged
                        if (ImGui::IsMouseClicked(0) && ImGui::IsItemActivated() && this->m_dragStartIterator == this->m_bookmarks->end())
                            this->m_dragStartIterator = iter;

                        // When the mouse moved away from the current bookmark, swap the dragged bookmark with the current one
                        if (ImGui::IsItemHovered() && this->m_dragStartIterator != this->m_bookmarks->end()) {
                            std::iter_swap(iter, this->m_dragStartIterator);
                            this->m_dragStartIterator = iter;
                        }

                        // When the mouse is released, reset the dragged bookmark
                        if (!ImGui::IsMouseDown(0))
                            this->m_dragStartIterator = this->m_bookmarks->end();
                    } else {
                        const auto rowHeight = ImGui::GetTextLineHeightWithSpacing() + 2 * ImGui::GetStyle().FramePadding.y;
                        if (ImGui::BeginTable("##bookmark_table", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
                            ImGui::TableSetupColumn("##name");
                            ImGui::TableSetupColumn("##spacing", ImGuiTableColumnFlags_WidthFixed, 20);
                            ImGui::TableSetupColumn("##value", ImGuiTableColumnFlags_WidthStretch);

                            ImGui::TableNextRow(ImGuiTableRowFlags_None, rowHeight);
                            ImGui::TableNextColumn();

                            // Draw bookmark name
                            ImGui::TextUnformatted("hex.builtin.view.bookmarks.header.name"_lang);
                            ImGui::TableNextColumn();
                            ImGui::TableNextColumn();

                            // Draw lock/unlock button
                            if (locked) {
                                if (ImGui::IconButton(ICON_VS_LOCK, ImGui::GetStyleColorVec4(ImGuiCol_Text))) locked = false;
                                ImGui::InfoTooltip("hex.builtin.view.bookmarks.tooltip.unlock"_lang);
                            } else {
                                if (ImGui::IconButton(ICON_VS_UNLOCK, ImGui::GetStyleColorVec4(ImGuiCol_Text))) locked = true;
                                ImGui::InfoTooltip("hex.builtin.view.bookmarks.tooltip.lock"_lang);
                            }

                            ImGui::SameLine();

                            // Draw color button
                            if (ImGui::ColorButton("hex.builtin.view.bookmarks.header.color"_lang, headerColor.Value, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoAlpha)) {
                                if (!locked)
                                    ImGui::OpenPopup("hex.builtin.view.bookmarks.header.color"_lang);
                            }
                            ImGui::InfoTooltip("hex.builtin.view.bookmarks.header.color"_lang);

                            // Draw color picker
                            if (ImGui::BeginPopup("hex.builtin.view.bookmarks.header.color"_lang)) {
                                drawColorPopup(headerColor);
                                color = headerColor;
                                ImGui::EndPopup();
                            }

                            ImGui::SameLine();

                            // Draw bookmark name if the bookmark is locked or an input text box if it's unlocked
                            if (locked)
                                ImGui::TextUnformatted(name.data());
                            else {
                                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
                                ImGui::InputText("##nameInput", name);
                                ImGui::PopItemWidth();
                            }

                            ImGui::TableNextRow(ImGuiTableRowFlags_None, rowHeight);
                            ImGui::TableNextColumn();

                            ImGui::TextUnformatted("hex.builtin.common.address"_lang);
                            ImGui::TableNextColumn();
                            ImGui::TableNextColumn();

                            // Draw jump to address button
                            if (ImGui::IconButton(ICON_VS_DEBUG_STEP_BACK, ImGui::GetStyleColorVec4(ImGuiCol_Text)))
                                ImHexApi::HexEditor::setSelection(region);
                            ImGui::InfoTooltip("hex.builtin.view.bookmarks.tooltip.jump_to"_lang);

                            ImGui::SameLine();

                            // Draw open in new view button
                            if (ImGui::IconButton(ICON_VS_GO_TO_FILE, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
                                TaskManager::doLater([region, provider]{
                                    auto newProvider = ImHexApi::Provider::createProvider("hex.builtin.provider.view", true);
                                    if (auto *viewProvider = dynamic_cast<ViewProvider*>(newProvider); viewProvider != nullptr) {
                                        viewProvider->setProvider(region.getStartAddress(), region.getSize(), provider);
                                        if (viewProvider->open()) {
                                            EventManager::post<EventProviderOpened>(viewProvider);
                                            AchievementManager::unlockAchievement("hex.builtin.achievement.hex_editor", "hex.builtin.achievement.hex_editor.open_new_view.name");
                                        }
                                    }
                                });
                            }
                            ImGui::InfoTooltip("hex.builtin.view.bookmarks.tooltip.open_in_view"_lang);

                            ImGui::SameLine();

                            // Draw the address of the bookmark
                            ImGui::TextFormatted("hex.builtin.view.bookmarks.address"_lang, region.getStartAddress(), region.getEndAddress());

                            ImGui::TableNextRow(ImGuiTableRowFlags_None, rowHeight);
                            ImGui::TableNextColumn();

                            // Draw size of the bookmark
                            ImGui::TextUnformatted("hex.builtin.common.size"_lang);
                            ImGui::TableNextColumn();
                            ImGui::TableNextColumn();
                            ImGui::TextFormatted(hex::toByteString(region.size));

                            ImGui::EndTable();
                        }

                        // Draw comment if the bookmark is locked or an input text box if it's unlocked
                        if (locked) {
                            if (!comment.empty()) {
                                ImGui::Header("hex.builtin.view.bookmarks.header.comment"_lang);
                                ImGui::TextFormattedWrapped("{}", comment.data());
                            }
                        }
                        else {
                            ImGui::Header("hex.builtin.view.bookmarks.header.comment"_lang);
                            ImGui::InputTextMultiline("##commentInput", comment, ImVec2(ImGui::GetContentRegionAvail().x, 150_scaled));
                        }

                        ImGui::NewLine();
                    }

                    // Mark a bookmark for removal when the user clicks the remove button
                    if (!open)
                        bookmarkToRemove = iter;
                }

                // Remove the bookmark that was marked for removal
                if (bookmarkToRemove != this->m_bookmarks->end()) {
                    this->m_bookmarks->erase(bookmarkToRemove);
                    EventManager::post<EventHighlightingChanged>();
                }
            }
            ImGui::EndChild();
        }
        ImGui::End();
    }

    bool ViewBookmarks::importBookmarks(prv::Provider *provider, const nlohmann::json &json) {
        if (!json.contains("bookmarks"))
            return false;

        for (const auto &bookmark : json["bookmarks"]) {
            if (!bookmark.contains("name") || !bookmark.contains("comment") || !bookmark.contains("color") || !bookmark.contains("region") || !bookmark.contains("locked"))
                continue;

            const auto &region = bookmark["region"];
            if (!region.contains("address") || !region.contains("size"))
                continue;

            this->m_bookmarks.get(provider).push_back({
                .region = { region["address"], region["size"] },
                .name = bookmark["name"],
                .comment = bookmark["comment"],
                .color = bookmark["color"],
                .locked = bookmark["locked"]
            });
        }

        return true;
    }

    bool ViewBookmarks::exportBookmarks(prv::Provider *provider, nlohmann::json &json) {
        json["bookmarks"] = nlohmann::json::array();
        size_t index = 0;
        for (const auto &bookmark : this->m_bookmarks.get(provider)) {
            json["bookmarks"][index] = {
                    { "name", bookmark.name },
                    { "comment", bookmark.comment },
                    { "color", bookmark.color },
                    { "region", {
                            { "address", bookmark.region.address },
                            { "size", bookmark.region.size }
                        }
                    },
                    { "locked", bookmark.locked }
            };
            index++;
        }

        return true;
    }

    void ViewBookmarks::registerMenuItems() {
        /* Create bookmark */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.edit", "hex.builtin.menu.edit.bookmark.create" }, 1900, CTRLCMD + Keys::B, [&] {
            if (!ImHexApi::HexEditor::isSelectionValid())
                return;

            auto selection = ImHexApi::HexEditor::getSelection();
            ImHexApi::Bookmarks::add(selection->getStartAddress(), selection->getSize(), {}, {});
        }, []{ return ImHexApi::Provider::isValid() && ImHexApi::HexEditor::isSelectionValid(); });


        ContentRegistry::Interface::addMenuItemSeparator({ "hex.builtin.menu.file", "hex.builtin.menu.file.import" }, 3000);

        /* Import bookmarks */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.menu.file.import", "hex.builtin.menu.file.import.bookmark" }, 3050, Shortcut::None, [this]{
            fs::openFileBrowser(fs::DialogMode::Open, { { "Bookmarks File", "hexbm"} }, [&, this](const std::fs::path &path) {
                try {
                    this->importBookmarks(ImHexApi::Provider::get(), nlohmann::json::parse(wolv::io::File(path, wolv::io::File::Mode::Read).readString()));
                } catch (...) { }
            });
        }, ImHexApi::Provider::isValid);

        ContentRegistry::Interface::addMenuItemSeparator({ "hex.builtin.menu.file", "hex.builtin.menu.file.export" }, 6200);


        /* Export bookmarks */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.menu.file.export", "hex.builtin.menu.file.export.bookmark" }, 6250, Shortcut::None, [this]{
            fs::openFileBrowser(fs::DialogMode::Save, { { "Bookmarks File", "hexbm"} }, [&, this](const std::fs::path &path) {
                nlohmann::json json;
                this->exportBookmarks(ImHexApi::Provider::get(), json);

                wolv::io::File(path, wolv::io::File::Mode::Create).writeString(json.dump(4));
            });
        }, [this]{
            return ImHexApi::Provider::isValid() && !this->m_bookmarks->empty();
        });
    }

}