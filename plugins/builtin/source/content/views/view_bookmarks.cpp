#include "content/views/view_bookmarks.hpp"

#include <hex/providers/provider.hpp>
#include <hex/helpers/fmt.hpp>

#include <hex/helpers/project_file_handler.hpp>

#include <cstring>

namespace hex::plugin::builtin {

    ViewBookmarks::ViewBookmarks() : View("hex.builtin.view.bookmarks.name") {
        EventManager::subscribe<RequestAddBookmark>(this, [this](Region region, std::string name, std::string comment, color_t color) {
            if (name.empty()) {
                name = hex::format("hex.builtin.view.bookmarks.default_title"_lang, region.address, region.address + region.size - 1);
            }

            if (color == 0x00)
                color = ImGui::GetColorU32(ImGuiCol_Header);


            this->m_bookmarks.push_back({ region,
                name,
                std::move(comment),
                color,
                false
            });

            ProjectFile::markDirty();
        });

        EventManager::subscribe<EventProjectFileLoad>(this, [this] {
            this->m_bookmarks = ProjectFile::getBookmarks();
        });

        EventManager::subscribe<EventProjectFileStore>(this, [this] {
            ProjectFile::setBookmarks(this->m_bookmarks);
        });

        EventManager::subscribe<EventProviderDeleted>(this, [this](const auto*) {
            this->m_bookmarks.clear();
        });


        ImHexApi::HexEditor::addBackgroundHighlightingProvider([this](u64 address, const u8* data, size_t size) -> std::optional<color_t> {
            hex::unused(data);

            for (const auto &bookmark : this->m_bookmarks) {
                if (Region { address, size }.isWithin(bookmark.region))
                    return bookmark.color;
            }

            return std::nullopt;
        });

        ImHexApi::HexEditor::addTooltipProvider([this](u64 address, const u8 *data, size_t size) {
            hex::unused(data);
            for (const auto &bookmark : this->m_bookmarks) {
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
    }

    ViewBookmarks::~ViewBookmarks() {
        EventManager::unsubscribe<RequestAddBookmark>(this);
        EventManager::unsubscribe<EventProjectFileLoad>(this);
        EventManager::unsubscribe<EventProjectFileStore>(this);
        EventManager::unsubscribe<EventProviderDeleted>(this);

        this->m_bookmarks.clear();
    }

    void ViewBookmarks::drawContent() {
        if (ImGui::Begin(View::toWindowName("hex.builtin.view.bookmarks.name").c_str(), &this->getWindowOpenState())) {

            ImGui::PushItemWidth(ImGui::GetContentRegionAvailWidth());
            ImGui::InputTextWithHint("##filter", "hex.builtin.common.filter"_lang, this->m_currFilter);
            ImGui::PopItemWidth();

            ImGui::NewLine();

            if (ImGui::BeginChild("##bookmarks")) {

                if (this->m_bookmarks.empty()) {
                    ImGui::TextFormattedCentered("hex.builtin.view.bookmarks.no_bookmarks"_lang);
                }

                u32 id = 1;
                auto bookmarkToRemove = this->m_bookmarks.end();
                for (auto iter = this->m_bookmarks.begin(); iter != this->m_bookmarks.end(); iter++) {
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

                    bool open = true;
                    if (ImGui::CollapsingHeader(hex::format("{}###bookmark", name).c_str(), &open)) {
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

                                for (size_t byte = 0; byte < 0x10; byte++) {
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

                        ImGui::ColorEdit4("hex.builtin.view.bookmarks.header.color"_lang, (float *)&headerColor.Value, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoAlpha | (locked ? ImGuiColorEditFlags_NoPicker : ImGuiColorEditFlags_None));
                        color = headerColor;
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

                    ImGui::PopID();
                    ImGui::PopStyleColor(3);
                    id++;
                }

                if (bookmarkToRemove != this->m_bookmarks.end()) {
                    this->m_bookmarks.erase(bookmarkToRemove);
                    ProjectFile::markDirty();
                }
            }
            ImGui::EndChild();
        }
        ImGui::End();
    }

}