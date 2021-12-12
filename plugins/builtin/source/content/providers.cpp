#include <hex/api/content_registry.hpp>

#include "content/providers/gdb_provider.hpp"
#include "content/providers/file_provider.hpp"
#include "content/providers/disk_provider.hpp"

namespace hex::plugin::builtin {

    void registerProviders() {

        ContentRegistry::Provider::add<prv::FileProvider>("hex.builtin.provider.file", false);
        ContentRegistry::Provider::add<prv::GDBProvider>("hex.builtin.provider.gdb");
        ContentRegistry::Provider::add<prv::DiskProvider>("hex.builtin.provider.disk");

    }

}