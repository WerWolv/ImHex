#include <hex/plugin.hpp>

#include <hex/api/achievement_manager.hpp>
#include <hex/api/content_registry/views.hpp>

#include <hex/helpers/logger.hpp>

#include <romfs/romfs.hpp>

#include <content/views/view_hashes.hpp>
#include <fonts/tabler_icons.hpp>

namespace hex::plugin::hashes {

    void registerHashes();

}

using namespace hex;
using namespace hex::plugin::hashes;

IMHEX_PLUGIN_SETUP("Hashes", "WerWolv", "Hashing algorithms") {
    hex::log::debug("Using romfs: '{}'", romfs::name());
    hex::LocalizationManager::addLanguages(romfs::get("lang/languages.json").string(), [](const std::filesystem::path &path) {
        return romfs::get(path).string();
    });

    registerHashes();
    ContentRegistry::Views::add<ViewHashes>();

    AchievementManager::addAchievement<Achievement>("hex.builtin.achievement.misc", "hex.hashes.achievement.misc.create_hash.name")
        .setDescription("hex.hashes.achievement.misc.create_hash.desc")
        .setIcon(ICON_TA_CRYSTAL_BALL)
        .addRequirement("hex.builtin.achievement.starting_out.open_file.name");
}
