#pragma once

#include <hex.hpp>

#include <cmath>
#include <concepts>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace hex::prv {

    class Provider {
    public:
        constexpr static size_t PageSize = 0x1000'0000;

        Provider() {
            this->m_patches.emplace_back();
        }

        virtual ~Provider() = default;

        virtual bool isAvailable() = 0;
        virtual bool isReadable() = 0;
        virtual bool isWritable() = 0;

        virtual void read(u64 offset, void *buffer, size_t size) { this->readRaw(offset, buffer, size); }
        virtual void write(u64 offset, const void *buffer, size_t size) { this->writeRaw(offset, buffer, size); }

        virtual void readRaw(u64 offset, void *buffer, size_t size) = 0;
        virtual void writeRaw(u64 offset, const void *buffer, size_t size) = 0;
        virtual size_t getActualSize() = 0;

        std::map<u64, u8>& getPatches() { return this->m_patches.back(); }
        void applyPatches() {
            for (auto &[patchAddress, patch] : this->m_patches.back())
                this->writeRaw(patchAddress, &patch, 1);
        }

        u32 getPageCount() { return std::ceil(this->getActualSize() / double(PageSize)); }
        u32 getCurrentPage() const { return this->m_currPage; }
        void setCurrentPage(u32 page) { if (page < getPageCount()) this->m_currPage = page; }

        virtual size_t getBaseAddress() {
            return PageSize * this->m_currPage;
        }

        virtual size_t getSize() {
            return std::min(this->getActualSize() - PageSize * this->m_currPage, PageSize);
        }

        virtual std::optional<u32> getPageOfAddress(u64 address) {
            u32 page = std::floor(address / double(PageSize));

            if (page >= this->getPageCount())
                return { };

            return page;
        }

        virtual std::vector<std::pair<std::string, std::string>> getDataInformation() = 0;

    protected:
        u32 m_currPage = 0;

        std::vector<std::map<u64, u8>> m_patches;
    };

}