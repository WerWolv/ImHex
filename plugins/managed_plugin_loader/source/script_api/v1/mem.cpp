#include <script_api.hpp>

#include <hex/api/imhex_api.hpp>
#include <hex/providers/provider.hpp>

#define VERSION V1

SCRIPT_API(void readMemory, u64 address, size_t size, void *buffer) {
    auto provider = hex::ImHexApi::Provider::get();

    if (provider == nullptr) {
        return;
    }

    provider->read(address, buffer, size);
}

SCRIPT_API(void writeMemory, u64 address, size_t size, void *buffer) {
    auto provider = hex::ImHexApi::Provider::get();

    if (provider == nullptr) {
        return;
    }

    provider->write(address, buffer, size);
}