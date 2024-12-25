#include "content/views/view_achievements.hpp"

#include <hex/api/content_registry.hpp>
#include <hex/api/task_manager.hpp>

#include <fonts/vscode_icons.hpp>

#include <cmath>

namespace hex::plugin::builtin {

    ViewAchievements::ViewAchievements() : View::Floating("hex.builtin.view.achievements.name") {
        // Add achievements menu item to Extas menu
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.extras", "hex.builtin.view.achievements.name" }, ICON_VS_SPARKLE, 2600, Shortcut::None, [&, this] {
            this->getWindowOpenState() = true;
        });

        // Add newly unlocked achievements to the display queue
        EventAchievementUnlocked::subscribe(this, [this](const Achievement &achievement) {
            m_achievementUnlockQueue.push_back(&achievement);
            AchievementManager::storeProgress();
        });

        RequestOpenWindow::subscribe(this, [this](const std::string &name) {
            if (name == "Achievements") {
                TaskManager::doLater([this] {
                    this->getWindowOpenState() = true;
                });
            }
        });

        // Load settings
        {
            bool defaultValue = true;
            #if defined(OS_WEB)
                defaultValue = false;
            #endif

            m_showPopup = ContentRegistry::Settings::read<bool>("hex.builtin.setting.interface", "hex.builtin.setting.interface.achievement_popup", defaultValue);
        }
    }

    ViewAchievements::~ViewAchievements() {
        EventAchievementUnlocked::unsubscribe(this);
    }

    void drawAchievement(ImDrawList *drawList, const AchievementManager::AchievementNode *node, ImVec2 position) {
        const auto achievementSize = scaled({ 50, 50 });

        auto &achievement = *node->achievement;

        // Determine achievement border color based on unlock state
        const auto borderColor = [&] {
            if (achievement.isUnlocked())
                return ImGuiExt::GetCustomColorU32(ImGuiCustomCol_AchievementUnlocked, 1.0F);
            else if (node->isUnlockable())
                return ImGui::GetColorU32(ImGuiCol_Button, 1.0F);
            else
                return ImGui::GetColorU32(ImGuiCol_PlotLines, 1.0F);
        }();

        // Determine achievement fill color based on unlock state
        const auto fillColor = [&] {
            if (achievement.isUnlocked()) {
                return ImGui::GetColorU32(ImGuiCol_FrameBg, 1.0F) | 0xFF000000;
            } else if (node->isUnlockable()) {
                auto cycleProgress = sinf(float(ImGui::GetTime()) * 6.0F) * 0.5F + 0.5F;
                return (u32(ImColor(ImLerp(ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled), ImGui::GetStyleColorVec4(ImGuiCol_Text), cycleProgress))) & 0x00FFFFFF) | 0x80000000;
            } else {
                return ImGui::GetColorU32(ImGuiCol_TextDisabled, 0.5F);
            }
        }();

        // Draw achievement background
        if (achievement.isUnlocked()) {
            drawList->AddRectFilled(position, position + achievementSize, fillColor, 5_scaled, 0);
            drawList->AddRect(position, position + achievementSize, borderColor, 5_scaled, 0, 2_scaled);
        } else {
            drawList->AddRectFilled(position, position + achievementSize, ImGui::GetColorU32(ImGuiCol_WindowBg) | 0xFF000000, 5_scaled, 0);
        }

        // Draw achievement icon if available
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

        // Dim achievement if it is not unlocked
        if (!achievement.isUnlocked()) {
            drawList->AddRectFilled(position, position + achievementSize, fillColor, 5_scaled, 0);
            drawList->AddRect(position, position + achievementSize, borderColor, 5_scaled, 0, 2_scaled);
        }

        auto tooltipPos = position + ImVec2(achievementSize.x, 0);
        auto tooltipSize = achievementSize * ImVec2(4, 0);

        // Draw achievement tooltip when hovering over it
        if (ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(position, position + achievementSize)) {
            ImGui::SetNextWindowPos(tooltipPos);
            ImGui::SetNextWindowSize(tooltipSize);
            if (ImGui::BeginTooltip()) {
                if (achievement.isBlacked() && !achievement.isUnlocked()) {
                    // Handle achievements that are blacked out
                    ImGui::TextUnformatted("[ ??? ]");
                } else {
                    // Handle regular achievements

                    ImGui::BeginDisabled(!achievement.isUnlocked());

                    // Draw achievement name
                    ImGui::TextUnformatted(Lang(achievement.getUnlocalizedName()));

                    // Draw progress bar if achievement has progress
                    if (auto requiredProgress = achievement.getRequiredProgress(); requiredProgress > 1) {
                        ImGui::ProgressBar(float(achievement.getProgress()) / float(requiredProgress + 1), ImVec2(achievementSize.x * 4, 5_scaled), "");
                    }

                    bool separator = false;

                    // Draw prompt to click on achievement if it has a click callback
                    if (achievement.getClickCallback() && !achievement.isUnlocked()) {
                        ImGui::Separator();
                        separator = true;

                        ImGuiExt::TextFormattedColored(ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_AchievementUnlocked), "[ {} ]", Lang("hex.builtin.view.achievements.click"));
                    }

                    // Draw achievement description if available
                    if (const auto &desc = achievement.getUnlocalizedDescription(); !desc.empty()) {
                        if (!separator)
                            ImGui::Separator();
                        else
                            ImGui::NewLine();

                        ImGuiExt::TextFormattedWrapped("{}", Lang(desc));
                    }
                    ImGui::EndDisabled();
                }

                ImGui::EndTooltip();
            }

            // Handle achievement click
            if (!achievement.isUnlocked() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                if (ImGui::GetIO().KeyShift) {
                    // Allow achievements to be unlocked in debug mode by shift-clicking them
                    #if defined (DEBUG)
                        AchievementManager::unlockAchievement(node->achievement->getUnlocalizedCategory(), node->achievement->getUnlocalizedName());
                    #endif
                } else {
                    // Trigger achievement click callback
                    if (auto clickCallback = achievement.getClickCallback(); clickCallback)
                        clickCallback(achievement);
                }
            }
        }
    }

    void drawOverlay(ImDrawList *drawList, ImVec2 windowMin, ImVec2 windowMax, const std::string &currCategory) {
        auto &achievements = AchievementManager::getAchievements().at(currCategory);

        // Calculate number of achievements that have been unlocked
        const auto unlockedCount = std::count_if(achievements.begin(), achievements.end(), [](const auto &entry) {
            const auto &[name, achievement] = entry;
            return achievement->isUnlocked();
        });

        // Calculate number of invisible achievements
        const auto invisibleCount = std::count_if(achievements.begin(), achievements.end(), [](const auto &entry) {
            const auto &[name, achievement] = entry;
            return achievement->isInvisible() && !achievement->isUnlocked();
        });

        // Calculate number of visible achievements
        const auto visibleCount = achievements.size() - invisibleCount;

        // Construct number of unlocked achievements text
        auto unlockedText = hex::format("{}: {} / {}{}", "hex.builtin.view.achievements.unlocked_count"_lang, unlockedCount, visibleCount, invisibleCount > 0 ? "+" : " ");

        // Calculate overlay size
        auto &style = ImGui::GetStyle();
        auto overlaySize = ImGui::CalcTextSize(unlockedText.c_str()) + style.ItemSpacing + style.WindowPadding * 2.0F;
        auto padding = scaled({ 10, 10 });

        auto overlayPos = ImVec2(windowMax.x - overlaySize.x - padding.x, windowMin.y + padding.y);

        // Draw overlay background
        drawList->AddRectFilled(overlayPos, overlayPos + overlaySize, ImGui::GetColorU32(ImGuiCol_WindowBg, 0.8F));
        drawList->AddRect(overlayPos, overlayPos + overlaySize, ImGui::GetColorU32(ImGuiCol_Border));

        // Draw overlay content
        ImGui::SetCursorScreenPos(overlayPos + padding);
        ImGui::BeginGroup();

        ImGui::TextUnformatted(unlockedText.c_str());

        ImGui::EndGroup();
    }

    void drawBackground(ImDrawList *drawList, ImVec2 min, ImVec2 max, ImVec2 offset) {
        const auto patternSize = scaled({ 10, 10 });

        const auto darkColor  = ImGui::GetColorU32(ImGuiCol_TableRowBg);
        const auto lightColor = ImGui::GetColorU32(ImGuiCol_TableRowBgAlt);

        // Draw a border around the entire background
        drawList->AddRect(min, max, ImGui::GetColorU32(ImGuiCol_Border), 0.0F, 0, 1.0_scaled);

        // Draw a checkerboard pattern
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

        // Loop over all available achievement nodes
        for (auto &node : nodes) {
            // If the achievement is invisible and not unlocked yet, don't draw anything
            if (node->achievement->isInvisible() && !node->achievement->isUnlocked())
                continue;

            // If the achievement has any visibility requirements, check if they are met
            if (!node->visibilityParents.empty()) {
                bool visible = true;

                // Check if all the visibility requirements are unlocked
                for (auto &parent : node->visibilityParents) {
                    if (!parent->achievement->isUnlocked()) {
                        visible = false;
                        break;
                    }
                }

                // If any of the visibility requirements are not unlocked, don't draw the achievement
                if (!visible)
                    continue;
            }

            drawList->ChannelsSetCurrent(1);

            // Check if the achievement has any parents
            if (prevNode != nullptr) {
                // Check if the parent achievement is in the same category
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

                // Draw a bezier curve between the parent and child achievement
                drawList->AddBezierQuadratic(start, middle, end, color, 2_scaled);

                // Handle jumping to an achievement
                if (m_achievementToGoto != nullptr) {
                    if (m_achievementToGoto == node->achievement) {
                        m_offset = position - scaled({ 100, 100 });
                    }
                }
            }

            drawList->ChannelsSetCurrent(2);

            // Draw the achievement
            drawAchievement(drawList, node, position);

            // Adjust the position for the next achievement and continue drawing the achievement tree
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
        if (ImGui::BeginTabBar("##achievement_categories")) {
            auto &startNodes = AchievementManager::getAchievementStartNodes();

            // Get all achievement category names
            std::vector<std::string> categories;
            for (const auto &[categoryName, achievements] : startNodes) {
                categories.push_back(categoryName);
            }

            std::reverse(categories.begin(), categories.end());

            // Draw each individual achievement category
            for (const auto &categoryName : categories) {
                const auto &achievements = startNodes.at(categoryName);

                // Check if any achievements in the category are unlocked or unlockable
                bool visible = false;
                for (const auto &achievement : achievements) {
                    if (achievement->isUnlocked() || achievement->isUnlockable()) {
                        visible = true;
                        break;
                    }
                }

                // If all achievements in this category are invisible, don't draw it
                if (!visible)
                    continue;

                ImGuiTabItemFlags flags = ImGuiTabItemFlags_None;

                // Handle jumping to the category of an achievement
                if (m_achievementToGoto != nullptr) {
                    if (m_achievementToGoto->getUnlocalizedCategory() == categoryName) {
                        flags |= ImGuiTabItemFlags_SetSelected;
                    }
                }

                // Draw the achievement category
                if (ImGui::BeginTabItem(Lang(categoryName), nullptr, flags)) {
                    auto drawList = ImGui::GetWindowDrawList();

                    const auto cursorPos = ImGui::GetCursorPos();
                    const auto windowPos = ImGui::GetWindowPos() + ImVec2(0, cursorPos.y);
                    const auto windowSize = ImGui::GetWindowSize() - ImVec2(0, cursorPos.y);
                    const float borderSize = 20.0_scaled;

                    const auto windowPadding = ImGui::GetStyle().WindowPadding;
                    const auto innerWindowPos = windowPos + ImVec2(borderSize, borderSize);
                    const auto innerWindowSize = windowSize - ImVec2(borderSize * 2, borderSize * 2) - ImVec2(0, ImGui::GetTextLineHeightWithSpacing());

                    // Prevent the achievement tree from being drawn outside of the window
                    drawList->PushClipRect(innerWindowPos, innerWindowPos + innerWindowSize, true);

                    drawList->ChannelsSplit(4);

                    drawList->ChannelsSetCurrent(0);

                    // Draw achievement background
                    drawBackground(drawList, innerWindowPos, innerWindowPos + innerWindowSize, m_offset);

                    // Draw the achievement tree
                    auto maxPos = drawAchievementTree(drawList, nullptr, achievements, innerWindowPos + scaled({ 100, 100 }) + m_offset);

                    drawList->ChannelsSetCurrent(3);

                    // Draw the achievement overlay
                    drawOverlay(drawList, innerWindowPos, innerWindowPos + innerWindowSize, categoryName);

                    drawList->ChannelsMerge();

                    // Handle dragging the achievement tree around
                    if (ImGui::IsMouseHoveringRect(innerWindowPos, innerWindowPos + innerWindowSize)) {
                        auto dragDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
                        m_offset += dragDelta;
                        ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
                    }

                    // Clamp the achievement tree to the window
                    m_offset = -ImClamp(-m_offset, { 0, 0 }, ImMax(maxPos - innerWindowPos - innerWindowSize, { 0, 0 }));

                    drawList->PopClipRect();

                    // Draw settings below the window
                    ImGui::SetCursorScreenPos(innerWindowPos + ImVec2(0, innerWindowSize.y + windowPadding.y));
                    ImGui::BeginGroup();
                    {
                        if (ImGui::Checkbox("Show popup", &m_showPopup))
                            ContentRegistry::Settings::write<bool>("hex.builtin.setting.interface", "hex.builtin.setting.interface.achievement_popup", m_showPopup);
                    }
                    ImGui::EndGroup();

                    ImGui::EndTabItem();
                }
            }

            ImGui::EndTabBar();
        }

        m_achievementToGoto = nullptr;
    }

    void ViewAchievements::drawAlwaysVisibleContent() {

        // Handle showing the achievement unlock popup
        if (m_achievementUnlockQueueTimer >= 0 && m_showPopup) {
            m_achievementUnlockQueueTimer -= ImGui::GetIO().DeltaTime;

            // Check if there's an achievement that can be drawn
            if (m_currAchievement != nullptr) {

                const ImVec2 windowSize = scaled({ 200, 55 });
                ImGui::SetNextWindowPos(ImHexApi::System::getMainWindowPosition() + ImVec2 { ImHexApi::System::getMainWindowSize().x - windowSize.x - 100_scaled, 0 });
                ImGui::SetNextWindowSize(windowSize);
                if (ImGui::Begin("##achievement_unlocked", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoInputs)) {
                    ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindowRead());

                    // Draw unlock text
                    ImGuiExt::TextFormattedColored(ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_AchievementUnlocked), "{}", "hex.builtin.view.achievements.unlocked"_lang);

                    // Draw achievement icon
                    ImGui::Image(m_currAchievement->getIcon(), scaled({ 20, 20 }));

                    ImGui::SameLine();
                    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
                    ImGui::SameLine();

                    // Draw name of achievement
                    ImGuiExt::TextFormattedWrapped("{}", Lang(m_currAchievement->getUnlocalizedName()));

                    // Handle clicking on the popup
                    if (ImGui::IsWindowHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                        // Open the achievement window and jump to the achievement
                        this->getWindowOpenState() = true;
                        m_achievementToGoto = m_currAchievement;
                    }
                }
                ImGui::End();
            }
        } else {
            // Reset the achievement unlock queue timer
            m_achievementUnlockQueueTimer = -1.0F;
            m_currAchievement = nullptr;

            // If there are more achievements to draw, draw the next one
            if (!m_achievementUnlockQueue.empty()) {
                m_currAchievement = m_achievementUnlockQueue.front();
                m_achievementUnlockQueue.pop_front();
                m_achievementUnlockQueueTimer = 5.0F;
            }
        }
    }

}