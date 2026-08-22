#include "content/views/view_bookmarks.hpp"

#include <hex/api/content_registry/reports.hpp>
#include <hex/api/content_registry/views.hpp>
#include <hex/api/content_registry/user_interface.hpp>
#include <hex/api/achievement_manager.hpp>
#include <hex/api/task_manager.hpp>
#include <hex/api/events/requests_interaction.hpp>
#include <hex/api/events/events_interaction.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/providers/provider.hpp>

#include <content/providers/view_provider.hpp>

#include <fonts/vscode_icons.hpp>

#include <nlohmann/json.hpp>

#include <wolv/io/file.hpp>
#include <wolv/utils/guards.hpp>
#include "imgui_internal.h"

namespace hex::plugin::builtin {

    ViewBookmarks::ViewBookmarks()
        : View::Window("hex.builtin.view.bookmarks.name"_unlocalized, ICON_VS_BOOKMARK),
          m_bookmarks({
              .typeId = "hex.builtin.bookmarks",
              .displayName = "hex.builtin.view.bookmarks.name"_unlocalized,
              .displayIcon = ICON_VS_BOOKMARK,
              .extensions = { { "Bookmarks File", "hexbm" } },
              .encode = &ViewBookmarks::encodeBookmarks,
              .decode = &ViewBookmarks::decodeBookmarks
          }) {

        m_bookmarks.setChangedCallback([this](prv::Provider *provider) {
            this->refreshBookmarkState(provider);
        });

        // Handle bookmark add requests sent by the API
        RequestAddBookmark::subscribe(this, [this](Region region, std::string name, std::string comment, color_t color, u64 *id) {
            if (name.empty()) {
                name = fmt::format("hex.builtin.view.bookmarks.default_title"_lang, region.address, region.address + region.size - 1);
            }

            if (color == 0x00)
                color = ImGui::GetColorU32(ImGuiCol_Header);

            m_currBookmarkId += 1;
            u64 bookmarkId = m_currBookmarkId;
            if (id != nullptr)
                *id = bookmarkId;

            auto bookmark = ImHexApi::Bookmarks::Entry {
                .region=region,
                .name=name,
                .comment=std::move(comment),
                .color=color,
                .locked=true,
                .id=bookmarkId
            };

            auto commentDisplay = ui::Markdown(bookmark.comment);
            m_bookmarks->emplace_back(std::move(bookmark), true, std::move(commentDisplay));
            m_bookmarks.markChanged();

            EventBookmarkCreated::post(m_bookmarks->back().entry);
            EventHighlightingChanged::post();
        });

        RequestRemoveBookmark::subscribe([this](u64 id) {
            if (std::erase_if(m_bookmarks.get(), [id](const auto &bookmark) {
                return bookmark.entry.id == id;
            }) > 0)
                m_bookmarks.markChanged();
        });

        // Draw hex editor background highlights for bookmarks
        ImHexApi::HexEditor::addBackgroundHighlightingProvider([this](u64 address, const u8* data, size_t size, bool) -> std::optional<color_t> {
            std::ignore = data;

            // Check all bookmarks for potential overlaps with the current address
            std::optional<ImColor> color;
            for (const auto &bookmark : *m_bookmarks) {
                if (!bookmark.highlightVisible)
                    continue;

                if (Region { .address=address, .size=size }.isWithin(bookmark.entry.region)) {
                    color = blendColors(color, bookmark.entry.color);
                }
            }

            return color;
        });

        // Draw hex editor tooltips for bookmarks
        ImHexApi::HexEditor::addTooltipProvider([this](u64 address, const u8 *data, size_t size) {
            std::ignore = data;

            // Loop over all bookmarks
            for (auto &[bookmark, highlightVisible, commentDisplay] : *m_bookmarks) {
                if (!highlightVisible)
                    continue;

                // Make sure the bookmark overlaps the currently hovered address
                if (!Region { .address=address, .size=size }.isWithin(bookmark.region))
                    continue;

                // Draw tooltip
                ImGui::BeginTooltip();

                ImGui::PushID(&bookmark);
                if (ImGui::BeginTable("##tooltips", 1, ImGuiTableFlags_RowBg | ImGuiTableFlags_NoClip)) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    {
                        // Draw bookmark header
                        ImGui::ColorButton("##color", ImColor(bookmark.color), ImGuiColorEditFlags_AlphaOpaque);
                        ImGui::SameLine(0, 10);
                        ImGuiExt::TextFormatted("{} ", bookmark.name);

                        // Draw extra information table when holding down shift
                        if (ImGui::GetIO().KeyShift) {
                            ImGui::Indent();
                            if (ImGui::BeginTable("##extra_info", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_NoClip)) {

                                ImGui::TableNextRow();
                                ImGui::TableNextColumn();

                                // Draw region
                                ImGui::TableNextRow();
                                ImGui::TableNextColumn();
                                ImGuiExt::TextFormatted("{}: ", "hex.ui.common.region"_lang.get());
                                ImGui::TableNextColumn();
                                ImGuiExt::TextFormatted("[ 0x{:08X} - 0x{:08X} ] ", bookmark.region.getStartAddress(), bookmark.region.getEndAddress());

                                // Draw comment if it's not empty
                                if (!bookmark.comment.empty() && bookmark.comment[0] != '\x00') {
                                    ImGui::TableNextRow();
                                    ImGui::TableNextColumn();
                                    ImGuiExt::TextFormatted("{}: ", "hex.builtin.view.bookmarks.header.comment"_lang.get());
                                    ImGui::TableNextColumn();
                                    ImGui::PushTextWrapPos(ImGui::CalcTextSize("X").x * 40);
                                    commentDisplay.draw();
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

        ContentRegistry::Reports::addReportProvider([this](prv::Provider *provider) -> std::string {
            std::string result;

            const auto &bookmarks = m_bookmarks.get(provider);
            if (bookmarks.empty())
                return "";

            result += "## Bookmarks\n\n";

            for (const auto &[bookmark, highlightVisible, commentDisplay] : bookmarks) {
                result += fmt::format("### <span style=\"background-color: #{:06X}80\">{} [0x{:04X} - 0x{:04X}]</span>\n\n", hex::changeEndianness(bookmark.color, std::endian::big) >> 8, bookmark.name, bookmark.region.getStartAddress(), bookmark.region.getEndAddress());

                for (const auto &line : wolv::util::splitString(bookmark.comment, "\n"))
                    result += fmt::format("> {}\n", line);
                result += "\n";

                result += "```\n";
                result += hex::generateHexView(bookmark.region.getStartAddress(), bookmark.region.getSize(), provider);
                result += "\n```\n\n";
            }

            return result;
        });

        this->registerMenuItems();
    }

    ViewBookmarks::~ViewBookmarks() {
        RequestAddBookmark::unsubscribe(this);
        EventProviderDeleted::unsubscribe(this);
    }

    static void drawColorPopup(ImColor &color) {
        // Generate color picker palette
        const static auto Palette = [] {
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

        bool colorChanged = false;

        // Draw default color picker
        if (ImGui::ColorPicker4("##picker", &color.Value.x, ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoOptions | ImGuiColorEditFlags_NoSmallPreview))
            colorChanged = true;

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
                colorChanged = true;
            }

            ImGui::PopID();
            id++;
        }

        if (colorChanged)
            EventHighlightingChanged::post();
    }

    void ViewBookmarks::drawDropTarget(Bookmarks::iterator it, float height) {
        height = std::max(height, 1.0F);

        if (it != m_bookmarks->begin()) {
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - height);
        } else {
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + height);
        }

        ImGui::InvisibleButton("##DropTarget", ImVec2(std::max(1.0F, ImGui::GetContentRegionAvail().x), height * 2.0F));
        const auto dropTarget = ImRect(ImGui::GetItemRectMin(), ImVec2(ImGui::GetItemRectMax().x, ImGui::GetItemRectMin().y + 2_scaled));

        if (it == m_bookmarks->begin()) {
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - height);
        }

        ImGui::PushStyleColor(ImGuiCol_DragDropTarget, 0x00);
        if (ImGui::BeginDragDropTarget()) {
            ImGui::GetWindowDrawList()->AddRectFilled(dropTarget.Min, dropTarget.Max, ImGui::GetColorU32(ImGuiCol_ButtonActive));

            if (auto payload = ImGui::AcceptDragDropPayload("BOOKMARK_PAYLOAD"); payload != nullptr) {
                // Receive the bookmark id from the payload
                u64 droppedBookmarkId = *static_cast<const u64*>(payload->Data);

                // Find the correct bookmark with that id
                auto droppedIter = std::ranges::find_if(m_bookmarks->begin(), m_bookmarks->end(), [droppedBookmarkId](const auto &bookmarkItem) {
                    return bookmarkItem.entry.id == droppedBookmarkId;
                });

                // Swap the two bookmarks
                if (droppedIter != m_bookmarks->end()) {
                    m_bookmarks->splice(it, m_bookmarks, droppedIter);
                    m_bookmarks.markChanged();

                    EventHighlightingChanged::post();
                }
            }

            ImGui::EndDragDropTarget();
        }
        ImGui::PopStyleColor();
    }

    void ViewBookmarks::drawContent() {
        // Draw filter input
        ImGui::PushItemWidth(-1);
        ImGuiExt::InputTextIcon("##filter", ICON_VS_FILTER, m_currFilter);
        ImGui::PopItemWidth();

        ImGui::NewLine();

        ImGui::PushStyleVarY(ImGuiStyleVar_WindowPadding, -5_scaled);
        bool open = ImGuiExt::BeginSubWindow("", nullptr, ImGui::GetContentRegionAvail());
        ImGui::PopStyleVar();
        if (open) {
            if (m_bookmarks->empty()) {
                ImGuiExt::TextOverlay("hex.builtin.view.bookmarks.no_bookmarks"_lang, ImGui::GetWindowPos() + ImGui::GetWindowSize() / 2, ImGui::GetWindowWidth() * 0.7);
            }

            auto bookmarkToRemove = m_bookmarks->end();
            const auto defaultItemSpacing = ImGui::GetStyle().ItemSpacing.y;

            drawDropTarget(m_bookmarks->begin(), defaultItemSpacing);

            // Draw all bookmarks
            for (auto it = m_bookmarks->begin(); it != m_bookmarks->end(); ++it) {
                auto &[bookmark, highlightVisible, commentDisplay] = *it;
                auto &[region, name, comment, color, locked, bookmarkId] = bookmark;

                // Apply filter
                if (!m_currFilter.empty()) {
                    if (!name.contains(m_currFilter) && !comment.contains(m_currFilter))
                        continue;
                }

                auto headerColor = ImColor(color);
                auto hoverColor  = ImColor(color);
                hoverColor.Value.w *= 1.3F;

                // Draw bookmark header in the same color as the bookmark was set to
                ImGui::PushID(bookmarkId);
                ImGui::PushStyleColor(ImGuiCol_Header, color);
                ImGui::PushStyleColor(ImGuiCol_HeaderActive, color);
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, u32(hoverColor));

                ON_SCOPE_EXIT {
                    ImGui::PopStyleColor(3);
                    ImGui::PopID();
                };

                bool notDeleted = true;

                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2());
                auto expanded = ImGui::CollapsingHeader(fmt::format("{}###bookmark", name).c_str(), &notDeleted);
                ImGui::PopStyleVar();

                if (!expanded) {
                    // Handle dragging bookmarks up and down when they're collapsed

                    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoHoldToOpenOthers | ImGuiDragDropFlags_SourceAllowNullID)) {
                        // Set the payload to the bookmark id
                        ImGui::SetDragDropPayload("BOOKMARK_PAYLOAD", &bookmarkId, sizeof(bookmarkId));

                        // Draw drag and drop tooltip
                        ImGui::ColorButton("##color", headerColor.Value, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaOpaque);
                        ImGui::SameLine();
                        ImGuiExt::TextFormatted("{}", name);

                        if (!comment.empty()) {
                            ImGui::Separator();
                            ImGui::PushTextWrapPos(300_scaled);
                            commentDisplay.draw();
                            ImGui::PopTextWrapPos();
                        }

                        ImGui::EndDragDropSource();
                    }
                }

                auto nextPos = ImGui::GetCursorPos();

                ImGui::SameLine();
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - 100_scaled);

                {
                    ImGui::PushStyleColor(ImGuiCol_Button, 0x00);
                    // Draw jump to region button
                    if (ImGuiExt::IconButton(ICON_VS_DEBUG_STEP_BACK, ImGui::GetStyleColorVec4(ImGuiCol_Text)))
                        ImHexApi::HexEditor::setSelection(region);
                    ImGui::SetItemTooltip("%s", "hex.builtin.view.bookmarks.tooltip.jump_to"_lang.get());

                    ImGui::SameLine(0, 0);

                    // Draw open in new view button
                    if (ImGuiExt::IconButton(ICON_VS_GO_TO_FILE, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
                        auto provider = ImHexApi::Provider::get();
                        TaskManager::doLater([region, provider, name]{
                            auto newProvider = ImHexApi::Provider::createProvider("hex.builtin.provider.view"_unlocalized, true);
                            if (auto *viewProvider = dynamic_cast<ViewProvider*>(newProvider.get()); viewProvider != nullptr) {
                                viewProvider->setProvider(region.getStartAddress(), region.getSize(), provider);
                                viewProvider->setName(fmt::format("'{}' View", name));

                                ImHexApi::Provider::openProvider(newProvider);

                                AchievementManager::unlockAchievement("hex.builtin.achievement.hex_editor"_unlocalized, "hex.builtin.achievement.hex_editor.open_new_view.name"_unlocalized);
                            }
                        });
                    }
                    ImGui::SetItemTooltip("%s", "hex.builtin.view.bookmarks.tooltip.open_in_view"_lang.get());

                    ImGui::SameLine(0, 0);

                    // Draw highlight visible toggle
                    if (ImGuiExt::IconButton(highlightVisible ? ICON_VS_EYE : ICON_VS_EYE_CLOSED, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
                        highlightVisible = !highlightVisible;
                        m_bookmarks.markChanged();
                        EventHighlightingChanged::post();
                    }

                    ImGui::PopStyleColor();
                }

                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2());
                drawDropTarget(std::next(it), defaultItemSpacing);
                ImGui::PopStyleVar();

                ImGui::SetCursorPos(nextPos);
                ImGui::Dummy({});

                if (expanded) {
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
                        if (ImGuiExt::DimmedIconToggle(ICON_VS_LOCK, ICON_VS_UNLOCK, &locked))
                            m_bookmarks.markChanged();
                        if (locked)
                            ImGuiExt::InfoTooltip("hex.builtin.view.bookmarks.tooltip.unlock"_lang);
                        else
                            ImGuiExt::InfoTooltip("hex.builtin.view.bookmarks.tooltip.lock"_lang);

                        ImGui::SameLine();

                        // Draw color button
                        if (ImGui::ColorButton("hex.builtin.view.bookmarks.header.color"_lang, headerColor.Value, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoAlpha)) {
                            if (!locked)
                                ImGui::OpenPopup("hex.builtin.view.bookmarks.header.color"_lang);
                        }
                        ImGuiExt::InfoTooltip("hex.builtin.view.bookmarks.header.color"_lang);

                        // Draw color picker
                        if (ImGui::BeginPopup("hex.builtin.view.bookmarks.header.color"_lang)) {
                            const auto oldColor = color;
                            drawColorPopup(headerColor);
                            color = headerColor;
                            if (color != oldColor)
                                m_bookmarks.markChanged();
                            ImGui::EndPopup();
                        }

                        ImGui::SameLine();

                        // Draw bookmark name if the bookmark is locked or an input text box if it's unlocked
                        if (locked) {
                            ImGui::TextUnformatted(name.data());
                        } else {
                            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
                            if (ImGui::InputText("##nameInput", name))
                                m_bookmarks.markChanged();
                            ImGui::PopItemWidth();
                        }

                        ImGui::TableNextRow(ImGuiTableRowFlags_None, rowHeight);
                        ImGui::TableNextColumn();

                        ImGui::TextUnformatted("hex.ui.common.address"_lang);
                        ImGui::TableNextColumn();
                        ImGui::TableNextColumn();

                        // Draw the address of the bookmark
                        u64 begin = region.getStartAddress();
                        u64 end   = region.getEndAddress();

                        if (!locked) {
                            bool updated = false;

                            ImGui::PushItemWidth(100_scaled);
                            if (ImGuiExt::InputHexadecimal("##begin", &begin))
                                updated = true;

                            ImGui::SameLine(0, 0);
                            ImGui::TextUnformatted(" - ");
                            ImGui::SameLine(0, 0);

                            if (ImGuiExt::InputHexadecimal("##end", &end))
                                updated = true;

                            ImGui::PopItemWidth();

                            if (updated && end >= begin) {
                                region = Region(begin, end - begin + 1);
                                m_bookmarks.markChanged();
                                EventHighlightingChanged::post();
                            }
                        } else {
                            ImGuiExt::TextFormatted("0x{:02X} - 0x{:02X}", begin, end);
                        }

                        ImGui::TableNextRow(ImGuiTableRowFlags_None, rowHeight);
                        ImGui::TableNextColumn();

                        // Draw size of the bookmark
                        ImGui::TextUnformatted("hex.ui.common.size"_lang);
                        ImGui::TableNextColumn();
                        ImGui::TableNextColumn();
                        ImGuiExt::TextFormatted(hex::toByteString(region.size));

                        ImGui::EndTable();
                    }

                    if (!locked || (locked && !comment.empty())) {
                        if (ImGuiExt::BeginSubWindow("hex.builtin.view.bookmarks.header.comment"_lang)) {
                            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::GetStyleColorVec4(ImGuiCol_ChildBg));
                            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1_scaled);
                            if (!locked) {
                                if (ImGui::InputTextMultiline("##comment", comment, ImVec2(ImGui::GetContentRegionAvail().x, 150_scaled), locked ? ImGuiInputTextFlags_ReadOnly : ImGuiInputTextFlags_None)) {
                                    commentDisplay = ui::Markdown(comment);
                                    m_bookmarks.markChanged();
                                }
                            } else {
                                commentDisplay.draw();
                            }
                            ImGui::PopStyleVar();
                            ImGui::PopStyleColor();
                        }
                        ImGuiExt::EndSubWindow();
                    }

                    ImGui::NewLine();
                }

                // Mark a bookmark for removal when the user clicks the remove button
                if (!notDeleted)
                    bookmarkToRemove = it;
            }

            // Remove the bookmark that was marked for removal
            if (bookmarkToRemove != m_bookmarks->end()) {
                m_bookmarks->erase(bookmarkToRemove);
                m_bookmarks.markChanged();
                EventHighlightingChanged::post();
            }
        }
        ImGuiExt::EndSubWindow();
    }

    FileBackedProviderData<ViewBookmarks::Bookmarks>::SerializedData ViewBookmarks::encodeBookmarks(const Bookmarks &bookmarks) {
        const auto data = bookmarksToJson(bookmarks).dump(4);
        return { data.begin(), data.end() };
    }

    std::optional<ViewBookmarks::Bookmarks> ViewBookmarks::decodeBookmarks(std::span<const u8> data) {
        try {
            return bookmarksFromJson(nlohmann::json::parse(data.begin(), data.end()));
        } catch (const std::exception &) {
            return std::nullopt;
        }
    }

    nlohmann::json ViewBookmarks::bookmarksToJson(const Bookmarks &bookmarks) {
        nlohmann::json json;
        json["bookmarks"] = nlohmann::json::array();
        size_t index = 0;
        for (const auto &[bookmark, highlightVisible, commentDisplay] : bookmarks) {
            json["bookmarks"][index] = {
                    { "name",       bookmark.name },
                    { "comment",    bookmark.comment },
                    { "color",      bookmark.color },
                    { "region", {
                            { "address",    bookmark.region.address },
                            { "size",       bookmark.region.size }
                        }
                    },
                    { "locked",     bookmark.locked },
                    { "id",         bookmark.id },
                    { "highlightVisible", highlightVisible }
            };

            index++;
        }

        return json;
    }

    std::optional<ViewBookmarks::Bookmarks> ViewBookmarks::bookmarksFromJson(const nlohmann::json &json) {
        if (!json.contains("bookmarks") || !json["bookmarks"].is_array())
            return std::nullopt;

        Bookmarks bookmarks;
        u64 nextBookmarkId = 0;
        for (const auto &bookmark : json["bookmarks"]) {
            if (!bookmark.contains("name") || !bookmark.contains("comment") || !bookmark.contains("color") || !bookmark.contains("region") || !bookmark.contains("locked"))
                continue;

            const auto &region = bookmark["region"];
            if (!region.contains("address") || !region.contains("size"))
                continue;

            const auto comment = bookmark["comment"].get<std::string>();
            const auto id = bookmark.contains("id") ? bookmark["id"].get<u64>() : nextBookmarkId;
            bookmarks.push_back({
                {
                    .region     = { .address=region["address"], .size=region["size"] },
                    .name       = bookmark["name"],
                    .comment    = comment,
                    .color      = bookmark["color"],
                    .locked     = bookmark["locked"],
                    .id         = id,
                },
                bookmark.contains("highlightVisible") ? bookmark["highlightVisible"].get<bool>() : true,
                ui::Markdown(comment)
            });
            nextBookmarkId = std::max(nextBookmarkId, id + 1);
        }

        return bookmarks;
    }

    void ViewBookmarks::refreshBookmarkState(prv::Provider *provider) {
        u64 currentBookmarkId = 0;
        for (auto &bookmark : m_bookmarks.get(provider)) {
            currentBookmarkId = std::max(currentBookmarkId, bookmark.entry.id);
            bookmark.commentDisplay = ui::Markdown(bookmark.entry.comment);
        }

        m_currBookmarkId.get(provider) = currentBookmarkId;
        EventHighlightingChanged::post();
    }

    bool ViewBookmarks::importBookmarks(prv::Provider *provider, const nlohmann::json &json) {
        auto importedBookmarks = bookmarksFromJson(json);
        if (!importedBookmarks.has_value())
            return false;

        auto &bookmarks = m_bookmarks.get(provider);
        const bool changed = !importedBookmarks->empty();
        bookmarks.splice(bookmarks.end(), *importedBookmarks);
        if (changed)
            m_bookmarks.markChanged(provider);
        this->refreshBookmarkState(provider);
        return true;
    }

    bool ViewBookmarks::exportBookmarks(prv::Provider *provider, nlohmann::json &json) {
        json = bookmarksToJson(m_bookmarks.get(provider));
        return true;
    }

    void ViewBookmarks::registerMenuItems() {
        /* Create bookmark */
        ContentRegistry::UserInterface::addMenuItem({ "hex.builtin.menu.edit"_unlocalized, "hex.builtin.menu.edit.bookmark.create"_unlocalized }, ICON_VS_BOOKMARK, 1900, CTRLCMD + Keys::B, [&] {
            if (!ImHexApi::HexEditor::isSelectionValid())
                return;

            auto selection = ImHexApi::HexEditor::getSelection();
            ImHexApi::Bookmarks::add(selection->getStartAddress(), selection->getSize(), {}, {});
        }, []{ return ImHexApi::Provider::isValid() && ImHexApi::HexEditor::isSelectionValid(); },
        ContentRegistry::Views::getViewByName("hex.builtin.view.hex_editor.name"_unlocalized));


        ContentRegistry::UserInterface::addMenuItemSeparator({ "hex.builtin.menu.file"_unlocalized, "hex.builtin.menu.file.import"_unlocalized }, 5400);

        /* Import bookmarks */
        ContentRegistry::UserInterface::addMenuItem({ "hex.builtin.menu.file"_unlocalized, "hex.builtin.menu.file.import"_unlocalized, "hex.builtin.menu.file.import.bookmark"_unlocalized }, ICON_VS_BOOKMARK, 5500, Shortcut::None, [this]{
            fs::openFileBrowser(fs::DialogMode::Open, { { "Bookmarks File", "hexbm"} }, [&, this](const std::fs::path &path) {
                try {
                    this->importBookmarks(ImHexApi::Provider::get(), nlohmann::json::parse(wolv::io::File(path, wolv::io::File::Mode::Read).readString()));
                } catch (const std::exception &e) {
                    log::warn("Failed to import bookmarks: {}", e.what());
                }
            });
        }, ImHexApi::Provider::isValid);

        ContentRegistry::UserInterface::addMenuItemSeparator({ "hex.builtin.menu.file"_unlocalized, "hex.builtin.menu.file.export"_unlocalized }, 6200);


        /* Export bookmarks */
        ContentRegistry::UserInterface::addMenuItem({ "hex.builtin.menu.file"_unlocalized, "hex.builtin.menu.file.export"_unlocalized, "hex.builtin.menu.file.export.bookmark"_unlocalized }, ICON_VS_BOOKMARK, 6250, Shortcut::None, [this]{
            fs::openFileBrowser(fs::DialogMode::Save, { { "Bookmarks File", "hexbm"} }, [&, this](const std::fs::path &path) {
                nlohmann::json json;
                this->exportBookmarks(ImHexApi::Provider::get(), json);

                wolv::io::File(path, wolv::io::File::Mode::Create).writeString(json.dump(4));
            });
        }, [this]{
            return ImHexApi::Provider::isValid() && !m_bookmarks->empty();
        });
    }

    void ViewBookmarks::drawHelpText() {
        ImGuiExt::TextFormattedWrapped("All your created Bookmarks will be listed in here.");
        ImGui::NewLine();
        ImGuiExt::TextFormattedWrapped("Bookmarks provide an easy way to mark important regions in your binary and quickly navigate to them later. You can also name them, add further information through comments or change their color.");
        ImGui::NewLine();
        ImGuiExt::TextFormattedWrapped(
            "To create a Bookmark, select a byte region in the Hex Editor view and use the {} option in the {} menu or use the shortcut '{}'.",
            "hex.builtin.menu.edit.bookmark.create"_lang, "hex.builtin.menu.edit"_lang,
            ShortcutManager::getShortcutByName(
                { "hex.builtin.menu.edit"_unlocalized, "hex.builtin.menu.edit.bookmark.create"_unlocalized },
                ContentRegistry::Views::getViewByName("hex.builtin.view.hex_editor.name"_unlocalized)
            ).toString()
        );
    }
}
