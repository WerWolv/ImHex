#include <hex/api/achievement_manager.hpp>

namespace hex::plugin::builtin {

    namespace {

        class AchievementStartingOut : public Achievement {
        public:
            explicit AchievementStartingOut(std::string unlocalizedName) : Achievement("hex.builtin.achievement.starting_out", std::move(unlocalizedName)) { }
        };

        class AchievementHexEditor : public Achievement {
        public:
            explicit AchievementHexEditor(std::string unlocalizedName) : Achievement("hex.builtin.achievement.hex_editor", std::move(unlocalizedName)) { }
        };

        class AchievementPatterns : public Achievement {
        public:
            explicit AchievementPatterns(std::string unlocalizedName) : Achievement("hex.builtin.achievement.patterns", std::move(unlocalizedName)) { }
        };

        class AchievementDataProcessor : public Achievement {
        public:
            explicit AchievementDataProcessor(std::string unlocalizedName) : Achievement("hex.builtin.achievement.data_processor", std::move(unlocalizedName)) { }
        };

        class AchievementFind : public Achievement {
        public:
            explicit AchievementFind(std::string unlocalizedName) : Achievement("hex.builtin.achievement.find", std::move(unlocalizedName)) { }
        };

        class AchievementMisc : public Achievement {
        public:
            explicit AchievementMisc(std::string unlocalizedName) : Achievement("hex.builtin.achievement.misc", std::move(unlocalizedName)) { }
        };

        void registerGettingStartedAchievements() {
            AchievementManager::addAchievement<AchievementStartingOut>("hex.builtin.achievement.starting_out.docs.name")
                    .setDescription("hex.builtin.achievement.starting_out.docs.desc");

            AchievementManager::addAchievement<AchievementStartingOut>("hex.builtin.achievement.starting_out.open_file.name")
                    .setDescription("hex.builtin.achievement.starting_out.open_file.desc");

            AchievementManager::addAchievement<AchievementStartingOut>("hex.builtin.achievement.starting_out.save_project.name")
                    .setDescription("hex.builtin.achievement.starting_out.save_project.desc")
                    .addRequirement("hex.builtin.achievement.starting_out.open_file.name");


            AchievementManager::addAchievement<AchievementStartingOut>("hex.builtin.achievement.starting_out.crash.name")
                    .setDescription("hex.builtin.achievement.starting_out.crash.desc")
                    .setInvisible();
        }

        void registerHexEditorAchievements() {
            AchievementManager::addAchievement<AchievementHexEditor>("hex.builtin.achievement.hex_editor.select_byte.name")
                    .setDescription("hex.builtin.achievement.hex_editor.select_byte.desc")
                    .addRequirement("hex.builtin.achievement.starting_out.open_file.name");

            AchievementManager::addAchievement<AchievementHexEditor>("hex.builtin.achievement.hex_editor.create_bookmark.name")
                    .setDescription("hex.builtin.achievement.hex_editor.create_bookmark.desc")
                    .addRequirement("hex.builtin.achievement.hex_editor.select_byte.name")
                    .addVisibilityRequirement("hex.builtin.achievement.hex_editor.select_byte.name");

            AchievementManager::addAchievement<AchievementHexEditor>("hex.builtin.achievement.hex_editor.open_new_view.name")
                    .setDescription("hex.builtin.achievement.hex_editor.open_new_view.desc")
                    .addRequirement("hex.builtin.achievement.hex_editor.create_bookmark.name");

            AchievementManager::addAchievement<AchievementHexEditor>("hex.builtin.achievement.hex_editor.modify_byte.name")
                    .setDescription("hex.builtin.achievement.hex_editor.modify_byte.desc")
                    .addRequirement("hex.builtin.achievement.hex_editor.select_byte.name")
                    .addVisibilityRequirement("hex.builtin.achievement.hex_editor.select_byte.name");

            AchievementManager::addAchievement<AchievementHexEditor>("hex.builtin.achievement.hex_editor.copy_as.name")
                    .setDescription("hex.builtin.achievement.hex_editor.copy_as.desc")
                    .addRequirement("hex.builtin.achievement.hex_editor.modify_byte.name");

            AchievementManager::addAchievement<AchievementHexEditor>("hex.builtin.achievement.hex_editor.create_patch.name")
                    .setDescription("hex.builtin.achievement.hex_editor.create_patch.desc")
                    .addRequirement("hex.builtin.achievement.hex_editor.modify_byte.name");

            AchievementManager::addAchievement<AchievementHexEditor>("hex.builtin.achievement.hex_editor.fill.name")
                    .setDescription("hex.builtin.achievement.hex_editor.fill.desc")
                    .addRequirement("hex.builtin.achievement.hex_editor.select_byte.name")
                    .addVisibilityRequirement("hex.builtin.achievement.hex_editor.select_byte.name");
        }

        void registerPatternsAchievements() {
            AchievementManager::addAchievement<AchievementPatterns>("hex.builtin.achievement.patterns.place_menu.name")
                    .setDescription("hex.builtin.achievement.patterns.place_menu.desc")
                    .addRequirement("hex.builtin.achievement.hex_editor.select_byte.name");

            AchievementManager::addAchievement<AchievementPatterns>("hex.builtin.achievement.patterns.load_existing.name")
                    .setDescription("hex.builtin.achievement.patterns.load_existing.desc")
                    .addRequirement("hex.builtin.achievement.patterns.place_menu.name");

            AchievementManager::addAchievement<AchievementPatterns>("hex.builtin.achievement.patterns.modify_data.name")
                    .setDescription("hex.builtin.achievement.patterns.modify_data.desc")
                    .addRequirement("hex.builtin.achievement.patterns.place_menu.name");


            AchievementManager::addAchievement<AchievementPatterns>("hex.builtin.achievement.patterns.data_inspector.name")
                    .setDescription("hex.builtin.achievement.patterns.data_inspector.desc")
                    .addRequirement("hex.builtin.achievement.hex_editor.select_byte.name");
        }


        void registerFindAchievements() {
            AchievementManager::addAchievement<AchievementFind>("hex.builtin.achievement.find.find_strings.name")
                    .setDescription("hex.builtin.achievement.find.find_string.desc")
                    .addRequirement("hex.builtin.achievement.starting_out.open_file.name");

            AchievementManager::addAchievement<AchievementFind>("hex.builtin.achievement.find.find_specific_string.name")
                    .setDescription("hex.builtin.achievement.find.find_specific_string.desc")
                    .addRequirement("hex.builtin.achievement.find.find_strings.name");

            AchievementManager::addAchievement<AchievementFind>("hex.builtin.achievement.find.find_numeric.name")
                    .setDescription("hex.builtin.achievement.find.find_numeric.desc")
                    .addRequirement("hex.builtin.achievement.find.find_strings.name");
        }

        void registerDataProcessorAchievements() {
            AchievementManager::addAchievement<AchievementDataProcessor>("hex.builtin.achievement.data_processor.place_node.name")
                    .setDescription("hex.builtin.achievement.data_processor.place_node.desc")
                    .addRequirement("hex.builtin.achievement.starting_out.open_file.name");

            AchievementManager::addAchievement<AchievementDataProcessor>("hex.builtin.achievement.data_processor.create_connection.name")
                    .setDescription("hex.builtin.achievement.data_processor.create_connection.desc")
                    .addRequirement("hex.builtin.achievement.data_processor.place_node.name");

            AchievementManager::addAchievement<AchievementDataProcessor>("hex.builtin.achievement.data_processor.modify_data.name")
                    .setDescription("hex.builtin.achievement.data_processor.modify_data.desc")
                    .addRequirement("hex.builtin.achievement.data_processor.create_connection.name");

            AchievementManager::addAchievement<AchievementDataProcessor>("hex.builtin.achievement.data_processor.custom_node.name")
                    .setDescription("hex.builtin.achievement.data_processor.custom_node.desc")
                    .addRequirement("hex.builtin.achievement.data_processor.create_connection.name");
        }

        void registerMiscAchievements() {
            AchievementManager::addAchievement<AchievementMisc>("hex.builtin.achievement.misc.analyze_file.name")
                    .setDescription("hex.builtin.achievement.misc.analyze_file.desc")
                    .addRequirement("hex.builtin.achievement.starting_out.open_file.name");

            AchievementManager::addAchievement<AchievementMisc>("hex.builtin.achievement.misc.download_from_store.name")
                    .setDescription("hex.builtin.achievement.misc.download_from_store.desc")
                    .addRequirement("hex.builtin.achievement.starting_out.open_file.name");

            AchievementManager::addAchievement<AchievementMisc>("hex.builtin.achievement.misc.create_hash.name")
                    .setDescription("hex.builtin.achievement.misc.create_hash.desc")
                    .addRequirement("hex.builtin.achievement.starting_out.open_file.name");
        }


        void registerEvents() {
            EventManager::subscribe<EventRegionSelected>([](const auto &region) {
                if (region.getSize() > 1)
                    AchievementManager::unlockAchievement("hex.builtin.achievement.hex_editor", "hex.builtin.achievement.hex_editor.select_byte.name");
            });

            EventManager::subscribe<EventBookmarkCreated>([](const auto&) {
                AchievementManager::unlockAchievement("hex.builtin.achievement.hex_editor", "hex.builtin.achievement.hex_editor.create_bookmark.name");
            });

            EventManager::subscribe<EventPatchCreated>([](u64, u8, u8){
                AchievementManager::unlockAchievement("hex.builtin.achievement.hex_editor", "hex.builtin.achievement.hex_editor.modify_byte.name");
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