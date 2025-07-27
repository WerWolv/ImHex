#include <imgui.h>
#include <fonts/fonts.hpp>
#include <hex/api/task_manager.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/helpers/logger.hpp>
#include <hex/ui/imgui_imhex_extensions.h>
#include <romfs/romfs.hpp>
#include <ui/markdown.hpp>

namespace hex::ui {

    Markdown::Markdown(const std::string &text) : m_text(text) {
        m_mdRenderer = MD_RENDERER();
        m_mdRenderer.flags = MD_DIALECT_GITHUB | MD_FLAG_NOHTML | MD_FLAG_TABLES | MD_FLAG_NOHTMLBLOCKS | MD_FLAG_TASKLISTS;
        m_mdRenderer.enter_block = [](MD_BLOCKTYPE type, void *detail, void *userdata) -> int {
            auto &self = *static_cast<Markdown*>(userdata);

            switch (type) {
                case MD_BLOCK_H:
                    fonts::Default().pushBold(ImGui::GetFontSize() * std::lerp(2.0F, 1.1F, ((MD_BLOCK_H_DETAIL*)detail)->level / 6.0F));
                    break;
                case MD_BLOCK_HR:
                    ImGui::Separator();
                    break;
                case MD_BLOCK_CODE: {
                    ImGui::NewLine();
                    bool open = ImGui::BeginTable(self.getElementId().c_str(), 1, ImGuiTableFlags_Borders);
                    self.m_tableVisibleStack.emplace_back(open);
                    if (open) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, ImGui::GetColorU32(ImGuiCol_MenuBarBg));
                    }
                    break;
                }
                case MD_BLOCK_TABLE: {
                    const auto *table = static_cast<MD_BLOCK_TABLE_DETAIL*>(detail);
                    ImGui::NewLine();
                    bool open = ImGui::BeginTable(self.getElementId().c_str(), table->col_count, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX);
                    self.m_tableVisibleStack.emplace_back(open);
                    break;
                }
                case MD_BLOCK_TD: {
                    if (self.inTable())
                        ImGui::TableNextColumn();
                    break;
                }
                case MD_BLOCK_TH: {
                    if (self.inTable())
                        ImGui::TableNextColumn();
                    break;
                }
                case MD_BLOCK_TBODY:
                    if (self.inTable())
                        ImGui::TableNextRow();
                    break;
                case MD_BLOCK_THEAD:
                    if (self.inTable())
                        ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
                    break;
                case MD_BLOCK_QUOTE:
                    ImGui::Indent();
                    break;
                case MD_BLOCK_UL: {
                    if (self.m_listIndent > 0) {
                        ImGui::NewLine();
                        ImGui::Indent();
                    }
                    self.m_listIndent += 1;
                    break;
                }
                case MD_BLOCK_LI: {
                    const auto *li = static_cast<MD_BLOCK_LI_DETAIL*>(detail);
                    ImGui::Bullet();

                    if (li->is_task) {
                        bool checked = li->task_mark != ' ';
                        ImGui::BeginDisabled();
                        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 0, 0 });
                        ImGui::Checkbox(self.getElementId().c_str(), &checked);
                        ImGui::PopStyleVar();
                        ImGui::EndDisabled();
                        ImGui::SameLine();
                    }
                    break;
                }
                default:
                    break;
            }

            std::ignore = detail;
            std::ignore = userdata;
            return 0; // No special handling for block enter
        };
        m_mdRenderer.leave_block = [](MD_BLOCKTYPE type, void *detail, void *userdata) -> int {
            auto &self = *static_cast<Markdown*>(userdata);

            switch (type) {
                case MD_BLOCK_H:
                    fonts::Default().pop();
                    ImGui::NewLine();
                    break;
                case MD_BLOCK_CODE:
                    if (self.inTable()) {
                        ImGui::EndTable();
                        self.m_tableVisibleStack.pop_back();
                    }
                    break;
                case MD_BLOCK_TABLE:
                    if (self.inTable()) {
                        ImGui::EndTable();
                        self.m_tableVisibleStack.pop_back();
                    }
                    break;
                case MD_BLOCK_QUOTE:
                    ImGui::Unindent();
                    break;
                case MD_BLOCK_UL:
                    if (self.m_listIndent > 0) {
                        ImGui::Unindent();
                    }
                    self.m_listIndent -= 1;
                    ImGui::SameLine();
                    break;
                case MD_BLOCK_LI:
                    ImGui::NewLine();
                    break;
                default:
                    break;
            }
            std::ignore = detail;
            std::ignore = userdata;
            return 0; // No special handling for block leave
        };
        m_mdRenderer.enter_span = [](MD_SPANTYPE type, void *detail, void *userdata) -> int {
            auto &self = *static_cast<Markdown*>(userdata);

            std::ignore = detail;
            std::ignore = userdata;
            switch (type) {
                case MD_SPAN_STRONG:
                    fonts::Default().pushBold();
                    break;
                case MD_SPAN_EM:
                    fonts::Default().pushItalic();
                    break;
                case MD_SPAN_A: {
                    const auto *link = static_cast<MD_SPAN_A_DETAIL*>(detail);
                    self.m_currentLink = std::string(link->href.text, link->href.size);
                    break;
                }
                case MD_SPAN_IMG:
                    ImGui::NewLine();
                    if (ImGui::BeginChild(self.getElementId().c_str(), { 100, 100 }, ImGuiChildFlags_Borders)) {

                    }
                    ImGui::EndChild();
                    self.m_drawingImageAltText = true;
                    break;
                default:
                    break;
            }
            return 0; // No special handling for span enter
        };
        m_mdRenderer.leave_span = [](MD_SPANTYPE type, void *detail, void *userdata) -> int {
            std::ignore = detail;
            std::ignore = userdata;
            switch (type) {
                case MD_SPAN_STRONG:
                case MD_SPAN_EM:
                    fonts::Default().pop();
                    break;
                default:
                    break;
            }
            return 0; // No special handling for span leave
        };
        m_mdRenderer.text = [](MD_TEXTTYPE type, const MD_CHAR *text, MD_SIZE size, void *userdata) -> int {
            auto &self = *static_cast<Markdown*>(userdata);

            std::ignore = type;
            std::ignore = userdata;
            std::string_view sv(text, size);

            if (sv.size() == 1 && sv[0] == '\n') {
                ImGui::NewLine();
                return 0;
            }

            while (!sv.empty()) {
                const auto c = sv.front();
                const bool whiteSpaces = std::isspace(static_cast<u8>(c)) != 0;

                size_t end = 0;
                while (end < sv.size() && (std::isspace(static_cast<u8>(sv[end])) != 0) == whiteSpaces) {
                    ++end;
                }

                std::string_view word = sv.substr(0, end);

                auto textSize = ImGui::CalcTextSize(word.begin(), word.end());
                if (ImGui::GetCursorPosX() > ImGui::GetStyle().WindowPadding.x && ImGui::GetCursorPosX() + textSize.x > ImGui::GetWindowSize().x && !whiteSpaces) {
                    ImGui::NewLine();
                }

                switch (type) {
                    case MD_TEXT_NORMAL:
                    case MD_TEXT_ENTITY:
                        if (!self.m_currentLink.empty()) {
                            if (ImGuiExt::Hyperlink(std::string(word.data(), word.data() + word.size()).c_str())) {
                                hex::openWebpage(self.m_currentLink);
                            }
                            ImGui::SetItemTooltip("%s", self.m_currentLink.c_str());
                            self.m_currentLink.clear();
                        } else if (self.m_drawingImageAltText) {
                            if (ImGui::IsItemHovered()) {
                                if (ImGui::BeginTooltip()) {
                                    ImGui::TextUnformatted(word.data(), word.data() + word.size());
                                    ImGui::SameLine(0, 0);
                                    ImGui::EndTooltip();
                                }
                            }
                        } else {
                            ImGui::TextUnformatted(word.data(), word.data() + word.size());
                        }
                        break;
                    case MD_TEXT_NULLCHAR:
                        ImGui::TextUnformatted("�");
                        break;
                    case MD_TEXT_CODE:
                        ImGui::GetWindowDrawList()->AddRectFilled(ImGui::GetCursorScreenPos(), ImGui::GetCursorScreenPos() + textSize, ImGui::GetColorU32(ImGuiCol_MenuBarBg));
                        ImGui::TextUnformatted(word.data(), word.data() + word.size());
                        break;
                    default:
                        break;
                }

                ImGui::SameLine(0, 0);

                sv.remove_prefix(end);  // Remove the word
            }

            self.m_drawingImageAltText = false;

            return 0; // Continue parsing
        };
        m_mdRenderer.debug_log = [](const char *msg, void *userdata) {
            std::ignore = userdata;
            log::debug("Markdown debug: {}", msg);
        };
    }

    void Markdown::draw() {
        m_elementId = 1;
        md_parse(m_text.c_str(), m_text.size(), &m_mdRenderer, this);
    }

    bool Markdown::inTable() const {
        if (m_tableVisibleStack.empty())
            return false;
        return m_tableVisibleStack.back() != 0;
    }

    std::string Markdown::getElementId() {
        return hex::format("##Element{}", m_elementId++);
    }

}