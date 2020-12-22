#pragma once

#include <hex.hpp>

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace hex::prv {

    class Provider {
    public:
        constexpr static size_t PageSize = 0x1000'0000;

        Provider();
        virtual ~Provider() = default;

        virtual bool isAvailable() = 0;
        virtual bool isReadable() = 0;
        virtual bool isWritable() = 0;

        virtual void read(u64 offset, void *buffer, size_t size);
        virtual void write(u64 offset, const void *buffer, size_t size);

        virtual void readRaw(u64 offset, void *buffer, size_t size) = 0;
        virtual void writeRaw(u64 offset, const void *buffer, size_t size) = 0;
        virtual size_t getActualSize() = 0;

        std::map<u64, u8>& getPatches();
        void applyPatches();

        u32 getPageCount();
        u32 getCurrentPage() const;
        void setCurrentPage(u32 page);

        virtual size_t getBaseAddress();
        virtual size_t getSize();
        virtual std::optional<u32> getPageOfAddress(u64 address);

        virtual std::vector<std::pair<std::string, std::string>> getDataInformation() = 0;

    protected:
        u32 m_currPage = 0;

        std::vector<std::map<u64, u8>> m_patches;
    };

}