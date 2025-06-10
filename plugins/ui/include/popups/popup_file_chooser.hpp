#pragma once

#include <hex/ui/popup.hpp>

#include <hex/api/localization_manager.hpp>

#include <wolv/utils/string.hpp>
#include <fonts/vscode_icons.hpp>

#include <functional>
#include <string>
#include <vector>
#include <set>
#include <algorithm>
#include <iterator>
#include <ranges>

namespace hex::ui {

    template<typename T>
    class PopupNamedFileChooserBase : public Popup<T> {
    public:
        PopupNamedFileChooserBase(const std::vector<std::fs::path> &basePaths, const std::vector<std::fs::path> &files, const std::vector<hex::fs::ItemFilter> &validExtensions, bool multiple, const std::function<void(std::fs::path)> &callback)
                : hex::Popup<T>("hex.ui.common.choose_file"),
                  m_files(files),
                  m_singleSelectedIndex(0),
                  m_multiSelectedFiles({ }),
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

                m_adjustedPaths[path] = adjustedPath;
            }

            std::sort(m_files.begin(), m_files.end(), [](const auto &a, const auto &b) {
                return a < b;
            });
        }

        void drawContent() override {
            bool doubleClickedOrEnter = false;

            if (m_justOpened) {
                ImGui::SetKeyboardFocusHere();
                m_justOpened = false;
            }

            bool filterOrListFocused = false;
            ImGui::PushItemWidth(-1);
            ImGuiExt::InputTextIcon("##search", ICON_VS_FILTER, m_filter);
            filterOrListFocused = ImGui::IsItemFocused();
            ImGui::PopItemWidth();

            Files::iterator singleSelectedIt;
            if (ImGui::BeginListBox("##files", scaled(ImVec2(500, 400)))) {
                bool lf = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
                filterOrListFocused |= lf;

                int filtered_index = 0;
                for (auto fileIt = m_files.begin(); fileIt != m_files.end(); ++fileIt) {
                    const auto &path = *fileIt;

                    const auto &pathNameString = getEntryName(path);
                    if (!m_filter.empty() && !pathNameString.contains(m_filter))
                        continue;

                    ImGui::PushID(&*fileIt);

                    bool selected = false;
                    if (!m_multiple) {
                        selected = (filtered_index==m_singleSelectedIndex);
                        if (selected)
                            singleSelectedIt = fileIt;
                    } else
                        selected = std::ranges::contains(m_multiSelectedFiles, fileIt);
                    if (ImGui::Selectable(pathNameString.c_str(), selected, ImGuiSelectableFlags_NoAutoClosePopups)) {
                        if (!m_multiple) {
                            m_singleSelectedFile = fileIt;
                            m_singleSelectedIndex = filtered_index;

                        } else {
                            if (selected) { // TODO: Is this logic backwards? "!selected"?
                                auto it = std::ranges::find(m_multiSelectedFiles, fileIt);
                                if (it!=m_multiSelectedFiles.end())
                                    m_multiSelectedFiles.erase(it);
                            } else {
                                m_multiSelectedFiles.push_back(fileIt);
                            }
                        }
                    }
                    if (selected)
                        ImGui::SetScrollHereY(0.5f);
 
                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
                        doubleClickedOrEnter = true;

                    ImGuiExt::InfoTooltip(wolv::util::toUTF8String(path).c_str());

                    ImGui::PopID();

                    ++filtered_index;
                }

                ImGui::EndListBox();

                if (!m_multiple && filterOrListFocused) {
                    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
                        --m_singleSelectedIndex;
                    else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
                        ++m_singleSelectedIndex;
                    else
                        doubleClickedOrEnter |= ImGui::IsKeyPressed(ImGuiKey_Enter);

                    m_singleSelectedIndex = std::clamp(m_singleSelectedIndex, 0, filtered_index-1);
                }
                
            }

            if (ImGui::Button("hex.ui.common.open"_lang) || doubleClickedOrEnter) {
                if (!m_multiple)
                    m_openCallback(*singleSelectedIt);
                else {
                    for (const auto &it : m_multiSelectedFiles)
                        m_openCallback(*it);  
                }
                    
                Popup<T>::close();
            }

            ImGui::SameLine();

            if (ImGui::Button("hex.ui.common.browse"_lang)) {
                fs::openFileBrowser(fs::DialogMode::Open, m_validExtensions, [this](const auto &path) {
                    m_openCallback(path);
                    Popup<T>::close();
                }, {}, m_multiple);
            }

            if (ImGui::IsKeyPressed(ImGuiKey_Escape))
                this->close();
        }

        [[nodiscard]] ImGuiWindowFlags getFlags() const override {
            return ImGuiWindowFlags_AlwaysAutoResize;
        }


    protected:
        const std::fs::path& getAdjustedPath(const std::fs::path &path) const {
            return m_adjustedPaths.at(path);
        }

        virtual std::string getEntryName(const std::fs::path &path) = 0;

    private:
        static bool isSubpath(const std::fs::path &basePath, const std::fs::path &path) {
            auto relativePath = std::fs::relative(path, basePath);

            return !relativePath.empty() && relativePath.native()[0] != '.';
        }

    private:
        std::string m_filter;
        using Files = std::vector<std::fs::path>;
        Files m_files;
        std::map<std::fs::path, std::fs::path> m_adjustedPaths;
        Files::const_iterator m_singleSelectedFile;
        int m_singleSelectedIndex;
        std::vector<Files::const_iterator> m_multiSelectedFiles;
        std::function<void(std::fs::path)> m_openCallback;
        std::vector<hex::fs::ItemFilter> m_validExtensions;
        bool m_multiple = false;
        bool m_justOpened = true;
    };

    class PopupNamedFileChooser : public PopupNamedFileChooserBase<PopupNamedFileChooser> {
    public:
        PopupNamedFileChooser(const std::vector<std::fs::path> &basePaths, const std::vector<std::fs::path> &files, const std::vector<hex::fs::ItemFilter> &validExtensions, bool multiple, const std::function<std::string(std::fs::path, std::fs::path)> &nameCallback, const std::function<void(std::fs::path)> &callback)
        : PopupNamedFileChooserBase(basePaths, files, validExtensions, multiple, callback), m_nameCallback(nameCallback) { }

        std::string getEntryName(const std::fs::path &path) override {
            return m_nameCallback(path, getAdjustedPath(path));
        }

    private:
        std::function<std::string(std::fs::path, std::fs::path)> m_nameCallback;
    };

    class PopupFileChooser : public PopupNamedFileChooserBase<PopupFileChooser> {
    public:
        PopupFileChooser(const std::vector<std::fs::path> &basePaths, const std::vector<std::fs::path> &files, const std::vector<hex::fs::ItemFilter> &validExtensions, bool multiple, const std::function<void(std::fs::path)> &callback)
            : PopupNamedFileChooserBase(basePaths, files, validExtensions, multiple, callback) { }

        std::string getEntryName(const std::fs::path &path) override {
            return wolv::util::toUTF8String(getAdjustedPath(path));
        }
    };

}