#pragma once

#include <hex/providers/provider.hpp>
#include <hex/helpers/literals.hpp>

#include <wolv/io/buffered_reader.hpp>

namespace hex::prv {

    using namespace hex::literals;

    inline void providerReaderFunction(Provider *provider, void *buffer, u64 address, size_t size) {
        provider->read(address, buffer, size);
    }

    class ProviderReader : public wolv::io::BufferedReader<prv::Provider, providerReaderFunction> {
    public:
        using BufferedReader::BufferedReader;

        explicit ProviderReader(Provider *provider, size_t bufferSize = 0x100000) : BufferedReader(provider, provider->getActualSize(), bufferSize) {
            this->setEndAddress(provider->getBaseAddress() + provider->getActualSize() - 1);
            this->seek(provider->getBaseAddress());
        }
    };

}