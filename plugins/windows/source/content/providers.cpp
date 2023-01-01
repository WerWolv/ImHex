#include <hex/api/content_registry.hpp>

#include <content/providers/process_memory_provider.hpp>

namespace hex::plugin::windows {

    void registerProviders() {
        ContentRegistry::Provider::add<ProcessMemoryProvider>();
    }

}