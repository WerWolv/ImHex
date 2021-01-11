#include "providers/provider.hpp"

#include <hex.hpp>

#include <cmath>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace hex::prv {

    Provider::Provider() {
        this->m_patches.emplace_back();
    }

    void Provider::read(u64 offset, void *buffer, size_t size) {
        this->readRaw(offset, buffer, size);
    }

    void Provider::write(u64 offset, const void *buffer, size_t size) {
        this->writeRaw(offset, buffer, size);
    }


    std::map<u64, u8>& Provider::getPatches() {
        return this->m_patches.back();
    }

    void Provider::applyPatches() {
        for (auto &[patchAddress, patch] : this->m_patches.back())
            this->writeRaw(patchAddress, &patch, 1);
    }

    u32 Provider::getPageCount() {
        return std::ceil(this->getActualSize() / double(PageSize));
    }

    u32 Provider::getCurrentPage() const {
        return this->m_currPage;
    }

    void Provider::setCurrentPage(u32 page) {
        if (page < getPageCount())
            this->m_currPage = page;
    }


    void Provider::setBaseAddress(u64 address) {
        this->m_baseAddress = address;
    }

    u64 Provider::getBaseAddress() {
        return this->m_baseAddress + PageSize * this->m_currPage;
    }

    size_t Provider::getSize() {
        return std::min(this->getActualSize() - PageSize * this->m_currPage, PageSize);
    }

    std::optional<u32> Provider::getPageOfAddress(u64 address) {
        u32 page = std::floor(address / double(PageSize));

        if (page >= this->getPageCount())
            return { };

        return page;
    }

}