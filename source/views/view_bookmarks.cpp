#include "views/view_bookmarks.hpp"

#include <hex/providers/provider.hpp>
#include "helpers/project_file_handler.hpp"

#include <cstring>

namespace hex {

    ViewBookmarks::ViewBookmarks(std::list<Bookmark> &bookmarks) : View("Bookmarks"), m_bookmarks(bookmarks) {
        View::subscribeEvent(Events::AddBookmark, [this](const void *userData) {
            Bookmark bookmark = *reinterpret_cast<const Bookmark*>(userData);
            bookmark.name.resize(64);
            bookmark.comment.resize(0xF'FFFF);

            std::memset(bookmark.name.data(), 0x00, 64);
            std::strcpy(bookmark.name.data(), hex::format("Bookmark [0x%lX - 0x%lX]",
                                                          bookmark.region.address,
                                                          bookmark.region.address + bookmark.region.size - 1).c_str());

            if (bookmark.comment.empty())
                std::memset(bookmark.comment.data(), 0x00, 0xF'FFFF);

            bookmark.color = ImGui::GetColorU32(ImGuiCol_Header);

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

    void ViewBookmarks::drawContent() {
        if (ImGui::Begin("Bookmarks", &this->getWindowOpenState())) {
            if (ImGui::BeginChild("##scrolling")) {

                if (this->m_bookmarks.empty()) {
                    ImGui::NewLine();
                    ImGui::Indent(30);
                    ImGui::TextWrapped("No bookmarks created yet. Add one with Edit -> Add Bookmark");
                }

                u32 id = 1;
                std::list<Bookmark>::const_iterator bookmarkToRemove = this->m_bookmarks.end();
                for (auto iter = this->m_bookmarks.begin(); iter != this->m_bookmarks.end(); iter++) {
                    auto &[region, name, comment, color] = *iter;

                    auto headerColor = ImColor(color);
                    auto hoverColor = ImColor(color);
                    hoverColor.Value.w *= 1.3F;

                    ImGui::PushID(id);
                    ImGui::PushStyleColor(ImGuiCol_Header, color);
                    ImGui::PushStyleColor(ImGuiCol_HeaderActive, color);
                    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, u32(hoverColor));
                    if (ImGui::CollapsingHeader((std::string(name.data()) + "###" + std::to_string((u64)comment.data())).c_str())) {
                        ImGui::TextUnformatted("Information");
                        ImGui::Separator();
                        ImGui::Text("0x%08lx : 0x%08lx (%lu bytes)", region.address, region.address + region.size - 1, region.size);

                        {
                            u8 bytes[10] = { 0 };
                            (SharedData::currentProvider)->read(region.address, bytes, std::min(region.size, size_t(10)));

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
                        ImGui::InputText("##nameInput", name.data(), 64);
                        ImGui::SameLine();
                        ImGui::ColorEdit4("Color", (float*)&headerColor.Value, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoAlpha);
                        color = headerColor;
                        ImGui::NewLine();
                        ImGui::TextUnformatted("Comment");
                        ImGui::Separator();
                        ImGui::InputTextMultiline("##colorInput", comment.data(), 0xF'FFFF);
                        ImGui::NewLine();

                    }
                    ImGui::PopID();
                    ImGui::PopStyleColor(3);
                    id++;
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

    void ViewBookmarks::drawMenu() {

    }

}