#include "views/view_bookmarks.hpp"

#include <hex/providers/provider.hpp>
#include "helpers/project_file_handler.hpp"

#include <cstring>

namespace hex {

    ViewBookmarks::ViewBookmarks() : View("hex.view.bookmarks.name") {
        EventManager::subscribe<RequestAddBookmark>(this, [](ImHexApi::Bookmarks::Entry bookmark) {
            bookmark.comment.resize(0xF'FFFF);

            if (bookmark.name.empty()) {
                bookmark.name.resize(64);
                std::memset(bookmark.name.data(), 0x00, 64);
                std::strcpy(bookmark.name.data(), hex::format("hex.view.bookmarks.default_title"_lang,
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
    }

    ViewBookmarks::~ViewBookmarks() {
        EventManager::unsubscribe<RequestAddBookmark>(this);
        EventManager::unsubscribe<EventProjectFileLoad>(this);
        EventManager::unsubscribe<EventProjectFileStore>(this);
    }

    void ViewBookmarks::drawContent() {
        if (ImGui::Begin(View::toWindowName("hex.view.bookmarks.name").c_str(), &this->getWindowOpenState())) {
            if (ImGui::BeginChild("##scrolling")) {

                auto &bookmarks = ImHexApi::Bookmarks::getEntries();

                if (bookmarks.empty()) {
                    ImGui::NewLine();
                    ImGui::Indent(30);
                    ImGui::TextWrapped("%s", static_cast<const char*>("hex.view.bookmarks.no_bookmarks"_lang));
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
                    if (ImGui::CollapsingHeader((std::string(name.data()) + "###" + std::to_string((u64)comment.data())).c_str())) {
                        ImGui::TextUnformatted("hex.view.bookmarks.title.info"_lang);
                        ImGui::Separator();
                        ImGui::TextUnformatted(hex::format("hex.view.bookmarks.address"_lang, region.address, region.address + region.size - 1, region.size).c_str());

                        {
                            u8 bytes[10] = { 0 };
                            SharedData::currentProvider->read(region.address, bytes, std::min(region.size, size_t(10)));

                            std::string bytesString;
                            for (u8 i = 0; i < std::min(region.size, size_t(10)); i++) {
                                bytesString += hex::format("{0:02X} ", bytes[i]);
                            }

                            if (region.size > 10) {
                                bytesString.pop_back();
                                bytesString += "...";
                            }

                            ImGui::TextColored(ImColor(0xFF9BC64D), "%s", bytesString.c_str());
                        }
                        if (ImGui::Button("hex.view.bookmarks.button.jump"_lang))
                            EventManager::post<RequestSelectionChange>(region);
                        ImGui::SameLine(0, 15);

                        if (ImGui::Button("hex.view.bookmarks.button.remove"_lang))
                            bookmarkToRemove = iter;
                        ImGui::SameLine(0, 15);

                        if (locked) {
                            if (ImGui::Button(ICON_FA_LOCK)) locked = false;
                        } else {
                            if (ImGui::Button(ICON_FA_UNLOCK)) locked = true;
                        }

                        ImGui::NewLine();
                        ImGui::TextUnformatted("hex.view.bookmarks.header.name"_lang);
                        ImGui::Separator();

                        ImGui::ColorEdit4("hex.view.bookmarks.header.color"_lang, (float*)&headerColor.Value, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoAlpha | (locked ? ImGuiColorEditFlags_NoPicker : ImGuiColorEditFlags_None));
                        color = headerColor;
                        ImGui::SameLine();

                        if (locked)
                            ImGui::TextUnformatted(name.data());
                        else
                            ImGui::InputText("##nameInput", name.data(), 64);

                        ImGui::NewLine();
                        ImGui::TextUnformatted("hex.view.bookmarks.header.comment"_lang);
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

                ImGui::EndChild();
            }
        }
        ImGui::End();
    }

    void ViewBookmarks::drawMenu() {

    }

}