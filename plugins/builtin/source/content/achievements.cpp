#include <iostream>
#include <fonts/tabler_icons.hpp>
#include <hex/api/achievement_manager.hpp>

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
            explicit AchievementStartingOut(UnlocalizedString unlocalizedName) : Achievement("hex.builtin.achievement.starting_out"_unlocalized, std::move(unlocalizedName)) { }
        };

        class AchievementHexEditor : public Achievement {
        public:
            explicit AchievementHexEditor(UnlocalizedString unlocalizedName) : Achievement("hex.builtin.achievement.hex_editor"_unlocalized, std::move(unlocalizedName)) { }
        };

        class AchievementPatterns : public Achievement {
        public:
            explicit AchievementPatterns(UnlocalizedString unlocalizedName) : Achievement("hex.builtin.achievement.patterns"_unlocalized, std::move(unlocalizedName)) { }
        };

        class AchievementDataProcessor : public Achievement {
        public:
            explicit AchievementDataProcessor(UnlocalizedString unlocalizedName) : Achievement("hex.builtin.achievement.data_processor"_unlocalized, std::move(unlocalizedName)) { }
        };

        class AchievementFind : public Achievement {
        public:
            explicit AchievementFind(UnlocalizedString unlocalizedName) : Achievement("hex.builtin.achievement.find"_unlocalized, std::move(unlocalizedName)) { }
        };

        class AchievementMisc : public Achievement {
        public:
            explicit AchievementMisc(UnlocalizedString unlocalizedName) : Achievement("hex.builtin.achievement.misc"_unlocalized, std::move(unlocalizedName)) { }
        };

        void registerGettingStartedAchievements() {
            AchievementManager::addAchievement<AchievementStartingOut>("hex.builtin.achievement.starting_out.docs.name"_unlocalized)
                    .setDescription("hex.builtin.achievement.starting_out.docs.desc"_unlocalized)
                    .setIcon(ICON_TA_BOOK);

            AchievementManager::addAchievement<AchievementStartingOut>("hex.builtin.achievement.starting_out.open_file.name"_unlocalized)
                    .setDescription("hex.builtin.achievement.starting_out.open_file.desc"_unlocalized)
                    .setIcon(ICON_VS_NEW_FILE);

            AchievementManager::addAchievement<AchievementStartingOut>("hex.builtin.achievement.starting_out.save_project.name"_unlocalized)
                    .setDescription("hex.builtin.achievement.starting_out.save_project.desc"_unlocalized)
                    .setIcon(ICON_VS_NOTEBOOK)
                    .addRequirement("hex.builtin.achievement.starting_out.open_file.name"_unlocalized);


            AchievementManager::addAchievement<AchievementStartingOut>("hex.builtin.achievement.starting_out.crash.name"_unlocalized)
                    .setDescription("hex.builtin.achievement.starting_out.crash.desc"_unlocalized)
                    .setIcon(ICON_TA_BOOM)
                    .setInvisible();
        }

        void registerHexEditorAchievements() {
            AchievementManager::addAchievement<AchievementHexEditor>("hex.builtin.achievement.hex_editor.select_byte.name"_unlocalized)
                    .setDescription("hex.builtin.achievement.hex_editor.select_byte.desc"_unlocalized)
                    .setIcon(ICON_VS_LIST_SELECTION)
                    .addRequirement("hex.builtin.achievement.starting_out.open_file.name"_unlocalized);

            AchievementManager::addAchievement<AchievementHexEditor>("hex.builtin.achievement.hex_editor.open_new_view.name"_unlocalized)
                    .setDescription("hex.builtin.achievement.hex_editor.open_new_view.desc"_unlocalized)
                    .setIcon(ICON_VS_GO_TO_FILE)
                    .addRequirement("hex.builtin.achievement.hex_editor.create_bookmark.name"_unlocalized);

            AchievementManager::addAchievement<AchievementHexEditor>("hex.builtin.achievement.hex_editor.modify_byte.name"_unlocalized)
                    .setDescription("hex.builtin.achievement.hex_editor.modify_byte.desc"_unlocalized)
                    .setIcon(ICON_VS_EDIT)
                    .addRequirement("hex.builtin.achievement.hex_editor.select_byte.name"_unlocalized)
                    .addVisibilityRequirement("hex.builtin.achievement.hex_editor.select_byte.name"_unlocalized);

            AchievementManager::addAchievement<AchievementHexEditor>("hex.builtin.achievement.hex_editor.copy_as.name"_unlocalized)
                    .setDescription("hex.builtin.achievement.hex_editor.copy_as.desc"_unlocalized)
                    .setIcon(ICON_VS_PREVIEW)
                    .addRequirement("hex.builtin.achievement.hex_editor.modify_byte.name"_unlocalized);

            AchievementManager::addAchievement<AchievementHexEditor>("hex.builtin.achievement.hex_editor.create_patch.name"_unlocalized)
                    .setDescription("hex.builtin.achievement.hex_editor.create_patch.desc"_unlocalized)
                    .setIcon(ICON_TA_BANDAGE)
                    .addRequirement("hex.builtin.achievement.hex_editor.modify_byte.name"_unlocalized);

            AchievementManager::addAchievement<AchievementHexEditor>("hex.builtin.achievement.hex_editor.fill.name"_unlocalized)
                    .setDescription("hex.builtin.achievement.hex_editor.fill.desc"_unlocalized)
                    .setIcon(ICON_VS_PAINTCAN)
                    .addRequirement("hex.builtin.achievement.hex_editor.select_byte.name"_unlocalized)
                    .addVisibilityRequirement("hex.builtin.achievement.hex_editor.select_byte.name"_unlocalized);

            AchievementManager::addAchievement<AchievementHexEditor>("hex.builtin.achievement.hex_editor.create_bookmark.name"_unlocalized)
                    .setDescription("hex.builtin.achievement.hex_editor.create_bookmark.desc"_unlocalized)
                    .setIcon(ICON_VS_BOOKMARK)
                    .addRequirement("hex.builtin.achievement.hex_editor.select_byte.name"_unlocalized)
                    .addVisibilityRequirement("hex.builtin.achievement.hex_editor.select_byte.name"_unlocalized);
        }

        void registerPatternsAchievements() {
            AchievementManager::addAchievement<AchievementPatterns>("hex.builtin.achievement.patterns.place_menu.name"_unlocalized)
                    .setDescription("hex.builtin.achievement.patterns.place_menu.desc"_unlocalized)
                    .setIcon(ICON_TA_CATEGORY_2)
                    .addRequirement("hex.builtin.achievement.hex_editor.select_byte.name"_unlocalized);

            AchievementManager::addAchievement<AchievementPatterns>("hex.builtin.achievement.patterns.load_existing.name"_unlocalized)
                    .setDescription("hex.builtin.achievement.patterns.load_existing.desc"_unlocalized)
                    .setIcon(ICON_TA_HOURGLASS)
                    .addRequirement("hex.builtin.achievement.patterns.place_menu.name"_unlocalized);

            AchievementManager::addAchievement<AchievementPatterns>("hex.builtin.achievement.patterns.modify_data.name"_unlocalized)
                    .setDescription("hex.builtin.achievement.patterns.modify_data.desc"_unlocalized)
                    .setIcon(ICON_TA_HAMMER)
                    .addRequirement("hex.builtin.achievement.patterns.place_menu.name"_unlocalized);


            AchievementManager::addAchievement<AchievementPatterns>("hex.builtin.achievement.patterns.data_inspector.name"_unlocalized)
                    .setDescription("hex.builtin.achievement.patterns.data_inspector.desc"_unlocalized)
                    .setIcon(ICON_TA_BUBBLE_TEXT)
                    .addRequirement("hex.builtin.achievement.hex_editor.select_byte.name"_unlocalized);
        }


        void registerFindAchievements() {
            AchievementManager::addAchievement<AchievementFind>("hex.builtin.achievement.find.find_strings.name"_unlocalized)
                    .setDescription("hex.builtin.achievement.find.find_strings.desc"_unlocalized)
                    .setIcon(ICON_TA_HAND_RING_FINGER)
                    .addRequirement("hex.builtin.achievement.starting_out.open_file.name"_unlocalized);

            AchievementManager::addAchievement<AchievementFind>("hex.builtin.achievement.find.find_specific_string.name"_unlocalized)
                    .setDescription("hex.builtin.achievement.find.find_specific_string.desc"_unlocalized)
                    .setIcon(ICON_TA_DIAMOND)
                    .addRequirement("hex.builtin.achievement.find.find_strings.name"_unlocalized);

            AchievementManager::addAchievement<AchievementFind>("hex.builtin.achievement.find.find_numeric.name"_unlocalized)
                    .setDescription("hex.builtin.achievement.find.find_numeric.desc"_unlocalized)
                    .setIcon(ICON_TA_ABACUS)
                    .addRequirement("hex.builtin.achievement.find.find_strings.name"_unlocalized);
        }

        void registerDataProcessorAchievements() {
            AchievementManager::addAchievement<AchievementDataProcessor>("hex.builtin.achievement.data_processor.place_node.name"_unlocalized)
                    .setDescription("hex.builtin.achievement.data_processor.place_node.desc"_unlocalized)
                    .setIcon(ICON_TA_CLOUD)
                    .addRequirement("hex.builtin.achievement.starting_out.open_file.name"_unlocalized);

            AchievementManager::addAchievement<AchievementDataProcessor>("hex.builtin.achievement.data_processor.create_connection.name"_unlocalized)
                    .setDescription("hex.builtin.achievement.data_processor.create_connection.desc"_unlocalized)
                    .setIcon(ICON_TA_SHARE)
                    .addRequirement("hex.builtin.achievement.data_processor.place_node.name"_unlocalized);

            AchievementManager::addAchievement<AchievementDataProcessor>("hex.builtin.achievement.data_processor.modify_data.name"_unlocalized)
                    .setDescription("hex.builtin.achievement.data_processor.modify_data.desc"_unlocalized)
                    .setIcon(ICON_TA_LAYERS_SUBTRACT)
                    .addRequirement("hex.builtin.achievement.data_processor.create_connection.name"_unlocalized);

            AchievementManager::addAchievement<AchievementDataProcessor>("hex.builtin.achievement.data_processor.custom_node.name"_unlocalized)
                    .setDescription("hex.builtin.achievement.data_processor.custom_node.desc"_unlocalized)
                    .setIcon(ICON_TA_MANUAL_GEARBOX)
                    .addRequirement("hex.builtin.achievement.data_processor.create_connection.name"_unlocalized);
        }

        void registerMiscAchievements() {
            AchievementManager::addAchievement<AchievementMisc>("hex.builtin.achievement.misc.analyze_file.name"_unlocalized)
                    .setDescription("hex.builtin.achievement.misc.analyze_file.desc"_unlocalized)
                    .setIcon(ICON_TA_BRAIN)
                    .addRequirement("hex.builtin.achievement.starting_out.open_file.name"_unlocalized);

            AchievementManager::addAchievement<AchievementMisc>("hex.builtin.achievement.misc.download_from_store.name"_unlocalized)
                    .setDescription("hex.builtin.achievement.misc.download_from_store.desc"_unlocalized)
                    .setIcon(ICON_TA_PACKAGE)
                    .addRequirement("hex.builtin.achievement.starting_out.open_file.name"_unlocalized);
        }


        void registerEvents() {
            EventRegionSelected::subscribe([](const auto &region) {
                if (region.getSize() > 1)
                    AchievementManager::unlockAchievement("hex.builtin.achievement.hex_editor"_unlocalized, "hex.builtin.achievement.hex_editor.select_byte.name"_unlocalized);
            });

            EventBookmarkCreated::subscribe([](const auto&) {
                AchievementManager::unlockAchievement("hex.builtin.achievement.hex_editor"_unlocalized, "hex.builtin.achievement.hex_editor.create_bookmark.name"_unlocalized);
            });

            EventProviderDataModified::subscribe([](const prv::Provider *, u64, const u64, const u8*) {
                // Warning: overlaps with the "Flood fill" achievement, since "Fill" works by writing to bytes one-by-one.
                // Thus, we do not check for size, that will always be equal to 1 even during a fill operation.
                AchievementManager::unlockAchievement("hex.builtin.achievement.hex_editor"_unlocalized, "hex.builtin.achievement.hex_editor.modify_byte.name"_unlocalized);
            });

            EventPatchCreated::subscribe([](const u8 *, u64, PatchKind) {
                AchievementManager::unlockAchievement("hex.builtin.achievement.hex_editor"_unlocalized, "hex.builtin.achievement.hex_editor.create_patch.name"_unlocalized);
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

    }

    void registerAchievements() {
        registerGettingStartedAchievements();
        registerHexEditorAchievements();
        registerPatternsAchievements();
        registerFindAchievements();
        registerDataProcessorAchievements();
        registerMiscAchievements();

        registerEvents();
    }

}
