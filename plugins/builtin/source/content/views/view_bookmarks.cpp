#include "content/views/view_bookmarks.hpp"

#include <hex/api/content_registry.hpp>
#include <hex/api/project_file_manager.hpp>
#include <hex/api/achievement_manager.hpp>
#include <hex/api/task_manager.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/providers/provider.hpp>

#include <content/providers/view_provider.hpp>

#include <fonts/vscode_icons.hpp>

#include <nlohmann/json.hpp>

#include <wolv/io/file.hpp>
#include <wolv/utils/guards.hpp>

namespace hex::plugin::builtin {

    ViewBookmarks::ViewBookmarks() : View::Window("hex.builtin.view.bookmarks.name", ICON_VS_BOOKMARK) {

        // Handle bookmark add requests sent by the API
        RequestAddBookmark::subscribe(this, [this](Region region, std::string name, std::string comment, color_t color, u64 *id) {
            if (name.empty()) {
                name = hex::format("hex.builtin.view.bookmarks.default_title"_lang, region.address, region.address + region.size - 1);
            }

            if (color == 0x00)
                color = ImGui::GetColorU32(ImGuiCol_Header);

            m_currBookmarkId += 1;
            u64 bookmarkId = m_currBookmarkId;
            if (id != nullptr)
                *id = bookmarkId;

            auto bookmark = ImHexApi::Bookmarks::Entry {
                region,
                name,
                std::move(comment),
                color,
                true,
                bookmarkId
            };

            m_bookmarks->emplace_back(std::move(bookmark), TextEditor(), true);

            ImHexApi::Provider::markDirty();

            EventBookmarkCreated::post(m_bookmarks->back().entry);
            EventHighlightingChanged::post();
        });

        RequestRemoveBookmark::subscribe([this](u64 id) {
            std::erase_if(m_bookmarks.get(), [id](const auto &bookmark) {
                return bookmark.entry.id == id;
            });
        });

        // Draw hex editor background highlights for bookmarks
        ImHexApi::HexEditor::addBackgroundHighlightingProvider([this](u64 address, const u8* data, size_t size, bool) -> std::optional<color_t> {
            std::ignore = data;

            // Check all bookmarks for potential overlaps with the current address
            std::optional<ImColor> color;
            for (const auto &bookmark : *m_bookmarks) {
                if (!bookmark.highlightVisible)
                    continue;

                if (Region { address, size }.isWithin(bookmark.entry.region)) {
                    color = blendColors(color, bookmark.entry.color);
                }
            }

            return color;
        });

        // Draw hex editor tooltips for bookmarks
        ImHexApi::HexEditor::addTooltipProvider([this](u64 address, const u8 *data, size_t size) {
            std::ignore = data;

            // Loop over all bookmarks
            for (const auto &[bookmark, editor, highlightVisible] : *m_bookmarks) {
                if (!highlightVisible)
                    continue;

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
                                    ImGuiExt::TextFormattedWrapped("{}", bookmark.comment);
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
            .load = [this](prv::Provider *provider, const std::fs::path &basePath, const Tar &tar) -> bool {
                auto fileContent = tar.readString(basePath);
                if (fileContent.empty())
                    return true;

                auto data = nlohmann::json::parse(fileContent.begin(), fileContent.end());
                m_bookmarks.get(provider).clear();
                return this->importBookmarks(provider, data);
            },
            .store = [this](prv::Provider *provider, const std::fs::path &basePath, const Tar &tar) -> bool {
                nlohmann::json data;

                bool result = this->exportBookmarks(provider, data);
                tar.writeString(basePath, data.dump(4));

                return result;
            }
        });

        ContentRegistry::Reports::addReportProvider([this](prv::Provider *provider) -> std::string {
            std::string result;

            const auto &bookmarks = m_bookmarks.get(provider);
            if (bookmarks.empty())
                return "";

            result += "## Bookmarks\n\n";

            for (const auto &[bookmark, editor, highlightVisible] : bookmarks) {
                result += hex::format("### <span style=\"background-color: #{:06X}80\">{} [0x{:04X} - 0x{:04X}]</span>\n\n", hex::changeEndianness(bookmark.color, std::endian::big) >> 8, bookmark.name, bookmark.region.getStartAddress(), bookmark.region.getEndAddress());

                for (const auto &line : hex::splitString(bookmark.comment, "\n"))
                    result += hex::format("> {}\n", line);
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

    void ViewBookmarks::drawDropTarget(std::list<Bookmark>::iterator it, float height) {
        height = std::max(height, 1.0F);

        if (it != m_bookmarks->begin()) {
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - height);
        } else {
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + height);
        }

        ImGui::InvisibleButton("##DropTarget", ImVec2(ImGui::GetContentRegionAvail().x, height * 2.0F));
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

        if (ImGui::BeginChild("##bookmarks")) {
            if (m_bookmarks->empty()) {
                ImGuiExt::TextOverlay("hex.builtin.view.bookmarks.no_bookmarks"_lang, ImGui::GetWindowPos() + ImGui::GetWindowSize() / 2, ImGui::GetWindowWidth() * 0.7);
            }

            auto bookmarkToRemove = m_bookmarks->end();
            const auto defaultItemSpacing = ImGui::GetStyle().ItemSpacing.y;

            ImGui::Dummy({ ImGui::GetContentRegionAvail().x, 0 });
            drawDropTarget(m_bookmarks->begin(), defaultItemSpacing);

            // Draw all bookmarks
            for (auto it = m_bookmarks->begin(); it != m_bookmarks->end(); ++it) {
                auto &[bookmark, editor, highlightVisible] = *it;
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
                auto expanded = ImGui::CollapsingHeader(hex::format("{}###bookmark", name).c_str(), &notDeleted);
                ImGui::PopStyleVar();

                if (!expanded) {
                    // Handle dragging bookmarks up and down when they're collapsed

                    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoHoldToOpenOthers | ImGuiDragDropFlags_SourceAllowNullID)) {
                        // Set the payload to the bookmark id
                        ImGui::SetDragDropPayload("BOOKMARK_PAYLOAD", &bookmarkId, sizeof(bookmarkId));

                        // Draw drag and drop tooltip
                        ImGui::ColorButton("##color", headerColor.Value, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoAlpha);
                        ImGui::SameLine();
                        ImGuiExt::TextFormatted("{}", name);

                        if (!comment.empty()) {
                            ImGui::Separator();
                            ImGui::PushTextWrapPos(300_scaled);
                            ImGuiExt::TextFormatted("{}", comment);
                            ImGui::PopTextWrapPos();
                        }

                        ImGui::EndDragDropSource();
                    }
                }

                auto nextPos = ImGui::GetCursorPos();

                ImGui::SameLine();
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - 100_scaled);

                {
                    // Draw jump to region button
                    if (ImGuiExt::DimmedIconButton(ICON_VS_DEBUG_STEP_BACK, ImGui::GetStyleColorVec4(ImGuiCol_Text)))
                        ImHexApi::HexEditor::setSelection(region);
                    ImGui::SetItemTooltip("%s", "hex.builtin.view.bookmarks.tooltip.jump_to"_lang.get());

                    ImGui::SameLine(0, 1_scaled);

                    // Draw open in new view button
                    if (ImGuiExt::DimmedIconButton(ICON_VS_GO_TO_FILE, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
                        auto provider = ImHexApi::Provider::get();
                        TaskManager::doLater([region, provider, name]{
                            auto newProvider = ImHexApi::Provider::createProvider("hex.builtin.provider.view", true);
                            if (auto *viewProvider = dynamic_cast<ViewProvider*>(newProvider); viewProvider != nullptr) {
                                viewProvider->setProvider(region.getStartAddress(), region.getSize(), provider);
                                viewProvider->setName(hex::format("'{}' View", name));

                                if (viewProvider->open()) {
                                    EventProviderOpened::post(viewProvider);
                                    AchievementManager::unlockAchievement("hex.builtin.achievement.hex_editor", "hex.builtin.achievement.hex_editor.open_new_view.name");
                                }
                            }
                        });
                    }
                    ImGui::SetItemTooltip("%s", "hex.builtin.view.bookmarks.tooltip.open_in_view"_lang.get());

                    ImGui::SameLine(0, 4_scaled);

                    // Draw highlight visible toggle
                    if (ImGuiExt::DimmedIconButton(highlightVisible ? ICON_VS_EYE : ICON_VS_EYE_CLOSED, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
                        highlightVisible = !highlightVisible;
                        EventHighlightingChanged::post();
                    }
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
                        ImGuiExt::DimmedIconToggle(ICON_VS_LOCK, ICON_VS_UNLOCK, &locked);
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
                            drawColorPopup(headerColor);
                            color = headerColor;
                            ImGui::EndPopup();
                        }

                        ImGui::SameLine();

                        // Draw bookmark name if the bookmark is locked or an input text box if it's unlocked
                        if (locked) {
                            ImGui::TextUnformatted(name.data());
                        } else {
                            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
                            ImGui::InputText("##nameInput", name);
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

                    // Draw comment if the bookmark is locked or an input text box if it's unlocked
                    editor.SetReadOnly(locked);
                    editor.SetShowLineNumbers(!locked);
                    editor.SetShowCursor(!locked);
                    editor.SetShowWhitespaces(false);

                    if (!locked || (locked && !comment.empty())) {
                        if (ImGuiExt::BeginSubWindow("hex.builtin.view.bookmarks.header.comment"_lang)) {
                            editor.Render("##comment", ImVec2(ImGui::GetContentRegionAvail().x, 150_scaled), false);
                        }
                        ImGuiExt::EndSubWindow();

                        if (editor.IsTextChanged())
                            comment = editor.GetText();
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
                EventHighlightingChanged::post();
            }
        }
        ImGui::EndChild();
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

            TextEditor editor;
            editor.SetText(bookmark["comment"]);
            m_bookmarks.get(provider).push_back({
                {
                    .region     = { region["address"], region["size"] },
                    .name       = bookmark["name"],
                    .comment    = bookmark["comment"],
                    .color      = bookmark["color"],
                    .locked     = bookmark["locked"],
                    .id         = bookmark.contains("id") ? bookmark["id"].get<u64>() : m_currBookmarkId.get(provider),
                },
                editor,
                bookmark.contains("highlightVisible") ? bookmark["highlightVisible"].get<bool>() : true,
            });
            if (bookmark.contains("id"))
                m_currBookmarkId.get(provider) = std::max<u64>(m_currBookmarkId.get(provider), bookmark["id"].get<i64>() + 1);
            else
                m_currBookmarkId.get(provider) += 1;
        }

        return true;
    }

    bool ViewBookmarks::exportBookmarks(prv::Provider *provider, nlohmann::json &json) {
        json["bookmarks"] = nlohmann::json::array();
        size_t index = 0;
        for (const auto &[bookmark, editor, highlightVisible]  : m_bookmarks.get(provider)) {
            json["bookmarks"][index] = {
                    { "name",       bookmark.name },
                    { "comment",    editor.GetText() },
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

        return true;
    }

    void ViewBookmarks::registerMenuItems() {
        /* Create bookmark */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.edit", "hex.builtin.menu.edit.bookmark.create" }, ICON_VS_BOOKMARK, 1900, CTRLCMD + Keys::B, [&] {
            if (!ImHexApi::HexEditor::isSelectionValid())
                return;

            auto selection = ImHexApi::HexEditor::getSelection();
            ImHexApi::Bookmarks::add(selection->getStartAddress(), selection->getSize(), {}, {});
        }, []{ return ImHexApi::Provider::isValid() && ImHexApi::HexEditor::isSelectionValid(); });


        ContentRegistry::Interface::addMenuItemSeparator({ "hex.builtin.menu.file", "hex.builtin.menu.file.import" }, 3000);

        /* Import bookmarks */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.menu.file.import", "hex.builtin.menu.file.import.bookmark" }, ICON_VS_BOOKMARK, 3050, Shortcut::None, [this]{
            fs::openFileBrowser(fs::DialogMode::Open, { { "Bookmarks File", "hexbm"} }, [&, this](const std::fs::path &path) {
                try {
                    this->importBookmarks(ImHexApi::Provider::get(), nlohmann::json::parse(wolv::io::File(path, wolv::io::File::Mode::Read).readString()));
                } catch (...) { }
            });
        }, ImHexApi::Provider::isValid);

        ContentRegistry::Interface::addMenuItemSeparator({ "hex.builtin.menu.file", "hex.builtin.menu.file.export" }, 6200);


        /* Export bookmarks */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.menu.file.export", "hex.builtin.menu.file.export.bookmark" }, ICON_VS_BOOKMARK, 6250, Shortcut::None, [this]{
            fs::openFileBrowser(fs::DialogMode::Save, { { "Bookmarks File", "hexbm"} }, [&, this](const std::fs::path &path) {
                nlohmann::json json;
                this->exportBookmarks(ImHexApi::Provider::get(), json);

                wolv::io::File(path, wolv::io::File::Mode::Create).writeString(json.dump(4));
            });
        }, [this]{
            return ImHexApi::Provider::isValid() && !m_bookmarks->empty();
        });
    }

}