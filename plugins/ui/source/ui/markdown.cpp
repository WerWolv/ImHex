#include <ui/markdown.hpp>

#include <imgui.h>
#include <fonts/fonts.hpp>
#include <hex/api/task_manager.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/helpers/logger.hpp>
#include <hex/ui/imgui_imhex_extensions.h>
#include <romfs/romfs.hpp>

#include <chrono>
#include <fonts/vscode_icons.hpp>
#include <hex/helpers/crypto.hpp>
#include <hex/helpers/http_requests.hpp>

#include <hex/helpers/scaling.hpp>
#include <hex/helpers/utils.hpp>
#include <utility>

#include <md4c.h>

namespace hex::ui {

    Markdown::Markdown(std::string text) : m_text(std::move(text)) {
        m_initialized = true;
        m_mdRenderer.flags = MD_DIALECT_GITHUB;
        m_mdRenderer.enter_block = [](MD_BLOCKTYPE type, void *detail, void *userdata) -> int {
            auto &self = *static_cast<Markdown*>(userdata);

            switch (type) {
                case MD_BLOCK_DOC:
                    self.m_firstLine = true;
                    return 0;
                case MD_BLOCK_H:
                    if (!self.m_firstLine) {
                        ImGui::NewLine();
                        ImGui::NewLine();
                    }
                    fonts::Default().pushBold(std::lerp(2.0F, 1.1F, ((MD_BLOCK_H_DETAIL*)detail)->level / 6.0F));
                    break;
                case MD_BLOCK_HR:
                    ImGui::NewLine();
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
                case MD_BLOCK_TD:
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
                    if (!self.m_quoteStarts.empty())
                        ImGui::NewLine();
                    self.m_quoteStarts.emplace_back(ImGui::GetCursorScreenPos());
                    self.m_quoteStart = true;
                    break;
                case MD_BLOCK_UL: {
                    ImGui::NewLine();
                    if (self.m_listIndent > 0) {
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
                case MD_BLOCK_P:
                    if (!self.m_firstLine)
                        ImGui::NewLine();
                    break;
                default:
                    break;
            }

            self.m_firstLine = false;

            std::ignore = detail;
            std::ignore = userdata;
            return 0; // No special handling for block enter
        };
        m_mdRenderer.leave_block = [](MD_BLOCKTYPE type, void *detail, void *userdata) -> int {
            auto &self = *static_cast<Markdown*>(userdata);

            switch (type) {
                case MD_BLOCK_H:
                    fonts::Default().pop();
                    break;
                case MD_BLOCK_CODE:
                case MD_BLOCK_TABLE:
                    if (self.inTable()) {
                        ImGui::EndTable();
                        self.m_tableVisibleStack.pop_back();
                    }
                    break;
                case MD_BLOCK_QUOTE:
                    if (!self.m_quoteNeedsChildEnd.empty()) {
                        if (self.m_quoteNeedsChildEnd.back()) {
                            ImGuiExt::EndSubWindow();
                            ImGui::PopStyleColor();
                        } else {
                            ImGui::Unindent();
                            ImGui::GetWindowDrawList()->AddLine(
                                self.m_quoteStarts.back(),
                                ImGui::GetCursorScreenPos() + ImVec2(0, ImGui::GetTextLineHeight()),
                                ImGui::GetColorU32(ImGuiCol_Separator),
                                3_scaled
                            );
                            self.m_quoteStarts.pop_back();
                        }
                        self.m_quoteNeedsChildEnd.pop_back();
                    }

                    break;
                case MD_BLOCK_UL:
                    if (self.m_listIndent > 1) {
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
                case MD_SPAN_IMG: {
                    ImGui::NewLine();
                    u32 id = self.m_elementId;
                    if (auto futureImageIt = self.m_futureImages.find(id); futureImageIt != self.m_futureImages.end()) {
                        auto &[_, future] = *futureImageIt;
                        if (future.valid() && future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                            self.m_images[id] = std::move(future.get().get());
                            self.m_futureImages.erase(futureImageIt);
                        } else {
                            ImGui::TextUnformatted("Loading image...");
                        }
                    } else if (auto imageIt = self.m_images.find(id); imageIt != self.m_images.end()) {
                        const auto &[_, image] = *imageIt;
                        if (image.isValid()) {
                            auto size = image.getSize();
                            const auto available = ImGui::GetContentRegionAvail().x;
                            if (size.x > available) {
                                size.x = available;
                                size.y = available / image.getAspectRatio();
                            }

                            ImGui::Image(image, size);
                        } else {
                            if (ImGui::BeginChild(self.getElementId().c_str(), { 100, 100 }, ImGuiChildFlags_Borders)) {
                                ImGui::TextUnformatted("???");
                            }
                            ImGui::EndChild();
                        }
                    } else {
                        const auto *img = static_cast<MD_SPAN_IMG_DETAIL*>(detail);
                        std::string path = { img->src.text, img->src.size };

                        self.m_futureImages.emplace(id, std::async(std::launch::async, [path = std::move(path), romfsLookup = self.m_romfsFileReader]() -> wolv::container::Lazy<ImGuiExt::Texture> {
                            std::vector<u8> data;
                            if (path.starts_with("data:image/")) {
                                auto pos = path.find(';');
                                if (pos != std::string::npos) {
                                    auto base64 = path.substr(pos + 1);
                                    if (base64.starts_with("base64,")) {
                                        base64 = base64.substr(7);
                                    }
                                    data = crypt::decode64({ base64.begin(), base64.end() });
                                }
                            } else if (path.starts_with("http://") || path.starts_with("https://")) {
                                HttpRequest request("GET", path);
                                const auto result = request.execute<std::vector<u8>>().get();
                                if (result.isSuccess()) {
                                    data = result.getData();
                                }
                            } else if (path.starts_with("romfs://")) {
                                if (romfsLookup) {
                                    return romfsLookup(path.substr(7));
                                }
                            }

                            return wolv::container::Lazy<ImGuiExt::Texture>([data = std::move(data)]() -> ImGuiExt::Texture {
                                if (data.empty())
                                    return {};

                                auto texture = ImGuiExt::Texture::fromImage(data.data(), data.size(), ImGuiExt::Texture::Filter::Linear);
                                if (!texture.isValid()) {
                                    texture = ImGuiExt::Texture::fromSVG({ reinterpret_cast<const std::byte*>(data.data()), data.size() }, 0, 0, ImGuiExt::Texture::Filter::Nearest);
                                }

                                return texture;
                            });
                        }));
                    }
                    self.m_drawingImageAltText = true;
                    self.m_elementId += 1;

                    break;
                }
                default:
                    break;
            }
            return 0; // No special handling for span enter
        };
        m_mdRenderer.leave_span = [](MD_SPANTYPE type, void *detail, void *userdata) -> int {
            auto &self = *static_cast<Markdown*>(userdata);

            std::ignore = detail;
            switch (type) {
                case MD_SPAN_STRONG:
                case MD_SPAN_EM:
                    fonts::Default().pop();
                    break;
                case MD_SPAN_IMG:
                    if (!self.m_currentLink.empty()) {
                        if (ImGui::IsItemClicked())
                            hex::openWebpage(self.m_currentLink);
                        ImGui::SetItemTooltip("%s", self.m_currentLink.c_str());
                        self.m_currentLink.clear();
                    }
                    break;
                default:
                    break;
            }
            return 0; // No special handling for span leave

        };
        m_mdRenderer.text = [](MD_TEXTTYPE type, const MD_CHAR *text, MD_SIZE size, void *userdata) -> int {
            auto &self = *static_cast<Markdown*>(userdata);

            std::ignore = userdata;
            std::string_view sv(text, size);

            if (self.m_quoteStart) {
                self.m_quoteStart = false;
                if (sv.starts_with("[!") && sv.ends_with("]")) {
                    self.m_quoteNeedsChildEnd.push_back(true);
                    const auto quoteType = sv.substr(2, sv.size() - 3);
                    const char *icon = nullptr;
                    ImColor color;
                    if (quoteType == "IMPORTANT") {
                        icon = ICON_VS_REPORT;
                        color = ImGuiExt::GetCustomColorU32(ImGuiCustomCol_ToolbarRed);
                    } else if (quoteType == "NOTE") {
                        icon = ICON_VS_INFO;
                        color = ImGuiExt::GetCustomColorU32(ImGuiCustomCol_ToolbarBlue);
                    } else if (quoteType == "TIP") {
                        icon = ICON_VS_LIGHTBULB;
                        color = ImGuiExt::GetCustomColorU32(ImGuiCustomCol_ToolbarGreen);
                    } else if (quoteType == "WARNING") {
                        icon = ICON_VS_WARNING;
                        color = ImGuiExt::GetCustomColorU32(ImGuiCustomCol_ToolbarYellow);
                    } else {
                        icon = ICON_VS_QUESTION;
                        color = ImGui::GetColorU32(ImGuiCol_Separator);
                    }
                    ImGui::PushStyleColor(ImGuiCol_MenuBarBg, color.Value);
                    ImGuiExt::BeginSubWindow(icon);
                    return 0;
                } else {
                    ImGui::Indent();
                    self.m_quoteNeedsChildEnd.push_back(false);
                }
            }

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

                auto textSize = ImGui::CalcTextSize(word.data(), word.data() + word.size());
                const auto windowPadding = ImGui::GetStyle().WindowPadding.x;
                if (ImGui::GetCursorPosX() > windowPadding && ImGui::GetCursorPosX() + textSize.x > ImGui::GetWindowSize().x - windowPadding * 2 && !whiteSpaces) {
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
                        
                        self.m_drawingImageAltText = false;
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

            return 0; // Continue parsing
        };
        m_mdRenderer.debug_log = [](const char *msg, void *userdata) {
            std::ignore = userdata;
            log::debug("Markdown debug: {}", msg);
        };
    }

    void Markdown::draw() {
        if (!m_initialized)
            return;

        m_elementId = 1;
        md_parse(m_text.c_str(), m_text.size(), &m_mdRenderer, this);
    }

    void Markdown::reset() {
        m_futureImages.clear();
        m_images.clear();
    }


    bool Markdown::inTable() const {
        if (m_tableVisibleStack.empty())
            return false;
        return m_tableVisibleStack.back() != 0;
    }

    std::string Markdown::getElementId() {
        return fmt::format("##Element{}", m_elementId++);
    }

}
