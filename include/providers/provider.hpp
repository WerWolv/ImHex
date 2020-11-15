#pragma once

#include <hex.hpp>

#include <string>
#include <vector>

namespace hex::prv {

    class Provider {
    public:
        Provider() = default;
        virtual ~Provider() = default;

        virtual bool isAvailable() = 0;
        virtual bool isReadable() = 0;
        virtual bool isWritable() = 0;

        virtual void read(u64 offset, void *buffer, size_t size) = 0;
        virtual void write(u64 offset, void *buffer, size_t size) = 0;
        virtual size_t getSize() = 0;

        virtual std::vector<std::pair<std::string, std::string>> getDataInformation() = 0;
    };

}