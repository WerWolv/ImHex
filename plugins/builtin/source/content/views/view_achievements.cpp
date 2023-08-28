#include "content/views/view_achievements.hpp"

#include <hex/api/content_registry.hpp>
#include <hex/api/task.hpp>

#include <cmath>

#include <imgui_internal.h>

namespace hex::plugin::builtin {

    ViewAchievements::ViewAchievements() : View("hex.builtin.view.achievements.name") {
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.extras", "hex.builtin.view.achievements.name" }, 2600, Shortcut::None, [&, this] {
            this->m_viewOpen = true;
            this->getWindowOpenState() = true;
        });

        EventManager::subscribe<EventAchievementUnlocked>(this, [this](const Achievement &achievement) {
            this->m_achievementUnlockQueue.push_back(&achievement);
        });

        this->m_showPopup = bool(ContentRegistry::Settings::read("hex.builtin.setting.interface", "hex.builtin.setting.interface.achievement_popup", 1));
    }

    ViewAchievements::~ViewAchievements() {
        EventManager::unsubscribe<EventAchievementUnlocked>(this);
    }

    void drawAchievement(ImDrawList *drawList, const AchievementManager::AchievementNode *node, ImVec2 position) {
        const auto achievementSize = scaled({ 50, 50 });

        auto &achievement = *node->achievement;

        const auto borderColor = [&] {
            if (achievement.isUnlocked())
                return ImGui::GetCustomColorU32(ImGuiCustomCol_ToolbarYellow, 1.0F);
            else if (node->isUnlockable())
                return ImGui::GetColorU32(ImGuiCol_Button, 1.0F);
            else
                return ImGui::GetColorU32(ImGuiCol_PlotLines, 1.0F);
        }();

        const auto fillColor = [&] {
            if (achievement.isUnlocked())
                return ImGui::GetColorU32(ImGuiCol_FrameBg, 1.0F) | 0xFF000000;
            else if (node->isUnlockable())
                return (u32(ImColor(ImLerp(ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled), ImGui::GetStyleColorVec4(ImGuiCol_Text), sinf(ImGui::GetTime() * 6.0F) * 0.5F + 0.5F))) & 0x00FFFFFF) | 0x80000000;
            else
                return ImGui::GetColorU32(ImGuiCol_TextDisabled, 0.5F);
        }();

        if (achievement.isUnlocked()) {
            drawList->AddRectFilled(position, position + achievementSize, fillColor, 5_scaled, 0);
            drawList->AddRect(position, position + achievementSize, borderColor, 5_scaled, 0, 2_scaled);
        } else {
            drawList->AddRectFilled(position, position + achievementSize, ImGui::GetColorU32(ImGuiCol_WindowBg) | 0xFF000000, 5_scaled, 0);
        }

        if (const auto &icon = achievement.getIcon(); icon.isValid()) {
            ImVec2 iconSize;
            if (icon.getSize().x > icon.getSize().y) {
                iconSize.x = achievementSize.x;
                iconSize.y = iconSize.x / icon.getSize().x * icon.getSize().y;
            } else {
                iconSize.y = achievementSize.y;
                iconSize.x = iconSize.y / icon.getSize().y * icon.getSize().x;
            }

            iconSize *= 0.7F;

            ImVec2 margin = (achievementSize - iconSize) / 2.0F;
            drawList->AddImage(icon, position + margin, position + margin + iconSize);
        }

        if (!achievement.isUnlocked()) {
            drawList->AddRectFilled(position, position + achievementSize, fillColor, 5_scaled, 0);
            drawList->AddRect(position, position + achievementSize, borderColor, 5_scaled, 0, 2_scaled);
        }

        auto tooltipPos = position + ImVec2(achievementSize.x, 0);
        auto tooltipSize = achievementSize * ImVec2(4, 0);

        if (ImGui::IsMouseHoveringRect(position, position + achievementSize)) {
            ImGui::SetNextWindowPos(tooltipPos);
            ImGui::SetNextWindowSize(tooltipSize);
            if (ImGui::BeginTooltip()) {
                if (achievement.isBlacked() && !achievement.isUnlocked()) {
                    ImGui::TextUnformatted("[ ??? ]");
                } else {
                    ImGui::BeginDisabled(!achievement.isUnlocked());
                    ImGui::TextUnformatted(LangEntry(achievement.getUnlocalizedName()));

                    if (auto requiredProgress = achievement.getRequiredProgress(); requiredProgress > 1) {
                        ImGui::ProgressBar(float(achievement.getProgress()) / float(requiredProgress + 1), ImVec2(achievementSize.x * 4, 5_scaled), "");
                    }

                    bool separator = false;

                    if (achievement.getClickCallback() && !achievement.isUnlocked()) {
                        ImGui::Separator();
                        separator = true;

                        ImGui::TextFormattedColored(ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarYellow), "[ {} ]", LangEntry("hex.builtin.view.achievements.click"));
                    }

                    if (const auto &desc = achievement.getUnlocalizedDescription(); !desc.empty()) {
                        if (!separator)
                            ImGui::Separator();
                        else
                            ImGui::NewLine();

                        ImGui::TextFormattedWrapped("{}", LangEntry(desc));
                    }
                    ImGui::EndDisabled();
                }

                ImGui::EndTooltip();
            }

            if (!achievement.isUnlocked() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                if (ImGui::GetIO().KeyShift) {
                    #if defined (DEBUG)
                        AchievementManager::unlockAchievement(node->achievement->getUnlocalizedCategory(), node->achievement->getUnlocalizedName());
                    #endif
                } else {
                    if (auto clickCallback = achievement.getClickCallback(); clickCallback)
                        clickCallback(achievement);
                }
            }
        }
    }

    void drawOverlay(ImDrawList *drawList, ImVec2 windowMin, ImVec2 windowMax, const std::string &currCategory) {
        auto &achievements = AchievementManager::getAchievements()[currCategory];
        auto unlockedCount = std::count_if(achievements.begin(), achievements.end(), [](const auto &entry) {
            const auto &[name, achievement] = entry;
            return achievement->isUnlocked();
        });

        auto invisibleCount = std::count_if(achievements.begin(), achievements.end(), [](const auto &entry) {
            const auto &[name, achievement] = entry;
            return achievement->isInvisible();
        });

        auto unlockedText = hex::format("{}: {} / {}{}", "hex.builtin.view.achievements.unlocked_count"_lang, unlockedCount, achievements.size() - invisibleCount, invisibleCount > 0 ? "+" : " ");

        auto &style = ImGui::GetStyle();
        auto overlaySize = ImGui::CalcTextSize(unlockedText.c_str()) + style.ItemSpacing + style.WindowPadding * 2.0F;
        auto padding = scaled({ 10, 10 });

        auto overlayPos = ImVec2(windowMax.x - overlaySize.x - padding.x, windowMin.y + padding.y);

        drawList->AddRectFilled(overlayPos, overlayPos + overlaySize, ImGui::GetColorU32(ImGuiCol_WindowBg, 0.8F));
        drawList->AddRect(overlayPos, overlayPos + overlaySize, ImGui::GetColorU32(ImGuiCol_Border));

        ImGui::SetCursorScreenPos(overlayPos + padding);
        ImGui::BeginGroup();

        ImGui::TextUnformatted(unlockedText.c_str());

        ImGui::EndGroup();
    }

    void drawBackground(ImDrawList *drawList, ImVec2 min, ImVec2 max, ImVec2 offset) {
        const auto patternSize = scaled({ 10, 10 });

        const auto darkColor  = ImGui::GetColorU32(ImGuiCol_TableRowBg);
        const auto lightColor = ImGui::GetColorU32(ImGuiCol_TableRowBgAlt);

        drawList->AddRect(min, max, ImGui::GetColorU32(ImGuiCol_Border), 0.0F, 0, 1.0_scaled);

        bool light = false;
        bool prevStart = false;
        for (float x = min.x + offset.x; x < max.x; x += i32(patternSize.x)) {
            if (prevStart == light)
                light = !light;
            prevStart = light;

            for (float y = min.y + offset.y; y < max.y; y += i32(patternSize.y)) {
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

            drawList->ChannelsSetCurrent(1);

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

            drawList->ChannelsSetCurrent(2);

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
        if (ImGui::Begin(View::toWindowName("hex.builtin.view.achievements.name").c_str(), &this->m_viewOpen, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking)) {
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

                        const auto windowPadding = ImGui::GetStyle().WindowPadding;
                        const auto innerWindowPos = windowPos + ImVec2(borderSize, borderSize);
                        const auto innerWindowSize = windowSize - ImVec2(borderSize * 2, borderSize * 2) - ImVec2(0, ImGui::GetTextLineHeightWithSpacing());
                        drawList->PushClipRect(innerWindowPos, innerWindowPos + innerWindowSize, true);

                        drawList->ChannelsSplit(4);

                        drawList->ChannelsSetCurrent(0);

                        drawBackground(drawList, innerWindowPos, innerWindowPos + innerWindowSize, this->m_offset);
                        auto maxPos = drawAchievementTree(drawList, nullptr, achievements, innerWindowPos + scaled({ 100, 100 }) + this->m_offset);

                        drawList->ChannelsSetCurrent(3);

                        drawOverlay(drawList, innerWindowPos, innerWindowPos + innerWindowSize, categoryName);

                        drawList->ChannelsMerge();

                        if (ImGui::IsMouseHoveringRect(innerWindowPos, innerWindowPos + innerWindowSize)) {
                            auto dragDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
                            this->m_offset += dragDelta;
                            ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
                        }

                        this->m_offset = -ImClamp(-this->m_offset, { 0, 0 }, ImMax(maxPos - innerWindowPos - innerWindowSize, { 0, 0 }));

                        drawList->PopClipRect();

                        ImGui::SetCursorScreenPos(innerWindowPos + ImVec2(0, innerWindowSize.y + windowPadding.y));
                        ImGui::BeginGroup();
                        {
                            if (ImGui::Checkbox("Show popup", &this->m_showPopup))
                                ContentRegistry::Settings::write("hex.builtin.setting.interface", "hex.builtin.setting.interface.achievement_popup", i64(this->m_showPopup));
                        }
                        ImGui::EndGroup();

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
        if (this->m_achievementUnlockQueueTimer >= 0 && this->m_showPopup) {
            this->m_achievementUnlockQueueTimer -= ImGui::GetIO().DeltaTime;

            if (this->m_currAchievement != nullptr) {

                const ImVec2 windowSize = scaled({ 200, 55 });
                ImGui::SetNextWindowPos(ImHexApi::System::getMainWindowPosition() + ImVec2 { ImHexApi::System::getMainWindowSize().x - windowSize.x - 100_scaled, 0 });
                ImGui::SetNextWindowSize(windowSize);
                if (ImGui::Begin("##achievement_unlocked", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoInputs)) {
                    ImGui::TextFormattedColored(ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarYellow), "{}", "hex.builtin.view.achievements.unlocked"_lang);

                    ImGui::Image(this->m_currAchievement->getIcon(), scaled({ 20, 20 }));
                    ImGui::SameLine();
                    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
                    ImGui::SameLine();
                    ImGui::TextFormattedWrapped("{}", LangEntry(this->m_currAchievement->getUnlocalizedName()));

                    if (ImGui::IsWindowHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                        this->m_viewOpen = true;
                        this->getWindowOpenState() = this->m_viewOpen;
                        this->m_achievementToGoto = this->m_currAchievement;
                    }
                }
                ImGui::End();
            }
        } else {
            this->m_achievementUnlockQueueTimer = -1.0F;
            this->m_currAchievement = nullptr;
            if (!this->m_achievementUnlockQueue.empty()) {
                this->m_currAchievement = this->m_achievementUnlockQueue.front();
                this->m_achievementUnlockQueue.pop_front();
                this->m_achievementUnlockQueueTimer = 2.5F;
            }
        }
    }

}