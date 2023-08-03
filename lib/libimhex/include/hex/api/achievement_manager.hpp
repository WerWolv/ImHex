#pragma once

#include <string>
#include <map>
#include <utility>
#include <memory>
#include <vector>
#include <set>

#include <hex/api/event.hpp>
#include <imgui.h>

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
            return this->m_unlocked;
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

    protected:
        void setUnlocked(bool unlocked) {
            this->m_unlocked = unlocked;
        }

    private:
        std::string m_unlocalizedCategory, m_unlocalizedName;
        std::string m_unlocalizedDescription;

        bool m_unlocked = false;
        bool m_blacked = false;
        bool m_invisible = false;
        std::vector<std::string> m_requirements, m_visibilityRequirements;

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

            auto [categoryIter, categoryInserted] = getAchievements().insert({ category, std::map<std::string, std::unique_ptr<Achievement>>{} });
            auto &[categoryKey, achievements] = *categoryIter;

            auto [achievementIter, achievementInserted] = achievements.insert({ name, std::move(newAchievement) });
            auto &[achievementKey, achievement] = *achievementIter;

            achievementAdded();

            return *achievement;
        }

        static void unlockAchievement(const std::string &unlocalizedCategory, const std::string &unlocalizedName);

        static std::map<std::string, std::map<std::string, std::unique_ptr<Achievement>>>& getAchievements();

        static std::map<std::string, std::vector<AchievementNode*>>& getAchievementStartNodes(bool rebuild = true);
        static std::map<std::string, std::list<AchievementNode>>& getAchievementNodes(bool rebuild = true);

        static void clear();

    private:
        static void achievementAdded();
    };

}