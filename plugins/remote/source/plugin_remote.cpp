#include <hex/plugin.hpp>

#include <hex/api/content_registry.hpp>
#include <hex/helpers/logger.hpp>

#include <pl/api.hpp>

#include <romfs/romfs.hpp>

#include <libssh2.h>
#include <content/helpers/sftp_client.hpp>
#include <content/providers/ssh_provider.hpp>

IMHEX_PLUGIN_SETUP("Remote", "WerWolv", "Reading data from remote servers") {
    hex::log::debug("Using romfs: '{}'", romfs::name());
    for (auto &path : romfs::list("lang"))
        hex::ContentRegistry::Language::addLocalization(nlohmann::json::parse(romfs::get(path).string()));

    hex::plugin::remote::SFTPClient::init();
    AT_FINAL_CLEANUP {
        hex::plugin::remote::SFTPClient::exit();
    };

    hex::ContentRegistry::Provider::add<hex::plugin::remote::SSHProvider>();
}
