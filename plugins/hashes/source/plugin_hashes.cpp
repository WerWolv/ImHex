#include <hex/plugin.hpp>

#include <hex/api/achievement_manager.hpp>
#include <hex/api/content_registry.hpp>

#include <hex/helpers/logger.hpp>

#include <romfs/romfs.hpp>

#include <content/views/view_hashes.hpp>

namespace hex::plugin::hashes {

    void registerHashes();

}

using namespace hex;
using namespace hex::plugin::hashes;

IMHEX_PLUGIN_SETUP("Hashes", "WerWolv", "Hashing algorithms") {
    hex::log::debug("Using romfs: '{}'", romfs::name());
    for (auto &path : romfs::list("lang"))
        hex::ContentRegistry::Language::addLocalization(nlohmann::json::parse(romfs::get(path).string()));

    registerHashes();
    ContentRegistry::Views::add<ViewHashes>();

    AchievementManager::addAchievement<Achievement>("hex.builtin.achievement.misc", "hex.hashes.achievement.misc.create_hash.name")
        .setDescription("hex.hashes.achievement.misc.create_hash.desc")
        .setIcon(romfs::get("assets/achievements/fortune-cookie.png").span())
        .addRequirement("hex.builtin.achievement.starting_out.open_file.name");
}
