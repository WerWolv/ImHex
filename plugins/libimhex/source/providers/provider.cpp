#include <hex/providers/provider.hpp>

#include <hex.hpp>

#include <cmath>
#include <map>
#include <optional>
#include <string>

namespace hex::prv {

    Provider::Provider() {
        this->m_patches.emplace_back();
    }

    Provider::~Provider() {
        for (auto &overlay : this->m_overlays)
            this->deleteOverlay(overlay);
    }

    void Provider::read(u64 offset, void *buffer, size_t size, bool overlays) {
        this->readRaw(offset, buffer, size);
    }

    void Provider::readRelative(u64 offset, void *buffer, size_t size, bool overlays) {
        this->read(offset + this->getBaseAddress(), buffer, size);
    }

    void Provider::write(u64 offset, const void *buffer, size_t size) {
        this->writeRaw(offset, buffer, size);
    }

    void Provider::writeRelative(u64 offset, const void *buffer, size_t size) {
        this->write(offset + this->getBaseAddress(), buffer, size);
    }

    void Provider::applyOverlays(u64 offset, void *buffer, size_t size) {
        for (auto &overlay : this->m_overlays) {
            auto overlayOffset = overlay->getAddress();
            auto overlaySize = overlay->getSize();

            s128 overlapMin = std::max(offset, overlayOffset);
            s128 overlapMax = std::min(offset + size, overlayOffset + overlaySize);
             if (overlapMax > overlapMin)
                 std::memcpy(static_cast<u8*>(buffer) + std::max<s128>(0, overlapMin - offset), overlay->getData().data() + std::max<s128>(0, overlapMin - overlayOffset), overlapMax - overlapMin);
        }
    }


    std::map<u64, u8>& Provider::getPatches() {
        return *(this->m_patches.end() - 1 - this->m_patchTreeOffset);
    }

    void Provider::applyPatches() {
        for (auto &[patchAddress, patch] : getPatches())
            this->writeRaw(patchAddress, &patch, 1);
    }


    Overlay* Provider::newOverlay() {
        return this->m_overlays.emplace_back(new Overlay());
    }

    void Provider::deleteOverlay(Overlay *overlay) {
        this->m_overlays.erase(std::find(this->m_overlays.begin(), this->m_overlays.end(), overlay));
        delete overlay;
    }

    const std::list<Overlay*>& Provider::getOverlays() {
        return this->m_overlays;
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
        u32 page = std::floor((address - this->getBaseAddress()) / double(PageSize));

        if (page >= this->getPageCount())
            return { };

        return page;
    }

    void Provider::addPatch(u64 offset, const void *buffer, size_t size) {
        if (this->m_patchTreeOffset > 0) {
            this->m_patches.erase(this->m_patches.end() - this->m_patchTreeOffset, this->m_patches.end());
            this->m_patchTreeOffset = 0;
        }

        this->m_patches.push_back(getPatches());

        for (u64 i = 0; i < size; i++)
            getPatches()[offset + i] = reinterpret_cast<const u8*>(buffer)[i];
    }

    void Provider::undo() {
        if (canUndo())
            this->m_patchTreeOffset++;
    }

    void Provider::redo() {
        if (canRedo())
            this->m_patchTreeOffset--;
    }

    bool Provider::canUndo() {
        return this->m_patchTreeOffset < this->m_patches.size() - 1;
    }

    bool Provider::canRedo() {
        return this->m_patchTreeOffset > 0;
    }

}