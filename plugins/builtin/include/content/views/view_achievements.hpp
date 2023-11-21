#pragma once

#include <hex/ui/view.hpp>
#include <hex/api/achievement_manager.hpp>

namespace hex::plugin::builtin {

    class ViewAchievements : public View::Floating {
    public:
        ViewAchievements();
        ~ViewAchievements() override;

        void drawContent() override;
        void drawAlwaysVisibleContent() override;

        [[nodiscard]] bool shouldDraw() const override { return true; }
        [[nodiscard]] bool hasViewMenuItemEntry() const override { return false; }

        [[nodiscard]] ImVec2 getMinSize() const override {
            return scaled({ 800, 600 });
        }

        [[nodiscard]] ImVec2 getMaxSize() const override {
            return scaled({ 1600, 1200 });
        }

    private:
        ImVec2 drawAchievementTree(ImDrawList *drawList, const AchievementManager::AchievementNode * prevNode, const std::vector<AchievementManager::AchievementNode*> &nodes, ImVec2 position);

    private:
        std::list<const Achievement*> m_achievementUnlockQueue;
        const Achievement *m_currAchievement = nullptr;
        const Achievement *m_achievementToGoto = nullptr;
        float m_achievementUnlockQueueTimer = -1;
        bool m_showPopup = true;

        ImVec2 m_offset;
    };

}