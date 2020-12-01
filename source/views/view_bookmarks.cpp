#include "views/view_bookmarks.hpp"

#include "providers/provider.hpp"
#include "helpers/project_file_handler.hpp"

#include <cstring>

namespace hex {

    ViewBookmarks::ViewBookmarks(prv::Provider* &dataProvider) : View("Bookmarks"), m_dataProvider(dataProvider) {
        View::subscribeEvent(Events::AddBookmark, [this](const void *userData) {
            Bookmark bookmark = *reinterpret_cast<const Bookmark*>(userData);
            bookmark.name.resize(64);
            bookmark.comment.resize(0xF'FFFF);

            if (bookmark.name.empty()) {
                std::memset(bookmark.name.data(), 0x00, 64);
                std::strcpy(bookmark.name.data(), ("Bookmark " + std::to_string(this->m_bookmarks.size() + 1)).c_str());
            }

            if (bookmark.comment.empty())
                std::memset(bookmark.comment.data(), 0x00, 0xF'FFFF);

            this->m_bookmarks.push_back(bookmark);
            ProjectFile::markDirty();
        });

        View::subscribeEvent(Events::ProjectFileLoad, [this](const void*) {
            this->m_bookmarks = ProjectFile::getBookmarks();
        });
        View::subscribeEvent(Events::ProjectFileStore, [this](const void*) {
            ProjectFile::setBookmarks(this->m_bookmarks);
        });
    }

    ViewBookmarks::~ViewBookmarks() {
        View::unsubscribeEvent(Events::AddBookmark);
        View::unsubscribeEvent(Events::ProjectFileLoad);
        View::unsubscribeEvent(Events::ProjectFileStore);
    }

    void ViewBookmarks::createView() {
        if (ImGui::Begin("Bookmarks", &this->getWindowOpenState())) {
            if (ImGui::BeginChild("##scrolling")) {

                u32 id = 1;
                std::list<Bookmark>::const_iterator bookmarkToRemove = this->m_bookmarks.end();
                for (auto iter = this->m_bookmarks.begin(); iter != this->m_bookmarks.end(); iter++) {
                    auto &[region, name, comment] = *iter;

                    if (ImGui::CollapsingHeader((std::string(name.data()) + "###" + std::to_string((u64)comment.data())).c_str())) {
                        ImGui::TextUnformatted("Information");
                        ImGui::Separator();
                        ImGui::Text("0x%08lx : 0x%08lx (%lu bytes)", region.address, region.address + region.size - 1, region.size);

                        {
                            u8 bytes[10] = { 0 };
                            this->m_dataProvider->read(region.address, bytes, std::min(region.size, size_t(10)));

                            std::string bytesString;
                            for (u8 i = 0; i < std::min(region.size, size_t(10)); i++) {
                                bytesString += hex::format("%02X ", bytes[i]);
                            }

                            if (region.size > 10) {
                                bytesString.pop_back();
                                bytesString += "...";
                            }

                            ImGui::TextColored(ImColor(0xFF9BC64D), bytesString.c_str());
                        }

                        if (ImGui::Button("Jump to"))
                            View::postEvent(Events::SelectionChangeRequest, &region);

                        ImGui::SameLine(0, 15);

                        if (ImGui::Button("Remove"))
                            bookmarkToRemove = iter;

                        ImGui::NewLine();
                        ImGui::TextUnformatted("Name");
                        ImGui::Separator();
                        ImGui::PushID(id);
                        ImGui::InputText("##nolabel", name.data(), 64);
                        ImGui::PopID();
                        ImGui::NewLine();
                        ImGui::TextUnformatted("Comment");
                        ImGui::Separator();
                        ImGui::PushID(id + 1);
                        ImGui::InputTextMultiline("##nolabel", comment.data(), 0xF'FFFF);
                        ImGui::PopID();
                        ImGui::NewLine();

                        id += 2;
                    }
                }

                if (bookmarkToRemove != this->m_bookmarks.end()) {
                    this->m_bookmarks.erase(bookmarkToRemove);
                    ProjectFile::markDirty();
                }

                ImGui::EndChild();
            }
        }
        ImGui::End();
    }

    void ViewBookmarks::createMenu() {

    }

}