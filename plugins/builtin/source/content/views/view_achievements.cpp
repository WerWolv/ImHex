#include "content/views/view_achievements.hpp"

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

    ImVec2 ViewAchievements::drawAchievementTree(ImDrawList *drawList, const AchievementManager::AchievementNode * prevNode, const std::vector<AchievementManager::AchievementNode*> &nodes, ImVec2 position) {
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

            drawList->ChannelsSetCurrent(0);

            if (prevNode != nullptr) {
                if (prevNode->achievement->getUnlocalizedCategory() != node->achievement->getUnlocalizedCategory())
                    continue;

                auto start = prevNode->position + scaled({ 25, 25 });
                auto end = position + scaled({ 25, 25 });
                auto middle = ((start + end) / 2.0F) - scaled({ 50, 0 });

                const auto color = [prevNode]{
                    if (prevNode->achievement->isUnlocked())
                        return ImGui::GetColorU32(ImGuiCol_Text) | 0xFF000000;
                    else
                        return ImGui::GetColorU32(ImGuiCol_TextDisabled) | 0xFF000000;
                }();

                drawList->AddBezierQuadratic(start, middle, end, color, 2_scaled);

                if (this->m_achievementToGoto != nullptr) {
                    if (this->m_achievementToGoto == node->achievement) {
                        this->m_offset = position - scaled({ 100, 100 });
                    }
                }
            }

            drawList->ChannelsSetCurrent(1);

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
            if (ImGui::BeginTabBar("##achievement_categories")) {
                auto &startNodes = AchievementManager::getAchievementStartNodes();

                std::vector<std::string> categories;
                for (const auto &[categoryName, achievements] : startNodes) {
                    categories.push_back(categoryName);
                }

                std::reverse(categories.begin(), categories.end());

                for (const auto &categoryName : categories) {
                    const auto &achievements = startNodes[categoryName];

                    bool visible = false;
                    for (const auto &achievement : achievements) {
                        if (achievement->isUnlocked() || achievement->isUnlockable()) {
                            visible = true;
                            break;
                        }
                    }

                    if (!visible)
                        continue;

                    ImGuiTabItemFlags flags = ImGuiTabItemFlags_None;

                    if (this->m_achievementToGoto != nullptr) {
                        if (this->m_achievementToGoto->getUnlocalizedCategory() == categoryName) {
                            flags |= ImGuiTabItemFlags_SetSelected;
                        }
                    }

                    if (ImGui::BeginTabItem(LangEntry(categoryName), nullptr, flags)) {
                        auto drawList = ImGui::GetWindowDrawList();

                        const auto cursorPos = ImGui::GetCursorPos();
                        const auto windowPos = ImGui::GetWindowPos() + ImVec2(0, cursorPos.y);
                        const auto windowSize = ImGui::GetWindowSize() - ImVec2(0, cursorPos.y);;
                        const float borderSize = 20.0_scaled;

                        const auto innerWindowPos = windowPos + ImVec2(borderSize, borderSize);
                        const auto innerWindowSize = windowSize - ImVec2(borderSize * 2, borderSize * 2);
                        drawList->PushClipRect(innerWindowPos, innerWindowPos + innerWindowSize, true);
                        drawBackground(drawList, innerWindowPos, innerWindowPos + innerWindowSize, this->m_offset);

                        drawList->ChannelsSplit(2);

                        auto maxPos = drawAchievementTree(drawList, nullptr, achievements, innerWindowPos + scaled({ 100, 100 }) + this->m_offset);

                        drawList->ChannelsMerge();

                        if (ImGui::IsMouseHoveringRect(innerWindowPos, innerWindowPos + innerWindowSize)) {
                            auto dragDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
                            this->m_offset += dragDelta;
                            ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
                        }

                        this->m_offset = -ImClamp(-this->m_offset, { 0, 0 }, ImMax(maxPos - innerWindowPos - innerWindowSize, { 0, 0 }));

                        drawList->PopClipRect();

                        ImGui::EndTabItem();
                    }
                }

                ImGui::EndTabBar();
            }
        }
        ImGui::End();

        this->getWindowOpenState() = this->m_viewOpen;

        this->m_achievementToGoto = nullptr;
    }

    void ViewAchievements::drawAlwaysVisible() {
        if (this->m_achievementUnlockQueueTimer >= 0) {
            this->m_achievementUnlockQueueTimer -= ImGui::GetIO().DeltaTime;

            if (this->m_currAchievement != nullptr) {

                const ImVec2 windowSize = scaled({ 200, 50 });
                ImGui::SetNextWindowPos(ImHexApi::System::getMainWindowPosition() + ImVec2 { ImHexApi::System::getMainWindowSize().x - windowSize.x - 20_scaled, 20_scaled });
                ImGui::SetNextWindowSize(windowSize);
                if (ImGui::Begin("##achievement_unlocked", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar)) {
                    ImGui::TextFormattedColored(ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarYellow), "{}", "hex.builtin.view.achievements.unlocked"_lang);
                    ImGui::TextUnformatted(LangEntry(this->m_currAchievement->getUnlocalizedName()));

                    if (ImGui::IsWindowHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                        this->m_viewOpen = true;
                        this->getWindowOpenState() = this->m_viewOpen;
                        this->m_achievementToGoto = this->m_currAchievement;
                    }
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