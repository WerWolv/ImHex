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

        void registerGettingStartedAchievements() {
            AchievementManager::addAchievement<AchievementStartingOut>("hex.builtin.achievement.starting_out.docs.name")
                    .setDescription("hex.builtin.achievement.starting_out.docs.desc");

            AchievementManager::addAchievement<AchievementStartingOut>("hex.builtin.achievement.starting_out.open_file.name")
                    .setDescription("hex.builtin.achievement.starting_out.open_file.desc");

            AchievementManager::addAchievement<AchievementStartingOut>("hex.builtin.achievement.starting_out.save_project.name")
                    .setDescription("hex.builtin.achievement.starting_out.save_project.desc")
                    .addRequirement("hex.builtin.achievement.starting_out.open_file.name");
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

            AchievementManager::addAchievement<AchievementHexEditor>("hex.builtin.achievement.hex_editor.fill.name")
                    .setDescription("hex.builtin.achievement.hex_editor.fill.desc")
                    .addRequirement("hex.builtin.achievement.hex_editor.select_byte.name")
                    .addVisibilityRequirement("hex.builtin.achievement.hex_editor.select_byte.name");
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

        registerEvents();
    }

}