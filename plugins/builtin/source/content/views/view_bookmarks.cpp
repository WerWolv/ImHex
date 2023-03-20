#include "content/views/view_bookmarks.hpp"

#include <hex/api/content_registry.hpp>
#include <hex/api/project_file_manager.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/helpers/utils.hpp>

#include <hex/providers/provider.hpp>
#include <content/providers/view_provider.hpp>

#include <nlohmann/json.hpp>
#include <cstring>

#include <content/helpers/provider_extra_data.hpp>

#include <wolv/io/file.hpp>
#include <wolv/utils/guards.hpp>

namespace hex::plugin::builtin {

    ViewBookmarks::ViewBookmarks() : View("hex.builtin.view.bookmarks.name") {
        EventManager::subscribe<RequestAddBookmark>(this, [](Region region, std::string name, std::string comment, color_t color) {
            if (name.empty()) {
                name = hex::format("hex.builtin.view.bookmarks.default_title"_lang, region.address, region.address + region.size - 1);
            }

            if (color == 0x00)
                color = ImGui::GetColorU32(ImGuiCol_Header);

            ProviderExtraData::getCurrent().bookmarks.push_back({
                region,
                name,
                std::move(comment),
                color,
                false
            });

            ImHexApi::Provider::markDirty();
        });

        ImHexApi::HexEditor::addBackgroundHighlightingProvider([](u64 address, const u8* data, size_t size, bool) -> std::optional<color_t> {
            hex::unused(data);

            for (const auto &bookmark : ProviderExtraData::getCurrent().bookmarks) {
                if (Region { address, size }.isWithin(bookmark.region))
                    return bookmark.color;
            }

            return std::nullopt;
        });

        ImHexApi::HexEditor::addTooltipProvider([](u64 address, const u8 *data, size_t size) {
            hex::unused(data);
            for (const auto &bookmark : ProviderExtraData::getCurrent().bookmarks) {
                if (!Region { address, size }.isWithin(bookmark.region))
                    continue;

                ImGui::BeginTooltip();

                ImGui::PushID(&bookmark);
                if (ImGui::BeginTable("##tooltips", 1, ImGuiTableFlags_RowBg | ImGuiTableFlags_NoClip)) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    {
                        ImGui::ColorButton("##color", ImColor(bookmark.color));
                        ImGui::SameLine(0, 10);
                        ImGui::TextFormatted("{} ", bookmark.name);

                        if (ImGui::GetIO().KeyShift) {
                            ImGui::Indent();
                            if (ImGui::BeginTable("##extra_info", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_NoClip)) {

                                ImGui::TableNextRow();
                                ImGui::TableNextColumn();

                                ImGui::TableNextRow();
                                ImGui::TableNextColumn();
                                ImGui::TextFormatted("{}: ", "hex.builtin.common.region"_lang.get());
                                ImGui::TableNextColumn();
                                ImGui::TextFormatted("[ 0x{:08X} - 0x{:08X} ] ", bookmark.region.getStartAddress(), bookmark.region.getEndAddress());

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

        ProjectFile::registerPerProviderHandler({
            .basePath = "bookmarks.json",
            .required = false,
            .load = [](prv::Provider *provider, const std::fs::path &basePath, Tar &tar) -> bool {
                auto fileContent = tar.read(basePath);
                if (fileContent.empty())
                    return true;

                auto data = nlohmann::json::parse(fileContent.begin(), fileContent.end());
                ProviderExtraData::get(provider).bookmarks.clear();
                return ViewBookmarks::importBookmarks(provider, data);
            },
            .store = [](prv::Provider *provider, const std::fs::path &basePath, Tar &tar) -> bool {
                nlohmann::json data;

                bool result = ViewBookmarks::exportBookmarks(provider, data);
                tar.write(basePath, data.dump(4));

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
        static auto Palette = []{
            constexpr static auto ColorCount = 36;
            std::array<ImColor, ColorCount> result = { 0 };

            u32 counter = 0;
            for (auto &color : result) {
                ImGui::ColorConvertHSVtoRGB(float(counter) / float(ColorCount - 1), 0.8F, 0.8F, color.Value.x, color.Value.y, color.Value.z);
                color.Value.w = 0.7f;

                counter++;
            }

            return result;
        }();

        ImGui::ColorPicker4("##picker", (float*)&color, ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoOptions | ImGuiColorEditFlags_NoSmallPreview);

        ImGui::Separator();

        int id = 0;
        for (const auto &paletteColor : Palette) {
            ImGui::PushID(id);
            if ((id % 9) != 0)
                ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.y);

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

            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
            ImGui::InputTextWithHint("##filter", "hex.builtin.common.filter"_lang, this->m_currFilter);
            ImGui::PopItemWidth();

            ImGui::NewLine();

            if (ImGui::BeginChild("##bookmarks")) {
                auto &bookmarks = ProviderExtraData::getCurrent().bookmarks;
                if (bookmarks.empty()) {
                    ImGui::TextFormattedCentered("hex.builtin.view.bookmarks.no_bookmarks"_lang);
                }

                int id = 1;
                auto bookmarkToRemove = bookmarks.end();
                for (auto iter = bookmarks.begin(); iter != bookmarks.end(); iter++) {
                    auto &[region, name, comment, color, locked] = *iter;

                    if (!this->m_currFilter.empty()) {
                        if (!name.contains(this->m_currFilter) && !comment.contains(this->m_currFilter))
                            continue;
                    }

                    auto headerColor = ImColor(color);
                    auto hoverColor  = ImColor(color);
                    hoverColor.Value.w *= 1.3F;

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
                        if (ImGui::IsMouseClicked(0) && ImGui::IsItemActivated() && this->m_dragStartIterator == bookmarks.end())
                            this->m_dragStartIterator = iter;

                        if (ImGui::IsItemHovered() && this->m_dragStartIterator != bookmarks.end()) {
                            std::iter_swap(iter, this->m_dragStartIterator);
                            this->m_dragStartIterator = iter;
                        }

                        if (!ImGui::IsMouseDown(0))
                            this->m_dragStartIterator = bookmarks.end();
                    } else {
                        const auto rowHeight = ImGui::GetTextLineHeightWithSpacing() + 2 * ImGui::GetStyle().FramePadding.y;
                        if (ImGui::BeginTable("##bookmark_table", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
                            ImGui::TableSetupColumn("##name");
                            ImGui::TableSetupColumn("##spacing", ImGuiTableColumnFlags_WidthFixed, 20);
                            ImGui::TableSetupColumn("##value", ImGuiTableColumnFlags_WidthStretch);

                            ImGui::TableNextRow(ImGuiTableRowFlags_None, rowHeight);
                            ImGui::TableNextColumn();

                            ImGui::TextUnformatted("hex.builtin.view.bookmarks.header.name"_lang);
                            ImGui::TableNextColumn();
                            ImGui::TableNextColumn();

                            if (locked) {
                                if (ImGui::IconButton(ICON_VS_LOCK, ImGui::GetStyleColorVec4(ImGuiCol_Text))) locked = false;
                            } else {
                                if (ImGui::IconButton(ICON_VS_UNLOCK, ImGui::GetStyleColorVec4(ImGuiCol_Text))) locked = true;
                            }

                            ImGui::SameLine();

                            if (ImGui::ColorButton("hex.builtin.view.bookmarks.header.color"_lang, headerColor.Value, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoAlpha)) {
                                if (!locked)
                                    ImGui::OpenPopup("hex.builtin.view.bookmarks.header.color"_lang);
                            }

                            if (ImGui::BeginPopup("hex.builtin.view.bookmarks.header.color"_lang)) {
                                drawColorPopup(headerColor);
                                color = headerColor;
                                ImGui::EndPopup();
                            }

                            ImGui::SameLine();

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

                            if (ImGui::IconButton(ICON_VS_DEBUG_STEP_BACK, ImGui::GetStyleColorVec4(ImGuiCol_Text)))
                                ImHexApi::HexEditor::setSelection(region);
                            ImGui::SameLine();
                            if (ImGui::IconButton(ICON_VS_GO_TO_FILE, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
                                auto newProvider = ImHexApi::Provider::createProvider("hex.builtin.provider.view", true);
                                if (auto *viewProvider = dynamic_cast<ViewProvider*>(newProvider); viewProvider != nullptr) {
                                    viewProvider->setProvider(region.getStartAddress(), region.getSize(), provider);
                                    if (viewProvider->open())
                                        EventManager::post<EventProviderOpened>(viewProvider);
                                }
                            }
                            ImGui::SameLine();
                            ImGui::TextFormatted("hex.builtin.view.bookmarks.address"_lang, region.getStartAddress(), region.getEndAddress());

                            ImGui::TableNextRow(ImGuiTableRowFlags_None, rowHeight);
                            ImGui::TableNextColumn();

                            ImGui::TextUnformatted("hex.builtin.common.size"_lang);
                            ImGui::TableNextColumn();
                            ImGui::TableNextColumn();
                            ImGui::TextFormatted(hex::toByteString(region.size));

                            ImGui::EndTable();
                        }

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

                    if (!open)
                        bookmarkToRemove = iter;
                }

                if (bookmarkToRemove != bookmarks.end()) {
                    bookmarks.erase(bookmarkToRemove);
                }
            }
            ImGui::EndChild();
        }
        ImGui::End();
    }

    bool ViewBookmarks::importBookmarks(prv::Provider *provider, const nlohmann::json &json) {
        if (!json.contains("bookmarks"))
            return false;

        auto &bookmarks = ProviderExtraData::get(provider).bookmarks;
        for (const auto &bookmark : json["bookmarks"]) {
            if (!bookmark.contains("name") || !bookmark.contains("comment") || !bookmark.contains("color") || !bookmark.contains("region") || !bookmark.contains("locked"))
                continue;

            const auto &region = bookmark["region"];
            if (!region.contains("address") || !region.contains("size"))
                continue;

            bookmarks.push_back({
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
        for (const auto &bookmark : ProviderExtraData::get(provider).bookmarks) {
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
            auto selection = ImHexApi::HexEditor::getSelection();
            ImHexApi::Bookmarks::add(selection->getStartAddress(), selection->getSize(), {}, {});
        }, []{ return ImHexApi::Provider::isValid() && ImHexApi::HexEditor::isSelectionValid(); });


        ContentRegistry::Interface::addMenuItemSeparator({ "hex.builtin.menu.file", "hex.builtin.menu.file.import" }, 3000);

        /* Import bookmarks */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.menu.file.import", "hex.builtin.menu.file.import.bookmark" }, 3050, Shortcut::None, []{
            fs::openFileBrowser(fs::DialogMode::Open, { { "Bookmarks File", "hexbm"} }, [&](const std::fs::path &path) {
                try {
                    importBookmarks(ImHexApi::Provider::get(), nlohmann::json::parse(wolv::io::File(path, wolv::io::File::Mode::Read).readString()));
                } catch (...) { }
            });
        }, ImHexApi::Provider::isValid);

        ContentRegistry::Interface::addMenuItemSeparator({ "hex.builtin.menu.file", "hex.builtin.menu.file.export" }, 6200);


        /* Export bookmarks */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.menu.file.export", "hex.builtin.menu.file.export.bookmark" }, 6250, Shortcut::None, []{
            fs::openFileBrowser(fs::DialogMode::Save, { { "Bookmarks File", "hexbm"} }, [&](const std::fs::path &path) {
                nlohmann::json json;
                exportBookmarks(ImHexApi::Provider::get(), json);

                wolv::io::File(path, wolv::io::File::Mode::Create).write(json.dump(4));
            });
        }, []{
            return ImHexApi::Provider::isValid() && !ProviderExtraData::getCurrent().bookmarks.empty();
        });
    }

}