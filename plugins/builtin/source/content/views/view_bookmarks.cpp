#include "content/views/view_bookmarks.hpp"

#include <hex/api/content_registry.hpp>
#include <hex/api/project_file_manager.hpp>
#include <hex/providers/provider.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/helpers/file.hpp>

#include <nlohmann/json.hpp>
#include <cstring>

#include <content/helpers/provider_extra_data.hpp>

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
                        ImGui::TextUnformatted(bookmark.name.c_str());

                        if (ImGui::GetIO().KeyShift) {
                            ImGui::Indent();
                            if (ImGui::BeginTable("##extra_info", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_NoClip)) {

                                ImGui::TableNextRow();
                                ImGui::TableNextColumn();

                                ImGui::TableNextRow();
                                ImGui::TableNextColumn();
                                ImGui::TextFormatted("{}: ", "hex.builtin.common.region"_lang.get());
                                ImGui::TableNextColumn();
                                ImGui::TextFormatted("[ 0x{:08X} - 0x{:08X} ]", bookmark.region.getStartAddress(), bookmark.region.getEndAddress());

                                if (!bookmark.comment.empty() && bookmark.comment[0] != '\x00') {
                                    ImGui::TableNextRow();
                                    ImGui::TableNextColumn();
                                    ImGui::TextFormatted("{}: ", "hex.builtin.view.bookmarks.header.comment"_lang.get());
                                    ImGui::TableNextColumn();
                                    ImGui::TextFormattedWrapped("\"{}\"", bookmark.comment);
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
                    if (!ImGui::CollapsingHeader(hex::format("{}###bookmark", name).c_str(), &open)) {
                        if (ImGui::IsMouseClicked(0) && ImGui::IsItemActivated() && this->m_dragStartIterator == bookmarks.end())
                            this->m_dragStartIterator = iter;

                        if (ImGui::IsItemHovered() && this->m_dragStartIterator != bookmarks.end()) {
                            std::iter_swap(iter, this->m_dragStartIterator);
                            this->m_dragStartIterator = iter;
                        }

                        if (!ImGui::IsMouseDown(0))
                            this->m_dragStartIterator = bookmarks.end();
                    } else {
                        ImGui::TextUnformatted("hex.builtin.view.bookmarks.title.info"_lang);
                        ImGui::Separator();
                        ImGui::TextFormatted("hex.builtin.view.bookmarks.address"_lang, region.address, region.address + region.size - 1, region.size);

                        if (ImGui::BeginChild("hexData", ImVec2(0, ImGui::GetTextLineHeight() * 8), true)) {
                            size_t offset = region.address % 0x10;

                            for (u8 byte = 0; byte < 0x10; byte++) {
                                ImGui::TextFormattedDisabled("{0:02X}", byte);
                                ImGui::SameLine();
                            }

                            ImGui::NewLine();

                            // TODO: Clip this somehow

                            // First line
                            {
                                std::array<u8, 0x10> bytes = { 0 };
                                size_t byteCount           = std::min<size_t>(0x10 - offset, region.size);
                                ImHexApi::Provider::get()->read(region.address, bytes.data() + offset, byteCount);

                                for (size_t byte = 0; byte < (offset % 0x10) + byteCount; byte++) {
                                    if (byte < offset)
                                        ImGui::TextUnformatted("  ");
                                    else
                                        ImGui::TextFormatted("{0:02X}", bytes[byte]);
                                    ImGui::SameLine();
                                }
                                ImGui::NewLine();
                            }

                            // Other lines
                            {
                                std::array<u8, 0x10> bytes = { 0 };
                                for (u32 i = 0x10 - offset; i < region.size; i += 0x10) {
                                    size_t byteCount = std::min<size_t>(region.size - i, 0x10);
                                    ImHexApi::Provider::get()->read(region.address + i, bytes.data(), byteCount);

                                    for (size_t byte = 0; byte < byteCount; byte++) {
                                        ImGui::TextFormatted("{0:02X}", bytes[byte]);
                                        ImGui::SameLine();
                                    }
                                    ImGui::NewLine();
                                }
                            }
                        }
                        ImGui::EndChild();

                        if (ImGui::Button("hex.builtin.view.bookmarks.button.jump"_lang))
                            ImHexApi::HexEditor::setSelection(region);
                        ImGui::SameLine(0, 15);

                        if (locked) {
                            if (ImGui::Button(ICON_FA_LOCK)) locked = false;
                        } else {
                            if (ImGui::Button(ICON_FA_UNLOCK)) locked = true;
                        }

                        ImGui::NewLine();
                        ImGui::TextUnformatted("hex.builtin.view.bookmarks.header.name"_lang);
                        ImGui::Separator();

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
                        else
                            ImGui::InputText("##nameInput", name);

                        ImGui::NewLine();
                        ImGui::TextUnformatted("hex.builtin.view.bookmarks.header.comment"_lang);
                        ImGui::Separator();

                        if (locked)
                            ImGui::TextFormattedWrapped("{}", comment.data());
                        else
                            ImGui::InputTextMultiline("##commentInput", comment);

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
        ContentRegistry::Interface::addMenuItem("hex.builtin.menu.edit", 1050, [&] {
            bool providerValid = ImHexApi::Provider::isValid();
            auto selection     = ImHexApi::HexEditor::getSelection();

            if (ImGui::MenuItem("hex.builtin.menu.edit.bookmark.create"_lang, nullptr, false, selection.has_value() && providerValid)) {
                ImHexApi::Bookmarks::add(selection->getStartAddress(), selection->getSize(), {}, {});
            }
        });

        ContentRegistry::Interface::addMenuItem("hex.builtin.menu.file", 4000, [&] {
            bool providerValid = ImHexApi::Provider::isValid();

            if (ImGui::MenuItem("hex.builtin.menu.file.bookmark.import"_lang, nullptr, false, providerValid)) {
                fs::openFileBrowser(fs::DialogMode::Open, { { "Bookmarks File", "hexbm"} }, [&](const std::fs::path &path) {
                    try {
                        importBookmarks(ImHexApi::Provider::get(), nlohmann::json::parse(fs::File(path, fs::File::Mode::Read).readString()));
                    } catch (...) { }
                });
            }
            if (ImGui::MenuItem("hex.builtin.menu.file.bookmark.export"_lang, nullptr, false, providerValid && !ProviderExtraData::getCurrent().bookmarks.empty())) {
                fs::openFileBrowser(fs::DialogMode::Save, { { "Bookmarks File", "hexbm"} }, [&](const std::fs::path &path) {
                    nlohmann::json json;
                    exportBookmarks(ImHexApi::Provider::get(), json);

                    fs::File(path, fs::File::Mode::Create).write(json.dump(4));
                });
            }
        });
    }

}