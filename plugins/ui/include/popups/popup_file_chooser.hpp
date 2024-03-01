#pragma once

#include <hex/ui/popup.hpp>

#include <hex/api/localization_manager.hpp>

#include <wolv/utils/string.hpp>
#include <fonts/codicons_font.h>

#include <functional>
#include <string>
#include <vector>
#include <set>

namespace hex::ui {

    template<typename T>
    class PopupNamedFileChooserBase : public Popup<T> {
    public:
        PopupNamedFileChooserBase(const std::vector<std::fs::path> &basePaths, const std::vector<std::fs::path> &files, const std::vector<hex::fs::ItemFilter> &validExtensions, bool multiple, const std::function<std::string(const std::fs::path &)> &nameCallback, const std::function<void(std::fs::path)> &callback)
                : hex::Popup<T>("hex.ui.common.choose_file"),
                  m_selectedFiles({ }),
                  m_nameCallback(nameCallback),
                  m_openCallback(callback),
                  m_validExtensions(validExtensions),
                  m_multiple(multiple) {

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

                m_files.push_back({ path, adjustedPath });
            }

            std::sort(m_files.begin(), m_files.end(), [](const auto &a, const auto &b) {
                return a.first < b.first;
            });
        }

        void drawContent() override {
            bool doubleClicked = false;

            if (m_justOpened) {
                ImGui::SetKeyboardFocusHere();
                m_justOpened = false;
            }

            ImGui::PushItemWidth(-1);
            ImGuiExt::InputTextIcon("##search", ICON_VS_FILTER, m_filter);
            ImGui::PopItemWidth();

            if (ImGui::BeginListBox("##files", scaled(ImVec2(500, 400)))) {
                for (auto fileIt = m_files.begin(); fileIt != m_files.end(); ++fileIt) {
                    const auto &[path, pathName] = *fileIt;

                    const auto &pathNameString = m_nameCallback(pathName);
                    if (!m_filter.empty() && !pathNameString.contains(m_filter))
                        continue;

                    ImGui::PushID(&*fileIt);

                    bool selected = m_selectedFiles.contains(fileIt);
                    if (ImGui::Selectable(pathNameString.c_str(), selected, ImGuiSelectableFlags_DontClosePopups)) {
                        if (!m_multiple) {
                            m_selectedFiles.clear();
                            m_selectedFiles.insert(fileIt);
                        } else {
                            if (selected) {
                                m_selectedFiles.erase(fileIt);
                            } else {
                                m_selectedFiles.insert(fileIt);
                            }
                        }
                    }

                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
                        doubleClicked = true;

                    ImGuiExt::InfoTooltip(wolv::util::toUTF8String(path).c_str());

                    ImGui::PopID();
                }

                ImGui::EndListBox();
            }

            if (ImGui::Button("hex.ui.common.open"_lang) || doubleClicked) {
                for (const auto &it : m_selectedFiles)
                    m_openCallback(it->first);
                Popup<T>::close();
            }

            ImGui::SameLine();

            if (ImGui::Button("hex.ui.common.browse"_lang)) {
                fs::openFileBrowser(fs::DialogMode::Open, m_validExtensions, [this](const auto &path) {
                    m_openCallback(path);
                    Popup<T>::close();
                }, {}, m_multiple);
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
        std::string m_filter;
        std::vector<std::pair<std::fs::path, std::fs::path>> m_files;
        std::set<std::vector<std::pair<std::fs::path, std::fs::path>>::const_iterator> m_selectedFiles;
        std::function<std::string(const std::fs::path &)> m_nameCallback;
        std::function<void(std::fs::path)> m_openCallback;
        std::vector<hex::fs::ItemFilter> m_validExtensions;
        bool m_multiple = false;
        bool m_justOpened = true;
    };

    class PopupNamedFileChooser : public PopupNamedFileChooserBase<PopupNamedFileChooser> {
    public:
        PopupNamedFileChooser(const std::vector<std::fs::path> &basePaths, const std::vector<std::fs::path> &files, const std::vector<hex::fs::ItemFilter> &validExtensions, bool multiple, const std::function<std::string(const std::fs::path &)> &nameCallback, const std::function<void(std::fs::path)> &callback)
        : PopupNamedFileChooserBase(basePaths, files, validExtensions, multiple, nameCallback, callback) { }
    };

    class PopupFileChooser : public PopupNamedFileChooserBase<PopupFileChooser> {
    public:
        PopupFileChooser(const std::vector<std::fs::path> &basePaths, const std::vector<std::fs::path> &files, const std::vector<hex::fs::ItemFilter> &validExtensions, bool multiple, const std::function<void(std::fs::path)> &callback)
        : PopupNamedFileChooserBase(basePaths, files, validExtensions, multiple, nameCallback, callback) { }

        static std::string nameCallback(const std::fs::path &path) {
            return wolv::util::toUTF8String(path);
        }
    };

}