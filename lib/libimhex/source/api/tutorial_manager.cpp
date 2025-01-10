#include <hex/api/tutorial_manager.hpp>
#include <hex/api/imhex_api.hpp>
#include <hex/api/localization_manager.hpp>
#include <hex/api/task_manager.hpp>

#include <hex/helpers/auto_reset.hpp>

#include <imgui_internal.h>
#include <hex/helpers/utils.hpp>
#include <wolv/utils/core.hpp>

#include <map>

#include <imgui.h>

namespace hex {

    namespace {

        AutoReset<std::map<std::string, TutorialManager::Tutorial>> s_tutorials;
        auto s_currentTutorial = s_tutorials->end();

        AutoReset<std::map<ImGuiID, std::string>> s_highlights;
        AutoReset<std::vector<std::pair<ImRect, std::string>>> s_highlightDisplays;
        AutoReset<std::map<ImGuiID, ImRect>> s_interactiveHelpDisplays;

        AutoReset<std::map<ImGuiID, std::function<void()>>> s_interactiveHelpItems;
        ImRect s_hoveredRect;
        ImGuiID s_hoveredId;
        ImGuiID s_activeHelpId;
        bool s_helpHoverActive = false;


        class IDStack {
        public:
            IDStack() {
                idStack.push_back(0);
            }

            void add(const char *string) {
                const ImGuiID seed = idStack.back();
                const ImGuiID id = ImHashStr(string, 0, seed);

                idStack.push_back(id);
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

        ImGuiID calculateId(const auto &ids) {
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

            return idStack.get();
        }

    }

    void TutorialManager::init() {
        EventImGuiElementRendered::subscribe([](ImGuiID id, const std::array<float, 4> bb){
            const auto boundingBox = ImRect(bb[0], bb[1], bb[2], bb[3]);

            {
                const auto element = hex::s_highlights->find(id);
                if (element != hex::s_highlights->end()) {
                    hex::s_highlightDisplays->emplace_back(boundingBox, element->second);

                    const auto window = ImGui::GetCurrentWindow();
                    if (window != nullptr && window->DockNode != nullptr && window->DockNode->TabBar != nullptr)
                        window->DockNode->TabBar->NextSelectedTabId = window->TabId;
                }
            }

            {
                const auto element = s_interactiveHelpItems->find(id);
                if (element != s_interactiveHelpItems->end()) {
                    (*s_interactiveHelpDisplays)[id] = boundingBox;
                }

            }

            if (id != 0 && boundingBox.Contains(ImGui::GetMousePos())) {
                if ((s_hoveredRect.GetArea() == 0 || boundingBox.GetArea() < s_hoveredRect.GetArea()) && s_interactiveHelpItems->contains(id)) {
                    s_hoveredRect = boundingBox;
                    s_hoveredId = id;
                }
            }
        });
    }

    const std::map<std::string, TutorialManager::Tutorial>& TutorialManager::getTutorials() {
        return s_tutorials;
    }

    std::map<std::string, TutorialManager::Tutorial>::iterator TutorialManager::getCurrentTutorial() {
        return s_currentTutorial;
    }


    TutorialManager::Tutorial& TutorialManager::createTutorial(const UnlocalizedString &unlocalizedName, const UnlocalizedString &unlocalizedDescription) {
        return s_tutorials->try_emplace(unlocalizedName, Tutorial(unlocalizedName, unlocalizedDescription)).first->second;
    }

    void TutorialManager::startHelpHover() {
        TaskManager::doLater([]{
            s_helpHoverActive = true;
        });
    }

    void TutorialManager::addInteractiveHelpText(std::initializer_list<std::variant<Lang, std::string, int>> &&ids, UnlocalizedString unlocalizedString) {
        auto id = calculateId(ids);

        s_interactiveHelpItems->emplace(id, [text = std::move(unlocalizedString)]{
            log::info("{}", Lang(text).get());
        });
    }

    void TutorialManager::addInteractiveHelpLink(std::initializer_list<std::variant<Lang, std::string, int>> &&ids, std::string link) {
        auto id = calculateId(ids);

        s_interactiveHelpItems->emplace(id, [link = std::move(link)]{
            hex::openWebpage(link);
        });
    }

    void TutorialManager::setLastItemInteractiveHelpPopup(std::function<void()> callback) {
        auto id = ImGui::GetItemID();

        if (!s_interactiveHelpItems->contains(id)) {
            s_interactiveHelpItems->emplace(id, [id]{
                s_activeHelpId = id;
            });
        }

        if (id == s_activeHelpId) {
            ImGui::SetNextWindowSize(scaled({ 400, 0 }));
            if (ImGui::BeginTooltip()) {
                callback();
                ImGui::EndTooltip();
            }

            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsKeyPressed(ImGuiKey_Escape))
                s_activeHelpId = 0;
        }
    }

    void TutorialManager::setLastItemInteractiveHelpLink(std::string link) {
        auto id = ImGui::GetItemID();

        if (s_interactiveHelpItems->contains(id))
            return;

        s_interactiveHelpItems->emplace(id, [link = std::move(link)]{
            hex::openWebpage(link);
        });
    }


    void TutorialManager::startTutorial(const UnlocalizedString &unlocalizedName) {
        s_currentTutorial = s_tutorials->find(unlocalizedName);
        if (s_currentTutorial == s_tutorials->end())
            return;

        s_currentTutorial->second.start();
    }

    void TutorialManager::drawHighlights() {
        if (s_helpHoverActive) {
            const auto &drawList = ImGui::GetForegroundDrawList();
            drawList->AddText(ImGui::GetMousePos() + scaled({ 10, -5, }), ImGui::GetColorU32(ImGuiCol_Text), "?");

            for (const auto &[id, boundingBox] : *s_interactiveHelpDisplays) {
                drawList->AddRect(
                    boundingBox.Min - ImVec2(5, 5),
                    boundingBox.Max + ImVec2(5, 5),
                    ImGui::GetColorU32(ImGuiCol_PlotHistogram),
                    5.0F,
                    ImDrawFlags_None,
                    2.0F
                );
            }

            s_interactiveHelpDisplays->clear();

            const bool mouseClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
            if (s_hoveredId != 0) {
                drawList->AddRectFilled(s_hoveredRect.Min, s_hoveredRect.Max, 0x30FFFFFF);

                if (mouseClicked) {
                    auto it = s_interactiveHelpItems->find(s_hoveredId);
                    if (it != s_interactiveHelpItems->end()) {
                        it->second();
                    }
                }

                s_hoveredId = 0;
                s_hoveredRect = {};
            }

            if (mouseClicked || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                s_helpHoverActive = false;
            }

            // Discard mouse click so it doesn't activate clicked item
            ImGui::GetIO().MouseDown[ImGuiMouseButton_Left]     = false;
            ImGui::GetIO().MouseReleased[ImGuiMouseButton_Left] = false;
            ImGui::GetIO().MouseClicked[ImGuiMouseButton_Left]  = false;
        }

        for (const auto &[rect, unlocalizedText] : *s_highlightDisplays) {
            const auto drawList = ImGui::GetForegroundDrawList();

            drawList->PushClipRectFullScreen();
            {
                auto highlightColor = ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_Highlight);
                highlightColor.w *= ImSin(ImGui::GetTime() * 6.0F) / 4.0F + 0.75F;

                drawList->AddRect(rect.Min - ImVec2(5, 5), rect.Max + ImVec2(5, 5), ImColor(highlightColor), 5.0F, ImDrawFlags_None, 2.0F);
            }

            {
                if (!unlocalizedText.empty()) {
                    const auto mainWindowPos = ImHexApi::System::getMainWindowPosition();
                    const auto mainWindowSize = ImHexApi::System::getMainWindowSize();

                    const auto margin = ImGui::GetStyle().WindowPadding;

                    ImVec2 windowPos  = { rect.Min.x + 20_scaled, rect.Max.y + 10_scaled };
                    ImVec2 windowSize = { std::max<float>(rect.Max.x - rect.Min.x - 40_scaled, 300_scaled), 0 };

                    const char* text = Lang(unlocalizedText);
                    const auto textSize = ImGui::CalcTextSize(text, nullptr, false, windowSize.x - margin.x * 2);
                    windowSize.y = textSize.y + margin.y * 2;

                    if (windowPos.y + windowSize.y > mainWindowPos.y + mainWindowSize.y)
                        windowPos.y = rect.Min.y - windowSize.y - 15_scaled;
                    if (windowPos.y < mainWindowPos.y)
                        windowPos.y = rect.Min.y + 10_scaled;

                    ImGui::SetNextWindowPos(windowPos);
                    ImGui::SetNextWindowSize(windowSize);
                    ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
                    if (ImGui::Begin(unlocalizedText.c_str(), nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize)) {
                        ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindowRead());
                        ImGuiExt::TextFormattedWrapped("{}", text);
                    }
                    ImGui::End();
                }
            }
            drawList->PopClipRect();

        }
        s_highlightDisplays->clear();
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
            ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindowRead());

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

            ImGui::BeginDisabled(!message->allowSkip && s_currentTutorial->second.m_currentStep == s_currentTutorial->second.m_latestStep);
            if (ImGui::ArrowButton("Forwards", ImGuiDir_Right)) {
                s_currentTutorial->second.m_currentStep->advance(1);
            }
            ImGui::EndDisabled();
        }
        ImGui::End();
    }

    void TutorialManager::drawTutorial() {
        drawHighlights();

        if (s_currentTutorial == s_tutorials->end())
            return;

        const auto &currentStep = s_currentTutorial->second.m_currentStep;
        if (currentStep == s_currentTutorial->second.m_steps.end())
            return;

        const auto &message = currentStep->m_message;
        drawMessageBox(message);
    }



    void TutorialManager::reset() {
        s_tutorials->clear();
        s_currentTutorial = s_tutorials->end();

        s_highlights->clear();
        s_highlightDisplays->clear();
    }

    TutorialManager::Tutorial::Step& TutorialManager::Tutorial::addStep() {
        auto &newStep = m_steps.emplace_back(this);
        m_currentStep = m_steps.end();
        m_latestStep  = m_currentStep;

        return newStep;
    }

    void TutorialManager::Tutorial::start() {
        m_currentStep = m_steps.begin();
        m_latestStep  = m_currentStep;
        if (m_currentStep == m_steps.end())
            return;

        m_currentStep->addHighlights();
    }

    void TutorialManager::Tutorial::Step::addHighlights() const {
        if (m_onAppear)
            m_onAppear();

        for (const auto &[text, ids] : m_highlights) {
            s_highlights->emplace(calculateId(ids), text);
        }
    }

    void TutorialManager::Tutorial::Step::removeHighlights() const {
        for (const auto &[text, ids] : m_highlights) {
            s_highlights->erase(calculateId(ids));
        }
    }

    void TutorialManager::Tutorial::Step::advance(i32 steps) const {
        m_parent->m_currentStep->removeHighlights();

        if (m_parent->m_currentStep == m_parent->m_latestStep && steps > 0)
            std::advance(m_parent->m_latestStep, steps);
        std::advance(m_parent->m_currentStep, steps);

        if (m_parent->m_currentStep != m_parent->m_steps.end())
            m_parent->m_currentStep->addHighlights();
        else
            s_currentTutorial = s_tutorials->end();
    }


    TutorialManager::Tutorial::Step& TutorialManager::Tutorial::Step::addHighlight(const UnlocalizedString &unlocalizedText, std::initializer_list<std::variant<Lang, std::string, int>>&& ids) {
        m_highlights.emplace_back(
            unlocalizedText,
            ids
        );

        return *this;
    }

    TutorialManager::Tutorial::Step& TutorialManager::Tutorial::Step::addHighlight(std::initializer_list<std::variant<Lang, std::string, int>>&& ids) {
        return this->addHighlight("", std::forward<decltype(ids)>(ids));
    }



    TutorialManager::Tutorial::Step& TutorialManager::Tutorial::Step::setMessage(const UnlocalizedString &unlocalizedTitle, const UnlocalizedString &unlocalizedMessage, Position position) {
        m_message = Message {
            position,
            unlocalizedTitle,
            unlocalizedMessage,
            false
        };

        return *this;
    }

    TutorialManager::Tutorial::Step& TutorialManager::Tutorial::Step::allowSkip() {
        if (m_message.has_value()) {
            m_message->allowSkip = true;
        } else {
            m_message = Message {
                Position::Bottom | Position::Right,
                "",
                "",
                true
            };
        }

        return *this;
    }

    TutorialManager::Tutorial::Step& TutorialManager::Tutorial::Step::onAppear(std::function<void()> callback) {
        m_onAppear = std::move(callback);

        return *this;
    }

    TutorialManager::Tutorial::Step& TutorialManager::Tutorial::Step::onComplete(std::function<void()> callback) {
        m_onComplete = std::move(callback);

        return *this;
    }




    bool TutorialManager::Tutorial::Step::isCurrent() const {
        const auto &currentStep = m_parent->m_currentStep;

        if (currentStep == m_parent->m_steps.end())
            return false;

        return &*currentStep == this;
    }

    void TutorialManager::Tutorial::Step::complete() const {
        if (this->isCurrent()) {
            this->advance();

            if (m_onComplete) {
                TaskManager::doLater([this] {
                    m_onComplete();
                });
            }
        }
    }

}