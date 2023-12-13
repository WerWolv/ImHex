#include <hex/api/tutorial_manager.hpp>
#include <hex/api/imhex_api.hpp>
#include <hex/api/localization_manager.hpp>

#include <imgui_internal.h>
#include <hex/helpers/utils.hpp>
#include <wolv/utils/core.hpp>

#include <map>

namespace hex {

    namespace {

        std::map<std::string, TutorialManager::Tutorial> s_tutorials;
        decltype(s_tutorials)::iterator s_currentTutorial = s_tutorials.end();

        std::map<ImGuiID, std::string> s_highlights;
        std::vector<std::pair<ImRect, std::string>> s_highlightDisplays;


        class IDStack {
        public:
            IDStack() {
                idStack.push_back(0);
            }

            void add(const std::string &string) {
                const ImGuiID seed = idStack.back();
                const ImGuiID id = ImHashStr(string.c_str(), string.length(), seed);

                idStack.push_back(id);
            }

            void add(const void *pointer) {
                const ImGuiID seed = idStack.back();
                const ImGuiID id = ImHashData(&pointer, sizeof(pointer), seed);

                idStack.push_back(id);
            }

            void add(int value) {
                const ImGuiID seed = idStack.back();
                const ImGuiID id = ImHashData(&value, sizeof(value), seed);

                idStack.push_back(id);
            }

            ImGuiID get() {
                return idStack.back();
            }
        private:
            ImVector<ImGuiID> idStack;
        };

    }


    const std::map<std::string, TutorialManager::Tutorial>& TutorialManager::getTutorials() {
        return s_tutorials;
    }

    std::map<std::string, TutorialManager::Tutorial>::iterator TutorialManager::getCurrentTutorial() {
        return s_currentTutorial;
    }


    TutorialManager::Tutorial& TutorialManager::createTutorial(const std::string& unlocalizedName, const std::string& unlocalizedDescription) {
        return s_tutorials.try_emplace(unlocalizedName, Tutorial(unlocalizedName, unlocalizedDescription)).first->second;
    }

    void TutorialManager::startTutorial(const std::string& unlocalizedName) {
        s_currentTutorial = s_tutorials.find(unlocalizedName);
        if (s_currentTutorial == s_tutorials.end())
            return;

        s_currentTutorial->second.start();
    }

    void TutorialManager::drawHighlights() {
        for (const auto &[rect, unlocalizedText] : s_highlightDisplays) {
            const auto drawList = ImGui::GetForegroundDrawList();

            drawList->PushClipRectFullScreen();
            {
                auto highlightColor = ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_Highlight);
                highlightColor.w *= ImSin(ImGui::GetTime() * 6.0F) / 4.0F + 0.75F;

                drawList->AddRect(rect.Min - ImVec2(5, 5), rect.Max + ImVec2(5, 5), ImColor(highlightColor), 5.0F, ImDrawFlags_None, 2.0F);
            }

            {
                if (!unlocalizedText.empty()) {
                    const auto margin   = ImGui::GetStyle().WindowPadding;

                    const ImVec2 windowPos  = { rect.Min.x + 20_scaled, rect.Max.y + 10_scaled };
                    ImVec2 windowSize = { std::max<float>(rect.Max.x - rect.Min.x - 40_scaled, 300_scaled), 0 };

                    const char* text = Lang(unlocalizedText);
                    const auto textSize = ImGui::CalcTextSize(text, nullptr, false, windowSize.x - margin.x * 2);
                    windowSize.y = textSize.y + margin.y * 2;

                    drawList->AddRectFilled(windowPos, windowPos + windowSize, ImGui::GetColorU32(ImGuiCol_WindowBg) | 0xFF000000);
                    drawList->AddRect(windowPos, windowPos + windowSize, ImGui::GetColorU32(ImGuiCol_Border));
                    drawList->AddText(nullptr, 0.0F, windowPos + margin, ImGui::GetColorU32(ImGuiCol_Text), text, nullptr, windowSize.x - margin.x * 2);
                }
            }
            drawList->PopClipRect();

        }
        s_highlightDisplays.clear();
    }

    void TutorialManager::drawMessageBox(std::optional<Tutorial::Step::Message> message) {
        const auto windowStart = ImHexApi::System::getMainWindowPosition() + scaled({ 10, 10 });
        const auto windowEnd = ImHexApi::System::getMainWindowPosition() + ImHexApi::System::getMainWindowSize() - scaled({ 10, 10 });

        ImVec2 position = ImHexApi::System::getMainWindowPosition() + ImHexApi::System::getMainWindowSize() / 2.0F;
        ImVec2 pivot    = { 0.5F, 0.5F };

        if (!message.has_value()) {
            message = Tutorial::Step::Message {
                 Position::None,
                "",
                "",
                false
            };
        }

        if (message->position == Position::None) {
            message->position = Position::Bottom | Position::Right;
        }

        if ((message->position & Position::Top) == Position::Top) {
            position.y  = windowStart.y;
            pivot.y     = 0.0F;
        }
        if ((message->position & Position::Bottom) == Position::Bottom) {
            position.y  = windowEnd.y;
            pivot.y     = 1.0F;
        }
        if ((message->position & Position::Left) == Position::Left) {
            position.x  = windowStart.x;
            pivot.x     = 0.0F;
        }
        if ((message->position & Position::Right) == Position::Right) {
            position.x  = windowEnd.x;
            pivot.x     = 1.0F;
        }

        ImGui::SetNextWindowPos(position, ImGuiCond_Always, pivot);
        if (ImGui::Begin("##TutorialMessage", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoFocusOnAppearing)) {
            ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());

            if (!message->unlocalizedTitle.empty())
                ImGuiExt::Header(Lang(message->unlocalizedTitle), true);

            if (!message->unlocalizedMessage.empty()) {
                ImGui::PushTextWrapPos(300_scaled);
                ImGui::TextUnformatted(Lang(message->unlocalizedMessage));
                ImGui::PopTextWrapPos();
                ImGui::NewLine();
            }

            ImGui::BeginDisabled(s_currentTutorial->second.m_currentStep == s_currentTutorial->second.m_steps.begin());
            if (ImGui::ArrowButton("Backwards", ImGuiDir_Left)) {
                s_currentTutorial->second.m_currentStep->advance(-1);
            }
            ImGui::EndDisabled();

            ImGui::SameLine();

            ImGui::BeginDisabled(s_currentTutorial->second.m_currentStep == s_currentTutorial->second.m_steps.end() || (!message->allowSkip && s_currentTutorial->second.m_currentStep == s_currentTutorial->second.m_latestStep));
            if (ImGui::ArrowButton("Forwards", ImGuiDir_Right)) {
                s_currentTutorial->second.m_currentStep->advance(1);
            }
            ImGui::EndDisabled();
        }
        ImGui::End();
    }

    void TutorialManager::drawTutorial() {
        drawHighlights();

        if (s_currentTutorial == s_tutorials.end())
            return;

        const auto &currentStep = s_currentTutorial->second.m_currentStep;
        if (currentStep == s_currentTutorial->second.m_steps.end())
            return;

        const auto &message = currentStep->m_message;
        drawMessageBox(message);
    }



    void TutorialManager::reset() {
        s_tutorials.clear();
        s_currentTutorial = s_tutorials.end();

        s_highlights.clear();
        s_highlightDisplays.clear();
    }

    TutorialManager::Tutorial::Step& TutorialManager::Tutorial::addStep() {
        auto &newStep = this->m_steps.emplace_back(this);
        this->m_currentStep = this->m_steps.end();
        this->m_latestStep  = this->m_currentStep;

        return newStep;
    }

    void TutorialManager::Tutorial::start() {
        this->m_currentStep = m_steps.begin();
        this->m_latestStep  = this->m_currentStep;
        if (m_currentStep == m_steps.end())
            return;

        m_currentStep->addHighlights();
    }

    void TutorialManager::Tutorial::Step::addHighlights() const {
        for (const auto &[text, id] : this->m_highlights) {
            s_highlights.emplace(id, text.c_str());
        }
    }

    void TutorialManager::Tutorial::Step::removeHighlights() const {
        for (const auto &[text, id] : this->m_highlights) {
            s_highlights.erase(id);
        }
    }

    void TutorialManager::Tutorial::Step::advance(i32 steps) const {
        m_parent->m_currentStep->removeHighlights();
        std::advance(m_parent->m_currentStep, steps);

        if (m_parent->m_currentStep != m_parent->m_steps.end())
            m_parent->m_currentStep->addHighlights();
        else
            s_currentTutorial = s_tutorials.end();
    }


    TutorialManager::Tutorial::Step& TutorialManager::Tutorial::Step::addHighlight(const std::string& unlocalizedText, std::initializer_list<std::variant<Lang, std::string, int>>&& ids) {
        IDStack idStack;

        for (const auto &id : ids) {
            std::visit(wolv::util::overloaded {
                [&idStack](const Lang &id) {
                    idStack.add(id.get());
                },
                [&idStack](const auto &id) {
                    idStack.add(id);
                }
            }, id);
        }

        this->m_highlights.emplace_back(
            unlocalizedText,
            idStack.get()
        );

        return *this;
    }

    TutorialManager::Tutorial::Step& TutorialManager::Tutorial::Step::addHighlight(std::initializer_list<std::variant<Lang, std::string, int>>&& ids) {
        return this->addHighlight("", std::move(ids));
    }



    TutorialManager::Tutorial::Step& TutorialManager::Tutorial::Step::setMessage(const std::string& unlocalizedTitle, const std::string& unlocalizedMessage, Position position) {
        this->m_message = Message {
            position,
            unlocalizedTitle,
            unlocalizedMessage,
            false
        };

        return *this;
    }

    TutorialManager::Tutorial::Step& TutorialManager::Tutorial::Step::allowSkip() {
        if (this->m_message.has_value()) {
            this->m_message->allowSkip = true;
        } else {
            this->m_message = Message {
                Position::Bottom | Position::Right,
                "",
                "",
                true
            };
        }

        return *this;
    }



    bool TutorialManager::Tutorial::Step::isCurrent() const {
        const auto &currentStep = this->m_parent->m_currentStep;

        if (currentStep == this->m_parent->m_steps.end())
            return false;

        return &*currentStep == this;
    }

    void TutorialManager::Tutorial::Step::complete() const {
        if (this->isCurrent()) {
            this->advance();
            this->m_parent->m_latestStep = this->m_parent->m_currentStep;
        }
    }

}

void ImGuiTestEngineHook_ItemAdd(ImGuiContext*, ImGuiID id, const ImRect& bb, const ImGuiLastItemData*) {
    const auto element = hex::s_highlights.find(id);
    if (element != hex::s_highlights.end()) {
        hex::s_highlightDisplays.emplace_back(bb, element->second);
    }
}

void ImGuiTestEngineHook_ItemInfo(ImGuiContext*, ImGuiID, const char*, ImGuiItemStatusFlags) {}
void ImGuiTestEngineHook_Log(ImGuiContext*, const char*, ...) {}
const char* ImGuiTestEngine_FindItemDebugLabel(ImGuiContext*, ImGuiID) { return nullptr; }