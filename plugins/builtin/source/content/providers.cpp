#include <hex/api/content_registry/provider.hpp>

#include "content/providers/gdb_provider.hpp"
#include "content/providers/file_provider.hpp"
#include "content/providers/null_provider.hpp"
#include "content/providers/disk_provider.hpp"
#include "content/providers/intel_hex_provider.hpp"
#include "content/providers/motorola_srec_provider.hpp"
#include "content/providers/memory_file_provider.hpp"
#include "content/providers/view_provider.hpp"
#include <content/providers/process_memory_provider.hpp>
#include <content/providers/base64_provider.hpp>
#include <content/providers/udp_provider.hpp>
#include <content/providers/command_provider.hpp>

namespace hex::plugin::builtin {

    void registerProviders() {

        ContentRegistry::Provider::add<FileProvider>(false);
        ContentRegistry::Provider::add<NullProvider>(false);
        #if !defined(OS_WEB)
            ContentRegistry::Provider::add<UDPProvider>();
            ContentRegistry::Provider::add<GDBProvider>();

            if (!isSandboxed()) {
                ContentRegistry::Provider::add<DiskProvider>();
                ContentRegistry::Provider::add<CommandProvider>();
                #if !defined(OS_FREEBSD)
                    ContentRegistry::Provider::add<ProcessMemoryProvider>();
                #endif
            }
        #endif
        ContentRegistry::Provider::add<IntelHexProvider>();
        ContentRegistry::Provider::add<MotorolaSRECProvider>();
        ContentRegistry::Provider::add<Base64Provider>();
        ContentRegistry::Provider::add<MemoryFileProvider>(false);
        ContentRegistry::Provider::add<ViewProvider>(false);
    }

}
