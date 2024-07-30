#pragma once

#include <hex/ui/popup.hpp>

#include <hex/ui/imgui_imhex_extensions.h>

#include <hex/api/localization_manager.hpp>
#include <hex/helpers/http_requests.hpp>

#include <functional>
#include <string>

namespace hex::plugin::builtin {

    class PopupDocsQuestion : public Popup<PopupDocsQuestion> {
    public:
        PopupDocsQuestion(const std::string &input = "")
            : hex::Popup<PopupDocsQuestion>("hex.builtin.popup.docs_question.title", true, true) {

            if (!input.empty()) {
                m_inputBuffer = input;
                this->executeQuery();
            }
        }

        enum class TextBlockType {
            Text,
            Code
        };

        void drawContent() override {
            ImGui::PushItemWidth(600_scaled);
            ImGui::BeginDisabled(m_requestTask.isRunning());
            if (ImGui::InputText("##input", m_inputBuffer, ImGuiInputTextFlags_EnterReturnsTrue)) {
                this->executeQuery();
            }
            ImGui::EndDisabled();
            ImGui::PopItemWidth();

            if (ImGui::BeginChild("##answer", scaled(ImVec2(600, 350)), true, ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
                if (!m_requestTask.isRunning()) {
                    if (m_answer.empty()) {
                        if (m_noAnswer)
                            ImGuiExt::TextFormattedCentered("{}", "hex.builtin.popup.docs_question.no_answer"_lang);
                        else
                            ImGuiExt::TextFormattedCentered("{}", "hex.builtin.popup.docs_question.prompt"_lang);
                    } else {
                        int id = 1;
                        for (auto &[type, text] : m_answer) {
                            ImGui::PushID(id);
                            switch (type) {
                                case TextBlockType::Text:
                                    ImGuiExt::TextFormattedWrapped("{}", text);
                                    break;
                                case TextBlockType::Code:
                                    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyle().Colors[ImGuiCol_WindowBg]);
                                    auto textWidth = 400_scaled - ImGui::GetStyle().FramePadding.x * 4 - ImGui::GetStyle().ScrollbarSize;
                                    auto textHeight = ImGui::CalcTextSize(text.c_str(), nullptr, false, textWidth).y + ImGui::GetStyle().FramePadding.y * 6;
                                    if (ImGui::BeginChild("##code", ImVec2(textWidth, textHeight), true)) {
                                        ImGuiExt::TextFormattedWrapped("{}", text);
                                        ImGui::EndChild();
                                    }
                                    ImGui::PopStyleColor();

                                    break;
                            }

                            ImGui::PopID();
                            id += 1;
                        }
                    }
                } else {
                    ImGuiExt::TextFormattedCentered("{}", "hex.builtin.popup.docs_question.thinking"_lang);
                }
            }
            ImGui::EndChild();

            if (ImGui::IsKeyPressed(ImGuiKey_Escape))
                this->close();
        }

        [[nodiscard]] ImGuiWindowFlags getFlags() const override {
            return ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize;
        }

    private:
        void executeQuery() {
            m_requestTask = TaskManager::createBackgroundTask("hex.builtin.task.query_docs"_lang, [this, input = m_inputBuffer](Task &) {
                m_noAnswer = false;
                for (auto space : { "xj7sbzGbHH260vbpZOu1", "WZzDdGjxmgMSIE3xly6o" }) {
                    m_answer.clear();

                    auto request = HttpRequest("POST", hex::format("https://api.gitbook.com/v1/spaces/{}/search/ask", space));

                    // Documentation API often takes a long time to respond, so we set a timeout of 30 seconds
                    request.setTimeout(30'000);

                    const nlohmann::json body = { { "query", input } };
                    request.setBody(body.dump());
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
                                m_answer.emplace_back(TextBlockType::Code, block);
                            } else if (block.starts_with("cpp\n")) {
                                block = block.substr(4);
                                m_answer.emplace_back(TextBlockType::Code, block);
                            } else {
                                m_answer.emplace_back(TextBlockType::Text, block);
                            }
                        }
                    } catch(...) {
                        continue;
                    }
                }

                m_noAnswer = m_answer.empty();
            });
        }

    private:
        std::string m_inputBuffer;
        std::vector<std::pair<TextBlockType, std::string>> m_answer;
        TaskHolder m_requestTask;
        bool m_noAnswer = false;
    };

}