#include <iostream>
#include <fonts/tabler_icons.hpp>
#include <hex/api/achievement_manager.hpp>
#include <hex/api/project_file_manager.hpp>

#include <hex/api/events/events_provider.hpp>
#include <hex/api/events/events_lifecycle.hpp>
#include <hex/api/events/events_interaction.hpp>

#include <hex/helpers/crypto.hpp>
#include <hex/providers/provider.hpp>

#include <toasts/toast_notification.hpp>
#include <popups/popup_notification.hpp>
#include <popups/popup_text_input.hpp>

#include <nlohmann/json.hpp>
#include <romfs/romfs.hpp>

namespace hex::plugin::builtin {

    namespace {

        class AchievementStartingOut : public Achievement {
        public:
            explicit AchievementStartingOut(UnlocalizedString unlocalizedName) : Achievement("hex.builtin.achievement.starting_out", std::move(unlocalizedName)) { }
        };

        class AchievementHexEditor : public Achievement {
        public:
            explicit AchievementHexEditor(UnlocalizedString unlocalizedName) : Achievement("hex.builtin.achievement.hex_editor", std::move(unlocalizedName)) { }
        };

        class AchievementPatterns : public Achievement {
        public:
            explicit AchievementPatterns(UnlocalizedString unlocalizedName) : Achievement("hex.builtin.achievement.patterns", std::move(unlocalizedName)) { }
        };

        class AchievementDataProcessor : public Achievement {
        public:
            explicit AchievementDataProcessor(UnlocalizedString unlocalizedName) : Achievement("hex.builtin.achievement.data_processor", std::move(unlocalizedName)) { }
        };

        class AchievementFind : public Achievement {
        public:
            explicit AchievementFind(UnlocalizedString unlocalizedName) : Achievement("hex.builtin.achievement.find", std::move(unlocalizedName)) { }
        };

        class AchievementMisc : public Achievement {
        public:
            explicit AchievementMisc(UnlocalizedString unlocalizedName) : Achievement("hex.builtin.achievement.misc", std::move(unlocalizedName)) { }
        };

        void registerGettingStartedAchievements() {
            AchievementManager::addAchievement<AchievementStartingOut>("hex.builtin.achievement.starting_out.docs.name")
                    .setDescription("hex.builtin.achievement.starting_out.docs.desc")
                    .setIcon(ICON_TA_BOOK);

            AchievementManager::addAchievement<AchievementStartingOut>("hex.builtin.achievement.starting_out.open_file.name")
                    .setDescription("hex.builtin.achievement.starting_out.open_file.desc")
                    .setIcon(ICON_VS_NEW_FILE);

            AchievementManager::addAchievement<AchievementStartingOut>("hex.builtin.achievement.starting_out.save_project.name")
                    .setDescription("hex.builtin.achievement.starting_out.save_project.desc")
                    .setIcon(ICON_VS_NOTEBOOK)
                    .addRequirement("hex.builtin.achievement.starting_out.open_file.name");


            AchievementManager::addAchievement<AchievementStartingOut>("hex.builtin.achievement.starting_out.crash.name")
                    .setDescription("hex.builtin.achievement.starting_out.crash.desc")
                    .setIcon(ICON_TA_BOOM)
                    .setInvisible();
        }

        void registerHexEditorAchievements() {
            AchievementManager::addAchievement<AchievementHexEditor>("hex.builtin.achievement.hex_editor.select_byte.name")
                    .setDescription("hex.builtin.achievement.hex_editor.select_byte.desc")
                    .setIcon(ICON_VS_LIST_SELECTION)
                    .addRequirement("hex.builtin.achievement.starting_out.open_file.name");

            AchievementManager::addAchievement<AchievementHexEditor>("hex.builtin.achievement.hex_editor.open_new_view.name")
                    .setDescription("hex.builtin.achievement.hex_editor.open_new_view.desc")
                    .setIcon(ICON_VS_GO_TO_FILE)
                    .addRequirement("hex.builtin.achievement.hex_editor.create_bookmark.name");

            AchievementManager::addAchievement<AchievementHexEditor>("hex.builtin.achievement.hex_editor.modify_byte.name")
                    .setDescription("hex.builtin.achievement.hex_editor.modify_byte.desc")
                    .setIcon(ICON_VS_EDIT)
                    .addRequirement("hex.builtin.achievement.hex_editor.select_byte.name")
                    .addVisibilityRequirement("hex.builtin.achievement.hex_editor.select_byte.name");

            AchievementManager::addAchievement<AchievementHexEditor>("hex.builtin.achievement.hex_editor.copy_as.name")
                    .setDescription("hex.builtin.achievement.hex_editor.copy_as.desc")
                    .setIcon(ICON_VS_PREVIEW)
                    .addRequirement("hex.builtin.achievement.hex_editor.modify_byte.name");

            AchievementManager::addAchievement<AchievementHexEditor>("hex.builtin.achievement.hex_editor.create_patch.name")
                    .setDescription("hex.builtin.achievement.hex_editor.create_patch.desc")
                    .setIcon(ICON_TA_BANDAGE)
                    .addRequirement("hex.builtin.achievement.hex_editor.modify_byte.name");

            AchievementManager::addAchievement<AchievementHexEditor>("hex.builtin.achievement.hex_editor.fill.name")
                    .setDescription("hex.builtin.achievement.hex_editor.fill.desc")
                    .setIcon(ICON_VS_PAINTCAN)
                    .addRequirement("hex.builtin.achievement.hex_editor.select_byte.name")
                    .addVisibilityRequirement("hex.builtin.achievement.hex_editor.select_byte.name");

            AchievementManager::addAchievement<AchievementHexEditor>("hex.builtin.achievement.hex_editor.create_bookmark.name")
                    .setDescription("hex.builtin.achievement.hex_editor.create_bookmark.desc")
                    .setIcon(ICON_VS_BOOKMARK)
                    .addRequirement("hex.builtin.achievement.hex_editor.select_byte.name")
                    .addVisibilityRequirement("hex.builtin.achievement.hex_editor.select_byte.name");
        }

        void registerPatternsAchievements() {
            AchievementManager::addAchievement<AchievementPatterns>("hex.builtin.achievement.patterns.place_menu.name")
                    .setDescription("hex.builtin.achievement.patterns.place_menu.desc")
                    .setIcon(ICON_TA_CATEGORY_2)
                    .addRequirement("hex.builtin.achievement.hex_editor.select_byte.name");

            AchievementManager::addAchievement<AchievementPatterns>("hex.builtin.achievement.patterns.load_existing.name")
                    .setDescription("hex.builtin.achievement.patterns.load_existing.desc")
                    .setIcon(ICON_TA_HOURGLASS)
                    .addRequirement("hex.builtin.achievement.patterns.place_menu.name");

            AchievementManager::addAchievement<AchievementPatterns>("hex.builtin.achievement.patterns.modify_data.name")
                    .setDescription("hex.builtin.achievement.patterns.modify_data.desc")
                    .setIcon(ICON_TA_HAMMER)
                    .addRequirement("hex.builtin.achievement.patterns.place_menu.name");


            AchievementManager::addAchievement<AchievementPatterns>("hex.builtin.achievement.patterns.data_inspector.name")
                    .setDescription("hex.builtin.achievement.patterns.data_inspector.desc")
                    .setIcon(ICON_TA_BUBBLE_TEXT)
                    .addRequirement("hex.builtin.achievement.hex_editor.select_byte.name");
        }


        void registerFindAchievements() {
            AchievementManager::addAchievement<AchievementFind>("hex.builtin.achievement.find.find_strings.name")
                    .setDescription("hex.builtin.achievement.find.find_strings.desc")
                    .setIcon(ICON_TA_HAND_RING_FINGER)
                    .addRequirement("hex.builtin.achievement.starting_out.open_file.name");

            AchievementManager::addAchievement<AchievementFind>("hex.builtin.achievement.find.find_specific_string.name")
                    .setDescription("hex.builtin.achievement.find.find_specific_string.desc")
                    .setIcon(ICON_TA_DIAMOND)
                    .addRequirement("hex.builtin.achievement.find.find_strings.name");

            AchievementManager::addAchievement<AchievementFind>("hex.builtin.achievement.find.find_numeric.name")
                    .setDescription("hex.builtin.achievement.find.find_numeric.desc")
                    .setIcon(ICON_TA_ABACUS)
                    .addRequirement("hex.builtin.achievement.find.find_strings.name");
        }

        void registerDataProcessorAchievements() {
            AchievementManager::addAchievement<AchievementDataProcessor>("hex.builtin.achievement.data_processor.place_node.name")
                    .setDescription("hex.builtin.achievement.data_processor.place_node.desc")
                    .setIcon(ICON_TA_CLOUD)
                    .addRequirement("hex.builtin.achievement.starting_out.open_file.name");

            AchievementManager::addAchievement<AchievementDataProcessor>("hex.builtin.achievement.data_processor.create_connection.name")
                    .setDescription("hex.builtin.achievement.data_processor.create_connection.desc")
                    .setIcon(ICON_TA_SHARE)
                    .addRequirement("hex.builtin.achievement.data_processor.place_node.name");

            AchievementManager::addAchievement<AchievementDataProcessor>("hex.builtin.achievement.data_processor.modify_data.name")
                    .setDescription("hex.builtin.achievement.data_processor.modify_data.desc")
                    .setIcon(ICON_TA_LAYERS_SUBTRACT)
                    .addRequirement("hex.builtin.achievement.data_processor.create_connection.name");

            AchievementManager::addAchievement<AchievementDataProcessor>("hex.builtin.achievement.data_processor.custom_node.name")
                    .setDescription("hex.builtin.achievement.data_processor.custom_node.desc")
                    .setIcon(ICON_TA_MANUAL_GEARBOX)
                    .addRequirement("hex.builtin.achievement.data_processor.create_connection.name");
        }

        void registerMiscAchievements() {
            AchievementManager::addAchievement<AchievementMisc>("hex.builtin.achievement.misc.analyze_file.name")
                    .setDescription("hex.builtin.achievement.misc.analyze_file.desc")
                    .setIcon(ICON_TA_BRAIN)
                    .addRequirement("hex.builtin.achievement.starting_out.open_file.name");

            AchievementManager::addAchievement<AchievementMisc>("hex.builtin.achievement.misc.download_from_store.name")
                    .setDescription("hex.builtin.achievement.misc.download_from_store.desc")
                    .setIcon(ICON_TA_PACKAGE)
                    .addRequirement("hex.builtin.achievement.starting_out.open_file.name");
        }


        void registerEvents() {
            EventRegionSelected::subscribe([](const auto &region) {
                if (region.getSize() > 1)
                    AchievementManager::unlockAchievement("hex.builtin.achievement.hex_editor", "hex.builtin.achievement.hex_editor.select_byte.name");
            });

            EventBookmarkCreated::subscribe([](const auto&) {
                AchievementManager::unlockAchievement("hex.builtin.achievement.hex_editor", "hex.builtin.achievement.hex_editor.create_bookmark.name");
            });

            EventProviderDataModified::subscribe([](const prv::Provider *, u64, const u64, const u8*) {
                // Warning: overlaps with the "Flood fill" achievement, since "Fill" works by writing to bytes one-by-one.
                // Thus, we do not check for size, that will always be equal to 1 even during a fill operation.
                AchievementManager::unlockAchievement("hex.builtin.achievement.hex_editor", "hex.builtin.achievement.hex_editor.modify_byte.name");
            });

            EventPatchCreated::subscribe([](const u8 *, u64, PatchKind) {
                AchievementManager::unlockAchievement("hex.builtin.achievement.hex_editor", "hex.builtin.achievement.hex_editor.create_patch.name");
            });


            EventImHexStartupFinished::subscribe(AchievementManager::loadProgress);
            EventAchievementUnlocked::subscribe([](const Achievement &) {
                AchievementManager::storeProgress();
            });

            // Clear temporary achievements when the last provider is closed
            EventProviderChanged::subscribe([](hex::prv::Provider *oldProvider, const hex::prv::Provider *newProvider) {
                std::ignore = oldProvider;
                if (newProvider == nullptr) {
                    AchievementManager::clearTemporary();
                }
            });
        }

        void registerChallengeAchievementHandlers() {
            static std::string challengeAchievement;
            static std::string challengeDescription;

            ProjectFile::registerHandler({
                .basePath = "challenge",
                .required = false,
                .load = [](const std::fs::path &basePath, const Tar &tar) {
                    if (!tar.contains(basePath / "achievements.json") || !tar.contains(basePath / "description.txt"))
                        return true;

                    challengeAchievement = tar.readString(basePath / "achievements.json");
                    challengeDescription = tar.readString(basePath / "description.txt");

                    nlohmann::json unlockedJson;
                    if (tar.contains(basePath / "unlocked.json")) {
                        unlockedJson = nlohmann::json::parse(tar.readString(basePath / "unlocked.json"));
                    }

                    try {
                        auto json = nlohmann::json::parse(challengeAchievement);

                        if (json.contains("achievements")) {
                            for (const auto &achievement : json["achievements"]) {
                                auto &newAchievement = AchievementManager::addTemporaryAchievement<Achievement>("hex.builtin.achievement.challenge", achievement["name"].get<std::string>())
                                        .setDescription(achievement["description"]);

                                if (achievement.contains("icon")) {
                                    if (const auto &icon = achievement["icon"]; icon.is_string() && !icon.is_null()) {
                                        auto glyph = icon.get<std::string>();

                                        newAchievement.setIcon(glyph);
                                    }
                                }

                                if (achievement.contains("requirements")) {
                                    if (const auto &requirements = achievement["requirements"]; requirements.is_array()) {
                                        for (const auto &requirement : requirements) {
                                            newAchievement.addRequirement(requirement.get<std::string>());
                                        }
                                    }
                                }

                                if (achievement.contains("visibility_requirements")) {
                                    if (const auto &requirements = achievement["visibility_requirements"]; requirements.is_array()) {
                                        for (const auto &requirement : requirements) {
                                            newAchievement.addVisibilityRequirement(requirement.get<std::string>());
                                        }
                                    }
                                }

                                if (achievement.contains("password")) {
                                    if (const auto &password = achievement["password"]; password.is_string() && !password.is_null()) {
                                        newAchievement.setClickCallback([password = password.get<std::string>()](Achievement &achievement) {
                                            if (password.empty())
                                                achievement.setUnlocked(true);
                                            else
                                                ui::PopupTextInput::open("Enter Password", "Enter the password to unlock this achievement", [password, &achievement](const std::string &input) {
                                                    if (input == password)
                                                        achievement.setUnlocked(true);
                                                    else
                                                        ui::ToastWarning::open("The password you entered was incorrect.");
                                                });
                                        });

                                        if (unlockedJson.contains("achievements") && unlockedJson["achievements"].is_array()) {
                                            for (const auto &unlockedAchievement : unlockedJson["achievements"]) {
                                                if (unlockedAchievement.is_string() && unlockedAchievement.get<std::string>() == achievement["name"].get<std::string>()) {
                                                    newAchievement.setUnlocked(true);
                                                    break;
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    } catch (const nlohmann::json::exception &e) {
                        log::error("Failed to load challenge project: {}", e.what());
                        return false;
                    }

                    ui::PopupInfo::open(challengeDescription);


                    return true;
                },
                .store = [](const std::fs::path &basePath, const Tar &tar) {
                    if (!challengeAchievement.empty())
                        tar.writeString(basePath / "achievements.json", challengeAchievement);
                    if (!challengeDescription.empty())
                        tar.writeString(basePath / "description.txt", challengeDescription);

                    nlohmann::json unlockedJson;

                    unlockedJson["achievements"] = nlohmann::json::array();
                    for (const auto &[categoryName, achievements] : AchievementManager::getAchievements()) {
                        for (const auto &[achievementName, achievement] : achievements) {
                            if (achievement->isTemporary() && achievement->isUnlocked()) {
                                unlockedJson["achievements"].push_back(achievementName);
                            }
                        }
                    }

                    tar.writeString(basePath / "unlocked.json", unlockedJson.dump(4));

                    return true;
                }
            });
        }

    }

    void registerAchievements() {
        registerGettingStartedAchievements();
        registerHexEditorAchievements();
        registerPatternsAchievements();
        registerFindAchievements();
        registerDataProcessorAchievements();
        registerMiscAchievements();

        registerEvents();
        registerChallengeAchievementHandlers();
    }

}