#pragma once

#include <string>
#include <unordered_map>
#include <utility>
#include <memory>
#include <vector>
#include <set>
#include <span>

#include <hex/api/event.hpp>
#include <imgui.h>
#include <hex/ui/imgui_imhex_extensions.h>

namespace hex {

    class AchievementManager;

    class Achievement {
    public:
        explicit Achievement(std::string unlocalizedCategory, std::string unlocalizedName) : m_unlocalizedCategory(std::move(unlocalizedCategory)), m_unlocalizedName(std::move(unlocalizedName)) { }

        [[nodiscard]] const std::string &getUnlocalizedName() const {
            return this->m_unlocalizedName;
        }

        [[nodiscard]] const std::string &getUnlocalizedCategory() const {
            return this->m_unlocalizedCategory;
        }

        [[nodiscard]] bool isUnlocked() const {
            return this->m_progress == this->m_maxProgress;
        }

        Achievement& setDescription(std::string description) {
            this->m_unlocalizedDescription = std::move(description);

            return *this;
        }

        Achievement& addRequirement(std::string requirement) {
            this->m_requirements.emplace_back(std::move(requirement));

            return *this;
        }

        Achievement& addVisibilityRequirement(std::string requirement) {
            this->m_visibilityRequirements.emplace_back(std::move(requirement));

            return *this;
        }

        Achievement& setBlacked() {
            this->m_blacked = true;

            return *this;
        }

        Achievement& setInvisible() {
            this->m_invisible = true;

            return *this;
        }

        [[nodiscard]] bool isBlacked() const {
            return this->m_blacked;
        }

        [[nodiscard]] bool isInvisible() const {
            return this->m_invisible;
        }

        [[nodiscard]] const std::vector<std::string> &getRequirements() const {
            return this->m_requirements;
        }

        [[nodiscard]] const std::vector<std::string> &getVisibilityRequirements() const {
            return this->m_visibilityRequirements;
        }

        [[nodiscard]] const std::string &getUnlocalizedDescription() const {
            return this->m_unlocalizedDescription;
        }

        [[nodiscard]] const ImGui::Texture &getIcon() const {
            if (this->m_iconData.empty())
                return this->m_icon;

            if (this->m_icon.isValid())
                return m_icon;

            this->m_icon = ImGui::Texture(reinterpret_cast<const u8*>(this->m_iconData.data()), this->m_iconData.size());

            return this->m_icon;
        }

        Achievement& setIcon(std::span<const std::byte> data) {
            this->m_iconData.reserve(data.size());
            for (auto &byte : data)
                this->m_iconData.emplace_back(static_cast<u8>(byte));

            return *this;
        }

        Achievement& setIcon(std::span<const u8> data) {
            this->m_iconData.assign(data.begin(), data.end());

            return *this;
        }

        Achievement& setIcon(std::vector<u8> data) {
            this->m_iconData = std::move(data);

            return *this;
        }

        Achievement& setIcon(std::vector<std::byte> data) {
            this->m_iconData.reserve(data.size());
            for (auto &byte : data)
                this->m_iconData.emplace_back(static_cast<u8>(byte));

            return *this;
        }

        Achievement& setRequiredProgress(u32 progress) {
            this->m_maxProgress = progress;

            return *this;
        }

        [[nodiscard]] u32 getRequiredProgress() const {
            return this->m_maxProgress;
        }

        [[nodiscard]] u32 getProgress() const {
            return this->m_progress;
        }

        void setClickCallback(const std::function<void(Achievement &)> &callback) {
            this->m_clickCallback = callback;
        }

        [[nodiscard]] const std::function<void(Achievement &)> &getClickCallback() const {
            return this->m_clickCallback;
        }

        [[nodiscard]] bool isTemporary() const {
            return this->m_temporary;
        }

        void setUnlocked(bool unlocked) {
            if (unlocked) {
                if (this->m_progress < this->m_maxProgress)
                    this->m_progress++;
            } else {
                this->m_progress = 0;
            }
        }

    protected:
        void setProgress(u32 progress) {
            this->m_progress = progress;
        }

    private:
        std::string m_unlocalizedCategory, m_unlocalizedName;
        std::string m_unlocalizedDescription;

        bool m_blacked = false;
        bool m_invisible = false;
        std::vector<std::string> m_requirements, m_visibilityRequirements;

        std::function<void(Achievement &)> m_clickCallback;

        std::vector<u8> m_iconData;
        mutable ImGui::Texture m_icon;

        u32 m_progress = 0;
        u32 m_maxProgress = 1;

        bool m_temporary = false;

        friend class AchievementManager;
    };

    class AchievementManager {
    public:
        AchievementManager() = delete;

        struct AchievementNode {
            Achievement *achievement;
            std::vector<AchievementNode*> children, parents;
            std::vector<AchievementNode*> visibilityParents;
            ImVec2 position;

            [[nodiscard]] bool hasParents() const {
                return !this->parents.empty();
            }

            [[nodiscard]] bool isUnlockable() const {
                return std::all_of(this->parents.begin(), this->parents.end(), [](auto &parent) { return parent->achievement->isUnlocked(); });
            }

            [[nodiscard]] bool isVisible() const {
                return std::all_of(this->visibilityParents.begin(), this->visibilityParents.end(), [](auto &parent) { return parent->achievement->isUnlocked(); });
            }

            [[nodiscard]] bool isUnlocked() const {
                return this->achievement->isUnlocked();
            }
        };

        template<std::derived_from<Achievement> T = Achievement>
        static Achievement& addAchievement(auto && ... args) {
            auto newAchievement = std::make_unique<T>(std::forward<decltype(args)>(args)...);

            const auto &category = newAchievement->getUnlocalizedCategory();
            const auto &name = newAchievement->getUnlocalizedName();

            auto [categoryIter, categoryInserted] = getAchievements().insert({ category, std::unordered_map<std::string, std::unique_ptr<Achievement>>{} });
            auto &[categoryKey, achievements] = *categoryIter;

            auto [achievementIter, achievementInserted] = achievements.insert({ name, std::move(newAchievement) });
            auto &[achievementKey, achievement] = *achievementIter;

            achievementAdded();

            return *achievement;
        }

        template<std::derived_from<Achievement> T = Achievement>
        static Achievement& addTemporaryAchievement(auto && ... args) {
            auto &achievement = addAchievement(std::forward<decltype(args)>(args)...);

            achievement.m_temporary = true;

            return achievement;
        }

        static void unlockAchievement(const std::string &unlocalizedCategory, const std::string &unlocalizedName);

        static std::unordered_map<std::string, std::unordered_map<std::string, std::unique_ptr<Achievement>>>& getAchievements();

        static std::unordered_map<std::string, std::vector<AchievementNode*>>& getAchievementStartNodes(bool rebuild = true);
        static std::unordered_map<std::string, std::list<AchievementNode>>& getAchievementNodes(bool rebuild = true);

        static void loadProgress();
        static void storeProgress();

        static void clear();
        static void clearTemporary();

    private:
        static void achievementAdded();
    };

}