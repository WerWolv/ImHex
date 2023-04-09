#include <hex/ui/popup.hpp>

#include <hex/ui/imgui_imhex_extensions.h>

#include <hex/api/localization.hpp>
#include <hex/helpers/http_requests.hpp>

#include <functional>
#include <string>

namespace hex::plugin::builtin {

    class PopupDocsQuestion : public Popup<PopupDocsQuestion> {
    public:
        PopupDocsQuestion()
            : hex::Popup<PopupDocsQuestion>("hex.builtin.popup.docs_question.title", true, true) { }

        enum class TextBlockType {
            Text,
            Code
        };

        void drawContent() override {
            ImGui::PushItemWidth(600_scaled);
            ImGui::BeginDisabled(this->m_requestTask.isRunning());
            if (ImGui::InputText("##input", this->m_inputBuffer, ImGuiInputTextFlags_EnterReturnsTrue)) {
                this->executeQuery();
            }
            ImGui::EndDisabled();
            ImGui::PopItemWidth();

            if (ImGui::BeginChild("##answer", scaled(ImVec2(600, 350)), true, ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
                if (!this->m_requestTask.isRunning()) {
                    if (this->m_answer.empty()) {
                        if (this->m_noAnswer)
                            ImGui::TextFormattedCentered("{}", "hex.builtin.popup.docs_question.no_answer"_lang);
                        else
                            ImGui::TextFormattedCentered("{}", "hex.builtin.popup.docs_question.prompt"_lang);
                    } else {
                        for (auto &[type, text] : this->m_answer) {
                            switch (type) {
                                case TextBlockType::Text:
                                    ImGui::TextFormattedWrapped("{}", text);
                                    break;
                                case TextBlockType::Code:
                                    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyle().Colors[ImGuiCol_WindowBg]);
                                    auto textWidth = 400_scaled - ImGui::GetStyle().FramePadding.x * 4 - ImGui::GetStyle().ScrollbarSize;
                                    auto textHeight = ImGui::CalcTextSize(text.c_str(), nullptr, false, textWidth).y + ImGui::GetStyle().FramePadding.y * 6;
                                    if (ImGui::BeginChild("##code", ImVec2(textWidth, textHeight), true)) {
                                        ImGui::TextFormattedWrapped("{}", text);
                                        ImGui::EndChild();
                                    }
                                    ImGui::PopStyleColor();

                                    break;
                            }
                        }
                    }
                } else {
                    ImGui::TextFormattedCentered("{}", "hex.builtin.popup.docs_question.thinking"_lang);
                }
            }
            ImGui::EndChild();
        }

        [[nodiscard]] ImGuiWindowFlags getFlags() const override {
            return ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize;
        }

    private:
        void executeQuery() {
            this->m_requestTask = TaskManager::createBackgroundTask("Query Docs", [this, input = this->m_inputBuffer](Task &) {
                this->m_noAnswer = false;
                for (auto space : { "xj7sbzGbHH260vbpZOu1", "WZzDdGjxmgMSIE3xly6o" }) {
                    this->m_answer.clear();

                    auto request = HttpRequest("POST", hex::format("https://api.gitbook.com/v1/spaces/{}/search/ask", space));

                    request.setBody(hex::format(R"({{ "query": "{}" }})", input));
                    request.addHeader("Content-Type", "application/json");

                    auto response = request.execute().get();
                    if (!response.isSuccess())
                        continue;

                    try {
                        auto json = nlohmann::json::parse(response.getData());
                        if (!json.contains("answer"))
                            continue;

                        auto answer = json["answer"]["text"].get<std::string>();
                        if (answer.empty())
                            break;

                        auto blocks = wolv::util::splitString(answer, "```");
                        for (auto &block : blocks) {
                            if (block.empty())
                                continue;

                            block = wolv::util::trim(block);

                            if (block.starts_with("rust\n")) {
                                block = block.substr(5);
                                this->m_answer.emplace_back(TextBlockType::Code, block);
                            } else if (block.starts_with("cpp\n")) {
                                block = block.substr(4);
                                this->m_answer.emplace_back(TextBlockType::Code, block);
                            } else {
                                this->m_answer.emplace_back(TextBlockType::Text, block);
                            }
                        }
                    } catch(...) {
                        continue;
                    }

                    this->m_noAnswer = this->m_answer.empty();
                }
            });
        }

    private:
        std::string m_inputBuffer;
        std::vector<std::pair<TextBlockType, std::string>> m_answer;
        TaskHolder m_requestTask;
        bool m_noAnswer = false;
    };

}