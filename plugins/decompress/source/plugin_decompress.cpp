#include <hex/plugin.hpp>

#include <hex/api/content_registry.hpp>
#include <hex/helpers/logger.hpp>

#include <romfs/romfs.hpp>

namespace hex::plugin::decompress {

    void registerPatternLanguageFunctions();

}

using namespace hex;
using namespace hex::plugin::decompress;

IMHEX_PLUGIN_FEATURES() {
    { "bzip2",      IMHEX_FEATURE_ENABLED(BZIP2)      },
    { "zlib",       IMHEX_FEATURE_ENABLED(ZLIB)       },
    { "LZMA",       IMHEX_FEATURE_ENABLED(LIBLZMA)    },
    { "zstd",       IMHEX_FEATURE_ENABLED(ZSTD)       },
};

IMHEX_PLUGIN_SETUP("Decompressing", "WerWolv", "Support for decompressing data") {
    hex::log::debug("Using romfs: '{}'", romfs::name());

    registerPatternLanguageFunctions();
}