#pragma once

#include <hex.hpp>

#include <functional>
#include <list>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <imgui.h>
#include <hex/ui/imgui_imhex_extensions.h>
#include <hex/api/localization_manager.hpp>

namespace hex {

    class AchievementManager;

    class Achievement {
    public:
        explicit Achievement(UnlocalizedString unlocalizedCategory, UnlocalizedString unlocalizedName) : m_unlocalizedCategory(std::move(unlocalizedCategory)), m_unlocalizedName(std::move(unlocalizedName)) { }

        /**
         * @brief Returns the unlocalized name of the achievement
         * @return Unlocalized name of the achievement
         */
        [[nodiscard]] const UnlocalizedString &getUnlocalizedName() const {
            return m_unlocalizedName;
        }

        /**
         * @brief Returns the unlocalized category of the achievement
         * @return Unlocalized category of the achievement
         */
        [[nodiscard]] const UnlocalizedString &getUnlocalizedCategory() const {
            return m_unlocalizedCategory;
        }

        /**
         * @brief Returns whether the achievement is unlocked
         * @return Whether the achievement is unlocked
         */
        [[nodiscard]] bool isUnlocked() const {
            return m_progress == m_maxProgress;
        }

        /**
         * @brief Sets the description of the achievement
         * @param description Description of the achievement
         * @return Reference to the achievement
         */
        Achievement& setDescription(std::string description) {
            m_unlocalizedDescription = std::move(description);

            return *this;
        }

        /**
         * @brief Adds a requirement to the achievement. The achievement will only be unlockable if all requirements are unlocked.
         * @param requirement Unlocalized name of the requirement
         * @return Reference to the achievement
         */
        Achievement& addRequirement(std::string requirement) {
            m_requirements.emplace_back(std::move(requirement));

            return *this;
        }

        /**
         * @brief Adds a visibility requirement to the achievement. The achievement will only be visible if all requirements are unlocked.
         * @param requirement Unlocalized name of the requirement
         * @return Reference to the achievement
         */
        Achievement& addVisibilityRequirement(std::string requirement) {
            m_visibilityRequirements.emplace_back(std::move(requirement));

            return *this;
        }

        /**
         * @brief Marks the achievement as blacked. Blacked achievements are visible but their name and description are hidden.
         * @return Reference to the achievement
         */
        Achievement& setBlacked() {
            m_blacked = true;

            return *this;
        }

        /**
         * @brief Marks the achievement as invisible. Invisible achievements are not visible at all.
         * @return Reference to the achievement
         */
        Achievement& setInvisible() {
            m_invisible = true;

            return *this;
        }

        /**
         * @brief Returns whether the achievement is blacked
         * @return Whether the achievement is blacked
         */
        [[nodiscard]] bool isBlacked() const {
            return m_blacked;
        }

        /**
         * @brief Returns whether the achievement is invisible
         * @return Whether the achievement is invisible
         */
        [[nodiscard]] bool isInvisible() const {
            return m_invisible;
        }

        /**
         * @brief Returns the list of requirements of the achievement
         * @return List of requirements of the achievement
         */
        [[nodiscard]] const std::vector<std::string> &getRequirements() const {
            return m_requirements;
        }

        /**
         * @brief Returns the list of visibility requirements of the achievement
         * @return List of visibility requirements of the achievement
         */
        [[nodiscard]] const std::vector<std::string> &getVisibilityRequirements() const {
            return m_visibilityRequirements;
        }

        /**
         * @brief Returns the unlocalized description of the achievement
         * @return Unlocalized description of the achievement
         */
        [[nodiscard]] const UnlocalizedString &getUnlocalizedDescription() const {
            return m_unlocalizedDescription;
        }

        /**
         * @brief Returns the icon of the achievement
         * @return Icon of the achievement
         */
        [[nodiscard]] const ImGuiExt::Texture &getIcon() const {
            if (m_iconData.empty())
                return m_icon;

            if (m_icon.isValid())
                return m_icon;

            m_icon = ImGuiExt::Texture::fromImage(m_iconData.data(), m_iconData.size(), ImGuiExt::Texture::Filter::Linear);

            return m_icon;
        }

        /**
         * @brief Sets the icon of the achievement
         * @param data Icon data
         * @return Reference to the achievement
         */
        Achievement& setIcon(std::span<const std::byte> data) {
            m_iconData.reserve(data.size());
            for (auto &byte : data)
                m_iconData.emplace_back(static_cast<u8>(byte));

            return *this;
        }

        /**
         * @brief Sets the icon of the achievement
         * @param data Icon data
         * @return Reference to the achievement
         */
        Achievement& setIcon(std::span<const u8> data) {
            m_iconData.assign(data.begin(), data.end());

            return *this;
        }

        /**
         * @brief Sets the icon of the achievement
         * @param data Icon data
         * @return Reference to the achievement
         */
        Achievement& setIcon(std::vector<u8> data) {
            m_iconData = std::move(data);

            return *this;
        }

        /**
         * @brief Sets the icon of the achievement
         * @param data Icon data
         * @return Reference to the achievement
         */
        Achievement& setIcon(const std::vector<std::byte> &data) {
            m_iconData.reserve(data.size());
            for (auto &byte : data)
                m_iconData.emplace_back(static_cast<u8>(byte));

            return *this;
        }

        /**
         * @brief Specifies the required progress to unlock the achievement. This is the number of times this achievement has to be triggered to unlock it. The default is 1.
         * @param progress Required progress
         * @return Reference to the achievement
         */
        Achievement& setRequiredProgress(u32 progress) {
            m_maxProgress = progress;

            return *this;
        }

        /**
         * @brief Returns the required progress to unlock the achievement
         * @return Required progress to unlock the achievement
         */
        [[nodiscard]] u32 getRequiredProgress() const {
            return m_maxProgress;
        }

        /**
         * @brief Returns the current progress of the achievement
         * @return Current progress of the achievement
         */
        [[nodiscard]] u32 getProgress() const {
            return m_progress;
        }

        /**
         * @brief Sets the callback to call when the achievement is clicked
         * @param callback Callback to call when the achievement is clicked
         */
        void setClickCallback(const std::function<void(Achievement &)> &callback) {
            m_clickCallback = callback;
        }

        /**
         * @brief Returns the callback to call when the achievement is clicked
         * @return Callback to call when the achievement is clicked
         */
        [[nodiscard]] const std::function<void(Achievement &)> &getClickCallback() const {
            return m_clickCallback;
        }

        /**
         * @brief Returns whether the achievement is temporary. Temporary achievements have been added by challenge projects for example and will be removed when the project is closed.
         * @return Whether the achievement is temporary
         */
        [[nodiscard]] bool isTemporary() const {
            return m_temporary;
        }

        /**
         * @brief Sets whether the achievement is unlocked
         * @param unlocked Whether the achievement is unlocked
         */
        void setUnlocked(bool unlocked) {
            if (unlocked) {
                if (m_progress < m_maxProgress)
                    m_progress++;
            } else {
                m_progress = 0;
            }
        }

    protected:
        void setProgress(u32 progress) {
            m_progress = progress;
        }

    private:
        UnlocalizedString m_unlocalizedCategory, m_unlocalizedName;
        UnlocalizedString m_unlocalizedDescription;

        bool m_blacked = false;
        bool m_invisible = false;
        std::vector<std::string> m_requirements, m_visibilityRequirements;

        std::function<void(Achievement &)> m_clickCallback;

        std::vector<u8> m_iconData;
        mutable ImGuiExt::Texture m_icon;

        u32 m_progress = 0;
        u32 m_maxProgress = 1;

        bool m_temporary = false;

        friend class AchievementManager;
    };

    class AchievementManager {
        static bool s_initialized;
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

        /**
         * @brief Adds a new achievement
         * @tparam T Type of the achievement
         * @param args Arguments to pass to the constructor of the achievement
         * @return Reference to the achievement
         */
        template<std::derived_from<Achievement> T = Achievement>
        static Achievement& addAchievement(auto && ... args) {
            auto newAchievement = std::make_unique<T>(std::forward<decltype(args)>(args)...);

            return addAchievementImpl(std::move(newAchievement));
        }

        /**
         * @brief Adds a new temporary achievement
         * @tparam T Type of the achievement
         * @param args Arguments to pass to the constructor of the achievement
         * @return Reference to the achievement
         */
        template<std::derived_from<Achievement> T = Achievement>
        static Achievement& addTemporaryAchievement(auto && ... args) {
            auto &achievement = addAchievement<T>(std::forward<decltype(args)>(args)...);

            achievement.m_temporary = true;

            return achievement;
        }

        /**
         * @brief Unlocks an achievement
         * @param unlocalizedCategory Unlocalized category of the achievement
         * @param unlocalizedName Unlocalized name of the achievement
         */
        static void unlockAchievement(const UnlocalizedString &unlocalizedCategory, const UnlocalizedString &unlocalizedName);

        /**
         * @brief Returns all registered achievements
         * @return All achievements
         */
        static const std::unordered_map<std::string, std::unordered_map<std::string, std::unique_ptr<Achievement>>>& getAchievements();

        /**
         * @brief Returns all achievement start nodes
         * @note Start nodes are all nodes that don't have any parents
         * @param rebuild Whether to rebuild the list of start nodes
         * @return All achievement start nodes
         */
        static const std::unordered_map<std::string, std::vector<AchievementNode*>>& getAchievementStartNodes(bool rebuild = true);

        /**
         * @brief Returns all achievement nodes
         * @param rebuild Whether to rebuild the list of nodes
         * @return All achievement nodes
         */
        static const std::unordered_map<std::string, std::list<AchievementNode>>& getAchievementNodes(bool rebuild = true);

        /**
         * @brief Loads the progress of all achievements from the achievements save file
         */
        static void loadProgress();

        /**
         * @brief Stores the progress of all achievements to the achievements save file
         */
        static void storeProgress();

        /**
         * @brief Removes all temporary achievements from the tree
         */
        static void clearTemporary();

        /**
         * \brief Returns the current progress of all achievements
         * \return A pair containing the number of unlocked achievements and the total number of achievements
         */
        static std::pair<u32, u32> getProgress();

    private:
        static void achievementAdded();

        static Achievement& addAchievementImpl(std::unique_ptr<Achievement> &&newAchievement);
    };

}