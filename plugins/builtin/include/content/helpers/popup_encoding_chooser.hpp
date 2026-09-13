#pragma once

#include <content/helpers/encoding_chooser.hpp>

#include <hex/ui/imgui_imhex_extensions.h>

#include <popups/popup_file_chooser.hpp>
#include <fonts/fonts.hpp>

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace hex::plugin::builtin {

    /**
     * @brief Chooses an encoding table, showing what each one is for
     */
    class PopupEncodingChooser : public ui::PopupNamedFileChooserBase<PopupEncodingChooser> {
    public:
        PopupEncodingChooser(const std::vector<std::fs::path> &basePaths, const std::vector<std::fs::path> &files, const std::vector<hex::fs::ItemFilter> &validExtensions, bool multiple, const std::function<void(std::fs::path)> &callback)
            : PopupEncodingChooser(basePaths, readEncodingChoices(files), validExtensions, multiple, callback) { }

        std::string getEntryName(const std::fs::path &path) override {
            const auto choice = m_choices.find(path);
            if (choice == m_choices.end())
                return wolv::util::toUTF8String(getAdjustedPath(path));

            // The search box matches every word the row shows.
            const auto lines = getEncodingChoiceLines(choice->second, getEncodingFileName(getAdjustedPath(path)));
            return lines.title + " " + lines.subtitle;
        }

    protected:
        bool drawEntry(const std::fs::path &path, const std::string &name, bool selected) override {
            const auto choice = m_choices.find(path);
            if (choice == m_choices.end())
                return PopupNamedFileChooserBase::drawEntry(path, name, selected);

            const auto [title, subtitle] = getEncodingChoiceLines(choice->second, getEncodingFileName(getAdjustedPath(path)));

            const auto startPos = ImGui::GetCursorPos();

            float height = ImGui::GetTextLineHeight();
            if (!subtitle.empty())
                height += ImGui::GetTextLineHeight() * SubtitleScale + ImGui::GetStyle().ItemSpacing.y;

            const bool clicked = ImGui::Selectable("##entry", selected, ImGuiSelectableFlags_NoAutoClosePopups, ImVec2(0, height));
            const auto rowSize = ImGui::GetItemRectSize();

            ImGui::SetCursorPos(startPos);

            fonts::Default().pushBold();
            ImGui::TextUnformatted(title.c_str());
            fonts::Default().pop();

            if (!subtitle.empty()) {
                fonts::Default().push(SubtitleScale);
                ImGuiExt::TextFormattedDisabled("{}", subtitle);
                fonts::Default().pop();
            }

            // The text above draws over the row, so this item alone gives the row its size.
            ImGui::SetCursorPos(startPos);
            ImGui::Dummy(rowSize);

            return clicked;
        }

    private:
        // Reads the tables once, since the list to show and what to say about it come together.
        PopupEncodingChooser(const std::vector<std::fs::path> &basePaths, EncodingChoices choices, const std::vector<hex::fs::ItemFilter> &validExtensions, bool multiple, const std::function<void(std::fs::path)> &callback)
            : PopupNamedFileChooserBase(basePaths, choices.files, validExtensions, multiple, callback),
              m_choices(std::move(choices.entries)) { }

        constexpr static float SubtitleScale = 0.8F;

        std::map<std::fs::path, EncodingChoice> m_choices;
    };

}
