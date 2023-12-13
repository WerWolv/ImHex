#pragma once

#include <hex/ui/popup.hpp>

#include <hex/api/localization_manager.hpp>

#include <wolv/utils/string.hpp>

#include <functional>
#include <string>

namespace hex::plugin::builtin {

    class PopupFileChooser : public Popup<PopupFileChooser> {
    public:
        PopupFileChooser(const std::vector<std::fs::path> &basePaths, const std::vector<std::fs::path> &files, const std::vector<hex::fs::ItemFilter> &validExtensions, bool multiple, const std::function<void(std::fs::path)> &callback)
                : hex::Popup<PopupFileChooser>("hex.builtin.common.choose_file"),
                  m_indices({ }),
                  m_openCallback(callback),
                  m_validExtensions(validExtensions), m_multiple(multiple) {

            for (const auto &path : files) {
                std::fs::path adjustedPath;
                for (const auto &basePath : basePaths) {
                    if (isSubpath(basePath, path)) {
                        adjustedPath = std::fs::relative(path, basePath);
                        break;
                    }
                }

                if (adjustedPath.empty())
                    adjustedPath = path.filename();

                this->m_files.push_back({ path, adjustedPath });
            }

            std::sort(this->m_files.begin(), this->m_files.end(), [](const auto &a, const auto &b) {
                return a.first < b.first;
            });
        }

        void drawContent() override {
            bool doubleClicked = false;

            if (ImGui::BeginListBox("##files", scaled(ImVec2(500, 400)))) {
                u32 index = 0;
                for (auto &[path, pathName] : this->m_files) {
                    ImGui::PushID(index);

                    bool selected = this->m_indices.contains(index);
                    if (ImGui::Selectable(wolv::util::toUTF8String(pathName).c_str(), selected, ImGuiSelectableFlags_DontClosePopups)) {
                        if (!this->m_multiple) {
                            this->m_indices.clear();
                            this->m_indices.insert(index);
                        } else {
                            if (selected) {
                                this->m_indices.erase(index);
                            } else {
                                this->m_indices.insert(index);
                            }
                        }
                    }

                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
                        doubleClicked = true;

                    ImGuiExt::InfoTooltip(wolv::util::toUTF8String(path).c_str());

                    ImGui::PopID();
                    index++;
                }

                ImGui::EndListBox();
            }

            if (ImGui::Button("hex.builtin.common.open"_lang) || doubleClicked) {
                for (const auto &index : this->m_indices)
                    this->m_openCallback(this->m_files[index].first);
                Popup::close();
            }

            ImGui::SameLine();

            if (ImGui::Button("hex.builtin.common.browse"_lang)) {
                fs::openFileBrowser(fs::DialogMode::Open, this->m_validExtensions, [this](const auto &path) {
                    this->m_openCallback(path);
                    Popup::close();
                }, {}, this->m_multiple);
            }

            if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Escape)))
                this->close();
        }

        [[nodiscard]] ImGuiWindowFlags getFlags() const override {
            return ImGuiWindowFlags_AlwaysAutoResize;
        }

    private:
        static bool isSubpath(const std::fs::path &basePath, const std::fs::path &path) {
            auto relativePath = std::fs::relative(path, basePath);

            return !relativePath.empty() && relativePath.native()[0] != '.';
        }

    private:
        std::set<u32> m_indices;
        std::vector<std::pair<std::fs::path, std::fs::path>> m_files;
        std::function<void(std::fs::path)> m_openCallback;
        std::vector<hex::fs::ItemFilter> m_validExtensions;
        bool m_multiple = false;
    };

}