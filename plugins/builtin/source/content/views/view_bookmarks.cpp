#include "content/views/view_bookmarks.hpp"

#include <hex/providers/provider.hpp>
#include <hex/helpers/fmt.hpp>

#include <hex/helpers/project_file_handler.hpp>

#include <cstring>

namespace hex::plugin::builtin {

    ViewBookmarks::ViewBookmarks() : View("hex.builtin.view.bookmarks.name") {
        EventManager::subscribe<RequestAddBookmark>(this, [](ImHexApi::Bookmarks::Entry bookmark) {
            bookmark.comment.resize(0xF'FFFF);

            if (bookmark.name.empty()) {
                bookmark.name.resize(64);
                std::memset(bookmark.name.data(), 0x00, 64);
                std::strcpy(bookmark.name.data(), hex::format("hex.builtin.view.bookmarks.default_title"_lang,
                                                              bookmark.region.address,
                                                              bookmark.region.address + bookmark.region.size - 1).c_str());
            }

            if (bookmark.comment.empty())
                std::memset(bookmark.comment.data(), 0x00, 0xF'FFFF);

            bookmark.color = ImGui::GetColorU32(ImGuiCol_Header);

            SharedData::bookmarkEntries.push_back(bookmark);
            ProjectFile::markDirty();
        });

        EventManager::subscribe<EventProjectFileLoad>(this, []{
            SharedData::bookmarkEntries = ProjectFile::getBookmarks();
        });

        EventManager::subscribe<EventProjectFileStore>(this, []{
            ProjectFile::setBookmarks(SharedData::bookmarkEntries);
        });

        EventManager::subscribe<EventFileUnloaded>(this, []{
            ImHexApi::Bookmarks::getEntries().clear();
        });
    }

    ViewBookmarks::~ViewBookmarks() {
        EventManager::unsubscribe<RequestAddBookmark>(this);
        EventManager::unsubscribe<EventProjectFileLoad>(this);
        EventManager::unsubscribe<EventProjectFileStore>(this);
        EventManager::unsubscribe<EventFileUnloaded>(this);
    }

    void ViewBookmarks::drawContent() {
        if (ImGui::Begin(View::toWindowName("hex.builtin.view.bookmarks.name").c_str(), &this->getWindowOpenState())) {
            if (ImGui::BeginChild("##scrolling")) {

                auto &bookmarks = ImHexApi::Bookmarks::getEntries();

                if (bookmarks.empty()) {
                    std::string text = "hex.builtin.view.bookmarks.no_bookmarks"_lang;
                    auto textSize = ImGui::CalcTextSize(text.c_str());
                    auto availableSpace = ImGui::GetContentRegionAvail();

                    ImGui::SetCursorPos((availableSpace - textSize) / 2.0F);
                    ImGui::TextUnformatted(text.c_str());
                }

                u32 id = 1;
                auto bookmarkToRemove = bookmarks.end();
                for (auto iter = bookmarks.begin(); iter != bookmarks.end(); iter++) {
                    auto &[region, name, comment, color, locked] = *iter;

                    auto headerColor = ImColor(color);
                    auto hoverColor = ImColor(color);
                    hoverColor.Value.w *= 1.3F;

                    ImGui::PushID(id);
                    ImGui::PushStyleColor(ImGuiCol_Header, color);
                    ImGui::PushStyleColor(ImGuiCol_HeaderActive, color);
                    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, u32(hoverColor));
                    if (ImGui::CollapsingHeader((std::string(name.data()) + "###bookmark").c_str())) {
                        ImGui::TextUnformatted("hex.builtin.view.bookmarks.title.info"_lang);
                        ImGui::Separator();
                        ImGui::TextUnformatted(hex::format("hex.builtin.view.bookmarks.address"_lang, region.address, region.address + region.size - 1, region.size).c_str());

                        if (ImGui::BeginChild("hexData", ImVec2(0, ImGui::GetTextLineHeight() * 8), true)) {
                            size_t offset = region.address % 0x10;

                            for (size_t byte = 0; byte < 0x10; byte++) {
                                ImGui::TextDisabled("%02X", byte);
                                ImGui::SameLine();
                            }

                            ImGui::NewLine();

                            // TODO: Clip this somehow

                            // First line
                            {
                                std::array<u8, 0x10> bytes = { 0 };
                                size_t byteCount = std::min<size_t>(0x10 - offset, region.size);
                                ImHexApi::Provider::get()->read(region.address, bytes.data() + offset, byteCount);

                                for (size_t byte = 0; byte < 0x10; byte++) {
                                    if (byte < offset)
                                        ImGui::TextUnformatted("  ");
                                    else
                                        ImGui::Text("%02X", bytes[byte]);
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
                                        ImGui::Text("%02X", bytes[byte]);
                                        ImGui::SameLine();
                                    }
                                    ImGui::NewLine();
                                }
                            }
                        }
                        ImGui::EndChild();

                        if (ImGui::Button("hex.builtin.view.bookmarks.button.jump"_lang))
                            EventManager::post<RequestSelectionChange>(region);
                        ImGui::SameLine(0, 15);

                        if (ImGui::Button("hex.builtin.view.bookmarks.button.remove"_lang))
                            bookmarkToRemove = iter;
                        ImGui::SameLine(0, 15);

                        if (locked) {
                            if (ImGui::Button(ICON_FA_LOCK)) locked = false;
                        } else {
                            if (ImGui::Button(ICON_FA_UNLOCK)) locked = true;
                        }

                        ImGui::NewLine();
                        ImGui::TextUnformatted("hex.builtin.view.bookmarks.header.name"_lang);
                        ImGui::Separator();

                        ImGui::ColorEdit4("hex.builtin.view.bookmarks.header.color"_lang, (float*)&headerColor.Value, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoAlpha | (locked ? ImGuiColorEditFlags_NoPicker : ImGuiColorEditFlags_None));
                        color = headerColor;
                        ImGui::SameLine();

                        if (locked)
                            ImGui::TextUnformatted(name.data());
                        else
                            ImGui::InputText("##nameInput", name.data(), 64);

                        ImGui::NewLine();
                        ImGui::TextUnformatted("hex.builtin.view.bookmarks.header.comment"_lang);
                        ImGui::Separator();

                        if (locked)
                            ImGui::TextWrapped("%s", comment.data());
                        else
                            ImGui::InputTextMultiline("##commentInput", comment.data(), 0xF'FFFF);

                        ImGui::NewLine();

                    }
                    ImGui::PopID();
                    ImGui::PopStyleColor(3);
                    id++;
                }

                if (bookmarkToRemove != bookmarks.end()) {
                    bookmarks.erase(bookmarkToRemove);
                    ProjectFile::markDirty();
                }

            }
            ImGui::EndChild();
        }
        ImGui::End();
    }

    void ViewBookmarks::drawMenu() {

    }

}