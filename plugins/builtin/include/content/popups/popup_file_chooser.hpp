#include <hex/ui/popup.hpp>

#include <hex/api/localization.hpp>

#include <functional>
#include <string>

namespace hex::plugin::builtin {

    class PopupFileChooser : public Popup<PopupFileChooser> {
    public:
        PopupFileChooser(const std::vector<std::fs::path> &files, const std::vector<nfdfilteritem_t> &validExtensions, bool multiple, const std::function<void(std::fs::path)> &callback)
                : hex::Popup<PopupFileChooser>("hex.builtin.common.choose_file", false),
                  m_indices({ }), m_files(files),
                  m_openCallback(callback),
                  m_validExtensions(validExtensions), m_multiple(multiple) { }

        void drawContent() override {
            bool doubleClicked = false;

            if (ImGui::BeginListBox("##files", scaled(ImVec2(500, 400)))) {
                u32 index = 0;
                for (auto &path : this->m_files) {
                    ImGui::PushID(index);

                    bool selected = this->m_indices.contains(index);
                    if (ImGui::Selectable(wolv::util::toUTF8String(path.filename()).c_str(), selected, ImGuiSelectableFlags_DontClosePopups)) {
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

                    ImGui::InfoTooltip(wolv::util::toUTF8String(path).c_str());

                    ImGui::PopID();
                    index++;
                }

                ImGui::EndListBox();
            }

            if (ImGui::Button("hex.builtin.common.open"_lang) || doubleClicked) {
                for (const auto &index : this->m_indices)
                    this->m_openCallback(this->m_files[index]);
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();

            if (ImGui::Button("hex.builtin.common.browse"_lang)) {
                fs::openFileBrowser(fs::DialogMode::Open, this->m_validExtensions, [this](const auto &path) {
                    this->m_openCallback(path);
                    ImGui::CloseCurrentPopup();
                }, {}, this->m_multiple);
            }
        }

        [[nodiscard]] ImGuiWindowFlags getFlags() const override {
            return ImGuiWindowFlags_AlwaysAutoResize;
        }

    private:
        std::set<u32> m_indices;
        std::vector<std::fs::path> m_files;
        std::function<void(std::fs::path)> m_openCallback;
        std::vector<nfdfilteritem_t> m_validExtensions;
        bool m_multiple = false;
    };

}