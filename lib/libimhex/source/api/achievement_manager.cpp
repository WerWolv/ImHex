#include <hex/api/achievement_manager.hpp>
#include <hex/api/event_manager.hpp>

#include <hex/helpers/auto_reset.hpp>
#include <hex/helpers/default_paths.hpp>

#include <nlohmann/json.hpp>

#if defined(OS_WEB)
    #include <emscripten.h>
#endif

namespace hex {

    static AutoReset<std::unordered_map<std::string, std::unordered_map<std::string, std::unique_ptr<Achievement>>>> s_achievements;
    const std::unordered_map<std::string, std::unordered_map<std::string, std::unique_ptr<Achievement>>> &AchievementManager::getAchievements() {
        return *s_achievements;
    }

    static AutoReset<std::unordered_map<std::string, std::list<AchievementManager::AchievementNode>>> s_nodeCategoryStorage;
    std::unordered_map<std::string, std::list<AchievementManager::AchievementNode>>& getAchievementNodesMutable(bool rebuild) {
        if (!s_nodeCategoryStorage->empty() || !rebuild)
            return s_nodeCategoryStorage;

        s_nodeCategoryStorage->clear();

        // Add all achievements to the node storage
        for (auto &[categoryName, achievements] : AchievementManager::getAchievements()) {
            auto &nodes = (*s_nodeCategoryStorage)[categoryName];

            for (auto &[achievementName, achievement] : achievements) {
                nodes.emplace_back(achievement.get());
            }
        }

        return s_nodeCategoryStorage;
    }

    const std::unordered_map<std::string, std::list<AchievementManager::AchievementNode>>& AchievementManager::getAchievementNodes(bool rebuild) {
        return getAchievementNodesMutable(rebuild);
    }

    static AutoReset<std::unordered_map<std::string, std::vector<AchievementManager::AchievementNode*>>> s_startNodes;
    const std::unordered_map<std::string, std::vector<AchievementManager::AchievementNode*>>& AchievementManager::getAchievementStartNodes(bool rebuild) {

        if (!s_startNodes->empty() || !rebuild)
            return s_startNodes;

        auto &nodeCategoryStorage = getAchievementNodesMutable(rebuild);

        s_startNodes->clear();

        // Add all parents and children to the nodes
        for (auto &[categoryName, achievements] : nodeCategoryStorage) {
            for (auto &achievementNode : achievements) {
                for (auto &requirement : achievementNode.achievement->getRequirements()) {
                    for (auto &[requirementCategoryName, requirementAchievements] : nodeCategoryStorage) {
                        auto iter = std::ranges::find_if(requirementAchievements, [&requirement](auto &node) {
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
                        auto iter = std::ranges::find_if(requirementAchievements, [&requirement](auto &node) {
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
                    (*s_startNodes)[categoryName].emplace_back(&achievementNode);
                }

                for (const auto &parent : achievementNode.parents) {
                    if (parent->achievement->getUnlocalizedCategory() != achievementNode.achievement->getUnlocalizedCategory())
                        (*s_startNodes)[categoryName].emplace_back(&achievementNode);
                }
            }
        }

        return s_startNodes;
    }

    void AchievementManager::unlockAchievement(const UnlocalizedString &unlocalizedCategory, const UnlocalizedString &unlocalizedName)  {
        auto &categories = getAchievements();

        auto categoryIter = categories.find(unlocalizedCategory);
        if (categoryIter == categories.end()) {
            return;
        }

        auto &[categoryName, achievements] = *categoryIter;

        const auto achievementIter = achievements.find(unlocalizedName);
        if (achievementIter == achievements.end()) {
            return;
        }

        const auto &nodes = getAchievementNodes();
        if (!nodes.contains(categoryName))
            return;

        for (const auto &node : nodes.at(categoryName)) {
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
                EventAchievementUnlocked::post(*achievement);

            return;
        }
    }

    void AchievementManager::clearTemporary() {
        auto &categories = *s_achievements;
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

        s_startNodes->clear();
        s_nodeCategoryStorage->clear();
    }

    std::pair<u32, u32> AchievementManager::getProgress() {
        u32 unlocked = 0;
        u32 total = 0;

        for (auto &[categoryName, achievements] : getAchievements()) {
            for (auto &[achievementName, achievement] : achievements) {
                total += 1;
                if (achievement->isUnlocked()) {
                    unlocked += 1;
                }
            }
        }

        return { unlocked, total };
    }

    void AchievementManager::achievementAdded() {
        s_startNodes->clear();
        s_nodeCategoryStorage->clear();
    }

    Achievement &AchievementManager::addAchievementImpl(std::unique_ptr<Achievement> &&newAchievement) {
        const auto &category = newAchievement->getUnlocalizedCategory();
        const auto &name = newAchievement->getUnlocalizedName();

        auto [categoryIter, categoryInserted] = s_achievements->insert({ category, std::unordered_map<std::string, std::unique_ptr<Achievement>>{} });
        auto &[categoryKey, achievements] = *categoryIter;

        auto [achievementIter, achievementInserted] = achievements.insert({ name, std::move(newAchievement) });
        auto &[achievementKey, achievement] = *achievementIter;

        achievementAdded();

        return *achievement;
    }


    constexpr static auto AchievementsFile = "achievements.json";
    bool AchievementManager::s_initialized = false;

    void AchievementManager::loadProgress() {
        if (s_initialized)
            return;
        for (const auto &directory : paths::Config.read()) {
            auto path = directory / AchievementsFile;

            if (!wolv::io::fs::exists(path)) {
                continue;
            }

            wolv::io::File file(path, wolv::io::File::Mode::Read);

            if (!file.isValid()) {
                continue;
            }

            try {
                #if defined(OS_WEB)
                    auto data = (char *) MAIN_THREAD_EM_ASM_INT({
                        let data = localStorage.getItem("achievements");
                        return data ? stringToNewUTF8(data) : null;
                    });
                #else
                    auto data = file.readString();
                #endif

                auto json = nlohmann::json::parse(data);

                for (const auto &[categoryName, achievements] : getAchievements()) {
                    for (const auto &[achievementName, achievement] : achievements) {
                        try {
                            const auto &progress = json[categoryName][achievementName];
                            if (progress.is_null())
                                continue;

                            achievement->setProgress(progress);
                        } catch (const std::exception &e) {
                            log::warn("Failed to load achievement progress for '{}::{}': {}", categoryName, achievementName, e.what());
                        }
                    }
                }
                
                s_initialized = true;
            } catch (const std::exception &e) {
                log::error("Failed to load achievements: {}", e.what());
            }

        }
    }

    void AchievementManager::storeProgress() {
        if (!s_initialized)
            loadProgress();
        nlohmann::json json;
        for (const auto &[categoryName, achievements] : getAchievements()) {
            json[categoryName] = nlohmann::json::object();

            for (const auto &[achievementName, achievement] : achievements) {
                json[categoryName][achievementName] = achievement->getProgress();
            }
        }

        if (json.empty())
            return;

        #if defined(OS_WEB)
            auto data = json.dump();
            MAIN_THREAD_EM_ASM({
                localStorage.setItem("achievements", UTF8ToString($0));
            }, data.c_str());
        #else
            for (const auto &directory : paths::Config.write()) {
                auto path = directory / AchievementsFile;

                wolv::io::File file(path, wolv::io::File::Mode::Create);
                if (!file.isValid())
                    continue;

                file.writeString(json.dump(4));
                break;
            }
        #endif
    }

}