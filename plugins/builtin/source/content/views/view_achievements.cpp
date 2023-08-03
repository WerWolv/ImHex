#include "content/views/view_achievements.hpp"

#include <hex/api/achievement_manager.hpp>
#include <hex/api/content_registry.hpp>

#include <wolv/utils/guards.hpp>

namespace hex::plugin::builtin {

    ViewAchievements::ViewAchievements() : View("hex.builtin.view.achievements.name") {
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.extras", "hex.builtin.view.achievements.name" }, 2600, Shortcut::None, [&, this] {
            this->m_viewOpen = true;
            this->getWindowOpenState() = true;
        });

        EventManager::subscribe<EventAchievementUnlocked>(this, [this](const Achievement &achievement) {
            this->m_achievementUnlockQueue.push_back(&achievement);
        });
    }

    ViewAchievements::~ViewAchievements() {
        EventManager::unsubscribe<EventAchievementUnlocked>(this);
    }

    void drawAchievement(ImDrawList *drawList, const AchievementManager::AchievementNode *node, ImVec2 position) {
        const auto achievementSize = scaled({ 50, 50 });

        const auto borderColor = [&] {
            if (node->achievement->isUnlocked())
                return ImGui::GetColorU32(ImGuiCol_PlotHistogram) | 0xFF000000;
            else if (node->isUnlockable())
                return ImGui::GetColorU32(ImGuiCol_Button) | 0xFF000000;
            else
                return ImGui::GetColorU32(ImGuiCol_Text) | 0xFF000000;
        }();

        const auto fillColor = [&] {
            if (node->achievement->isUnlocked())
                return ImGui::GetColorU32(ImGuiCol_Text) | 0xFF000000;
            else if (node->isUnlockable())
                return u32(ImColor(ImLerp(ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled), ImGui::GetStyleColorVec4(ImGuiCol_Text), std::sin(ImGui::GetTime() * 6) * 0.5f + 0.5f)));
            else
                return ImGui::GetColorU32(ImGuiCol_TextDisabled) | 0xFF000000;
        }();

        drawList->AddRectFilled(position, position + achievementSize, fillColor, 5_scaled, 0);
        drawList->AddRect(position, position + achievementSize, borderColor, 5_scaled, 0, 2_scaled);

        if (ImGui::IsMouseHoveringRect(position, position + achievementSize)) {
            ImGui::SetNextWindowPos(position + ImVec2(achievementSize.x, 0));
            ImGui::SetNextWindowSize(achievementSize * ImVec2(4, 0));
            ImGui::BeginTooltip();
            {
                if (node->achievement->isBlacked() && !node->achievement->isUnlocked()) {
                    ImGui::TextUnformatted("???");
                } else {
                    ImGui::BeginDisabled(!node->achievement->isUnlocked());
                    ImGui::TextUnformatted(LangEntry(node->achievement->getUnlocalizedName()));

                    if (const auto &desc = node->achievement->getUnlocalizedDescription(); !desc.empty()) {
                        ImGui::Separator();
                        ImGui::TextWrapped("%s", static_cast<const char *>(LangEntry(desc)));
                    }
                    ImGui::EndDisabled();
                }

            }
            ImGui::EndTooltip();

            #if defined (DEBUG)
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    AchievementManager::unlockAchievement(node->achievement->getUnlocalizedCategory(), node->achievement->getUnlocalizedName());
                }
            #endif
        }
    }

    void drawBackground(ImDrawList *drawList, ImVec2 min, ImVec2 max, ImVec2 offset) {
        const auto patternSize = scaled({ 10, 10 });

        const auto darkColor  = ImGui::GetColorU32(ImGuiCol_TableRowBg);
        const auto lightColor = ImGui::GetColorU32(ImGuiCol_TableRowBgAlt);

        drawList->AddRect(min, max, ImGui::GetColorU32(ImGuiCol_Border), 0.0F, 0, 1.0_scaled);

        bool light = false;
        bool prevStart = false;
        for (float x = min.x + offset.x; x < max.x; x += patternSize.x) {
            if (prevStart == light)
                light = !light;
            prevStart = light;

            for (float y = min.y + offset.y; y < max.y; y += patternSize.y) {
                drawList->AddRectFilled({ x, y }, { x + patternSize.x, y + patternSize.y }, light ? lightColor : darkColor);
                light = !light;
            }
        }
    }

    ImVec2 drawAchievementTree(ImDrawList *drawList, const AchievementManager::AchievementNode * prevNode, const std::vector<AchievementManager::AchievementNode*> &nodes, ImVec2 position) {
        ImVec2 maxPos = position;
        for (auto &node : nodes) {
            if (node->achievement->isInvisible() && !node->achievement->isUnlocked())
                continue;

            if (!node->visibilityParents.empty()) {
                bool visible = true;
                for (auto &parent : node->visibilityParents) {
                    if (!parent->achievement->isUnlocked()) {
                        visible = false;
                        break;
                    }
                }

                if (!visible)
                    continue;
            }

            if (prevNode != nullptr) {
                if (prevNode->achievement->getUnlocalizedCategory() != node->achievement->getUnlocalizedCategory())
                    continue;

                auto start = prevNode->position + scaled({ 50, 25 });
                auto end = node->position + scaled({ 0, 25 });
                auto middle1 = ((start + end) / 2.0F) + scaled({ 50, 0 });
                auto middle2 = ((start + end) / 2.0F) - scaled({ 50, 0 });

                const auto color = [prevNode]{
                    if (prevNode->achievement->isUnlocked())
                        return ImGui::GetColorU32(ImGuiCol_Text) | 0xFF000000;
                    else
                        return ImGui::GetColorU32(ImGuiCol_TextDisabled) | 0xFF000000;
                }();

                drawList->AddBezierCubic(start, middle1, middle2, end, color, 2_scaled);
            }

            drawAchievement(drawList, node, position);

            node->position = position;
            auto newMaxPos = drawAchievementTree(drawList, node, node->children, position + scaled({ 150, 0 }));
            if (newMaxPos.x > maxPos.x)
                maxPos.x = newMaxPos.x;
            if (newMaxPos.y > maxPos.y)
                maxPos.y = newMaxPos.y;

            position.y = maxPos.y + 100_scaled;
        }

        return maxPos;
    }

    void ViewAchievements::drawContent() {
        if (ImGui::Begin(View::toWindowName("hex.builtin.view.achievements.name").c_str(), &this->m_viewOpen, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoDocking)) {
            for (const auto &[categoryName, achievements] : AchievementManager::getAchievementStartNodes()) {
                if (ImGui::BeginTabBar("##achievement_categories")) {
                    ON_SCOPE_EXIT {
                        ImGui::EndTabBar();
                    };

                    bool visible = false;
                    for (const auto &achievement : achievements) {
                        if (achievement->isUnlocked() || achievement->isUnlockable()) {
                            visible = true;
                            break;
                        }
                    }

                    if (!visible)
                        continue;

                    if (ImGui::BeginTabItem(LangEntry(categoryName))) {
                        auto drawList = ImGui::GetWindowDrawList();

                        const auto cursorPos = ImGui::GetCursorPos();
                        const auto windowPos = ImGui::GetWindowPos() + ImVec2(0, cursorPos.y);
                        const auto windowSize = ImGui::GetWindowSize() - ImVec2(0, cursorPos.y);;
                        const float borderSize = 50.0_scaled;

                        const auto innerWindowPos = windowPos + ImVec2(borderSize, borderSize);
                        const auto innerWindowSize = windowSize - ImVec2(borderSize * 2, borderSize * 2);
                        drawList->PushClipRect(innerWindowPos, innerWindowPos + innerWindowSize, true);
                        drawBackground(drawList, innerWindowPos, innerWindowPos + innerWindowSize, this->m_offset);

                        auto maxPos = drawAchievementTree(drawList, nullptr, achievements, innerWindowPos + scaled({ 100, 100 }) + this->m_offset);


                        if (ImGui::IsMouseHoveringRect(innerWindowPos, innerWindowPos + innerWindowSize)) {
                            auto dragDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
                            this->m_offset += dragDelta;
                            ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
                        }

                        if (this->m_offset.x > 0) this->m_offset.x = 0;
                        else if (this->m_offset.x < (-maxPos.x + innerWindowPos.x)) this->m_offset.x = -maxPos.x + innerWindowPos.x;

                        if (this->m_offset.y > 0) this->m_offset.y = 0;
                        else if (this->m_offset.y < (-maxPos.y + innerWindowPos.y)) this->m_offset.y = -maxPos.y + innerWindowPos.y;

                        drawList->PopClipRect();

                        ImGui::EndTabItem();
                    }
                }
            }
        }
        ImGui::End();

        this->getWindowOpenState() = this->m_viewOpen;
    }

    void ViewAchievements::drawAlwaysVisible() {
        if (this->m_achievementUnlockQueueTimer >= 0) {
            this->m_achievementUnlockQueueTimer -= ImGui::GetIO().DeltaTime;

            if (this->m_currAchievement != nullptr) {

                const ImVec2 windowSize = scaled({ 200, 50 });
                ImGui::SetNextWindowPos(ImHexApi::System::getMainWindowPosition() + ImVec2 { ImHexApi::System::getMainWindowSize().x - windowSize.x - 20_scaled, 20_scaled });
                ImGui::SetNextWindowSize(windowSize);
                if (ImGui::Begin("##achievement_unlocked", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar)) {
                    ImGui::TextColored(ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarYellow), "hex.builtin.view.achievements.unlocked"_lang);
                    ImGui::TextUnformatted(LangEntry(this->m_currAchievement->getUnlocalizedName()));
                }
                ImGui::End();
            }
        } else {
            this->m_currAchievement = nullptr;
            if (!this->m_achievementUnlockQueue.empty()) {
                this->m_currAchievement = this->m_achievementUnlockQueue.front();
                this->m_achievementUnlockQueue.pop_front();
                this->m_achievementUnlockQueueTimer = 2.5;
            }
        }
    }

}