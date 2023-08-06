#include <hex/api/achievement_manager.hpp>

#include <nlohmann/json.hpp>

namespace hex {

    std::unordered_map<std::string, std::unordered_map<std::string, std::unique_ptr<Achievement>>> &AchievementManager::getAchievements() {
        static std::unordered_map<std::string, std::unordered_map<std::string, std::unique_ptr<Achievement>>> achievements;

        return achievements;
    }

    std::unordered_map<std::string, std::list<AchievementManager::AchievementNode>>& AchievementManager::getAchievementNodes(bool rebuild) {
        static std::unordered_map<std::string, std::list<AchievementNode>> nodeCategoryStorage;

        if (!nodeCategoryStorage.empty() || !rebuild)
            return nodeCategoryStorage;

        nodeCategoryStorage.clear();

        // Add all achievements to the node storage
        for (auto &[categoryName, achievements] : getAchievements()) {
            auto &nodes = nodeCategoryStorage[categoryName];

            for (auto &[achievementName, achievement] : achievements) {
                nodes.emplace_back(achievement.get());
            }
        }

        return nodeCategoryStorage;
    }

    std::unordered_map<std::string, std::vector<AchievementManager::AchievementNode*>>& AchievementManager::getAchievementStartNodes(bool rebuild) {
        static std::unordered_map<std::string, std::vector<AchievementNode*>> startNodes;

        if (!startNodes.empty() || !rebuild)
            return startNodes;

        auto &nodeCategoryStorage = getAchievementNodes();

        startNodes.clear();

        // Add all parents and children to the nodes
        for (auto &[categoryName, achievements] : nodeCategoryStorage) {
            for (auto &achievementNode : achievements) {
                for (auto &requirement : achievementNode.achievement->getRequirements()) {
                    for (auto &[requirementCategoryName, requirementAchievements] : nodeCategoryStorage) {
                        auto iter = std::find_if(requirementAchievements.begin(), requirementAchievements.end(), [&requirement](auto &node) {
                            return node.achievement->getUnlocalizedName() == requirement;
                        });

                        if (iter != requirementAchievements.end()) {
                            achievementNode.parents.emplace_back(&*iter);
                            iter->children.emplace_back(&achievementNode);
                        }
                    }
                }

                for (auto &requirement : achievementNode.achievement->getVisibilityRequirements()) {
                    for (auto &[requirementCategoryName, requirementAchievements] : nodeCategoryStorage) {
                        auto iter = std::find_if(requirementAchievements.begin(), requirementAchievements.end(), [&requirement](auto &node) {
                            return node.achievement->getUnlocalizedName() == requirement;
                        });

                        if (iter != requirementAchievements.end()) {
                            achievementNode.visibilityParents.emplace_back(&*iter);
                        }
                    }
                }
            }
        }

        for (auto &[categoryName, achievements] : nodeCategoryStorage) {
            for (auto &achievementNode : achievements) {
                if (!achievementNode.hasParents()) {
                    startNodes[categoryName].emplace_back(&achievementNode);
                }

                for (const auto &parent : achievementNode.parents) {
                    if (parent->achievement->getUnlocalizedCategory() != achievementNode.achievement->getUnlocalizedCategory())
                        startNodes[categoryName].emplace_back(&achievementNode);
                }
            }
        }

        return startNodes;
    }

    void AchievementManager::unlockAchievement(const std::string &unlocalizedCategory, const std::string &unlocalizedName)  {
        auto &categories = getAchievements();

        auto categoryIter = categories.find(unlocalizedCategory);
        if (categoryIter == categories.end()) {
            return;
        }

        auto &[categoryName, achievements] = *categoryIter;

        auto achievementIter = achievements.find(unlocalizedName);

        if (achievementIter == achievements.end()) {
            return;
        }

        auto &nodes = getAchievementNodes()[categoryName];

        for (const auto &node : nodes) {
            auto &achievement = node.achievement;

            if (achievement->getUnlocalizedCategory() != unlocalizedCategory) {
                continue;
            }
            if (achievement->getUnlocalizedName() != unlocalizedName) {
                continue;
            }

            if (node.achievement->isUnlocked()) {
                return;
            }

            for (const auto &requirement : node.parents) {
                if (!requirement->achievement->isUnlocked()) {
                    return;
                }
            }

            achievement->setUnlocked(true);

            if (achievement->isUnlocked())
                EventManager::post<EventAchievementUnlocked>(*achievement);
        }
    }

    void AchievementManager::clear() {
        getAchievements().clear();
        getAchievementStartNodes(false).clear();
        getAchievementNodes(false).clear();
    }

    void AchievementManager::clearTemporary() {
        auto &categories = getAchievements();
        for (auto &[categoryName, achievements] : categories) {
            std::erase_if(achievements, [](auto &data) {
                auto &[achievementName, achievement] = data;
                return achievement->isTemporary();
            });
        }

        std::erase_if(categories, [](auto &data) {
            auto &[categoryName, achievements] = data;
            return achievements.empty();
        });

        getAchievementStartNodes(false).clear();
        getAchievementNodes(false).clear();
    }

    void AchievementManager::achievementAdded() {
        getAchievementStartNodes(false).clear();
        getAchievementNodes(false).clear();
    }

    constexpr static auto AchievementsFile = "achievements.json";

    void AchievementManager::loadProgress() {
        for (const auto &directory : fs::getDefaultPaths(fs::ImHexPath::Config)) {
            auto path = directory / AchievementsFile;

            if (!wolv::io::fs::exists(path)) {
                continue;
            }

            wolv::io::File file(path, wolv::io::File::Mode::Read);

            if (!file.isValid()) {
                continue;
            }

            try {
                auto json = nlohmann::json::parse(file.readString());

                for (const auto &[categoryName, achievements] : getAchievements()) {
                    for (const auto &[achievementName, achievement] : achievements) {
                        try {
                            achievement->setProgress(json[categoryName][achievementName]);
                        } catch (const std::exception &e) {
                            log::warn("Failed to load achievement progress for '{}::{}': {}", categoryName, achievementName, e.what());
                        }
                    }
                }
            } catch (const std::exception &e) {
                log::error("Failed to load achievements: {}", e.what());
            }

        }
    }

    void AchievementManager::storeProgress() {
        for (const auto &directory : fs::getDefaultPaths(fs::ImHexPath::Config)) {
            auto path = directory / AchievementsFile;

            wolv::io::File file(path, wolv::io::File::Mode::Create);

            if (!file.isValid()) {
                continue;
            }

            nlohmann::json json;

            for (const auto &[categoryName, achievements] : getAchievements()) {
                json[categoryName] = nlohmann::json::object();

                for (const auto &[achievementName, achievement] : achievements) {
                    json[categoryName][achievementName] = achievement->getProgress();
                }
            }

            file.writeString(json.dump(4));
            break;
        }
    }

}