#include <hex/providers/provider.hpp>

#include <hex.hpp>

#include <cmath>
#include <cstring>
#include <map>
#include <optional>

namespace hex::prv {

    u32 Provider::s_idCounter = 0;

    Provider::Provider() : m_id(s_idCounter++) {
        this->m_patches.emplace_back();
    }

    Provider::~Provider() {
        for (auto &overlay : this->m_overlays)
            this->deleteOverlay(overlay);
    }

    void Provider::read(u64 offset, void *buffer, size_t size, bool overlays) {
        hex::unused(overlays);

        this->readRaw(offset - this->getBaseAddress(), buffer, size);
    }

    void Provider::write(u64 offset, const void *buffer, size_t size) {
        this->writeRaw(offset - this->getBaseAddress(), buffer, size);
        this->markDirty();
    }

    void Provider::save() { }
    void Provider::saveAs(const std::fs::path &path) {
        hex::unused(path);
    }

    void Provider::resize(size_t newSize) {
        hex::unused(newSize);

        this->markDirty();
    }

    void Provider::insert(u64 offset, size_t size) {
        auto &patches = getPatches();

        std::vector<std::pair<u64, u8>> patchesToMove;

        for (auto &[address, value] : patches) {
            if (address > offset)
                patchesToMove.emplace_back(address, value);
        }

        for (const auto &[address, value] : patchesToMove)
            patches.erase(address);
        for (const auto &[address, value] : patchesToMove)
            patches.insert({ address + size, value });

        this->markDirty();
    }

    void Provider::remove(u64 offset, size_t size) {
        auto &patches = getPatches();

        std::vector<std::pair<u64, u8>> patchesToMove;

        for (auto &[address, value] : patches) {
            if (address > offset)
                patchesToMove.emplace_back(address, value);
        }

        for (const auto &[address, value] : patchesToMove)
            patches.erase(address);
        for (const auto &[address, value] : patchesToMove)
            patches.insert({ address - size, value });

        this->markDirty();
    }

    void Provider::applyOverlays(u64 offset, void *buffer, size_t size) {
        for (auto &overlay : this->m_overlays) {
            auto overlayOffset = overlay->getAddress();
            auto overlaySize   = overlay->getSize();

            i128 overlapMin = std::max(offset, overlayOffset);
            i128 overlapMax = std::min(offset + size, overlayOffset + overlaySize);
            if (overlapMax > overlapMin)
                std::memcpy(static_cast<u8 *>(buffer) + std::max<i128>(0, overlapMin - offset), overlay->getData().data() + std::max<i128>(0, overlapMin - overlayOffset), overlapMax - overlapMin);
        }
    }


    std::map<u64, u8> &Provider::getPatches() {
        auto iter = this->m_patches.end();
        for (u32 i = 0; i < this->m_patchTreeOffset + 1; i++)
            iter--;

        return *(iter);
    }

    const std::map<u64, u8> &Provider::getPatches() const {
        auto iter = this->m_patches.end();
        for (u32 i = 0; i < this->m_patchTreeOffset + 1; i++)
            iter--;

        return *(iter);
    }

    void Provider::applyPatches() {
        for (auto &[patchAddress, patch] : getPatches()) {
            this->writeRaw(patchAddress - this->getBaseAddress(), &patch, 1);
        }
        this->markDirty();
    }


    Overlay *Provider::newOverlay() {
        return this->m_overlays.emplace_back(new Overlay());
    }

    void Provider::deleteOverlay(Overlay *overlay) {
        this->m_overlays.erase(std::find(this->m_overlays.begin(), this->m_overlays.end(), overlay));
        delete overlay;
    }

    const std::list<Overlay *> &Provider::getOverlays() {
        return this->m_overlays;
    }


    u32 Provider::getPageCount() const {
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
        this->markDirty();
    }

    u64 Provider::getBaseAddress() const {
        return this->m_baseAddress;
    }

    u64 Provider::getCurrentPageAddress() const {
        return PageSize * this->getCurrentPage();
    }

    size_t Provider::getSize() const {
        return std::min(this->getActualSize() - PageSize * this->m_currPage, PageSize);
    }

    std::optional<u32> Provider::getPageOfAddress(u64 address) const {
        u32 page = std::floor((address - this->getBaseAddress()) / double(PageSize));

        if (page >= this->getPageCount())
            return std::nullopt;

        return page;
    }

    void Provider::addPatch(u64 offset, const void *buffer, size_t size, bool createUndo) {
        if (this->m_patchTreeOffset > 0) {
            auto iter = this->m_patches.end();
            for (u32 i = 0; i < this->m_patchTreeOffset; i++)
                iter--;

            this->m_patches.erase(iter, this->m_patches.end());
            this->m_patchTreeOffset = 0;
        }

        if (createUndo)
            createUndoPoint();

        for (u64 i = 0; i < size; i++) {
            u8 patch         = reinterpret_cast<const u8 *>(buffer)[i];
            u8 originalValue = 0x00;
            this->readRaw(offset + i, &originalValue, sizeof(u8));

            if (patch == originalValue)
                getPatches().erase(offset + i);
            else
                getPatches()[offset + i] = patch;
        }

        this->markDirty();
    }

    void Provider::createUndoPoint() {
        this->m_patches.push_back(getPatches());
    }

    void Provider::undo() {
        if (canUndo())
            this->m_patchTreeOffset++;
    }

    void Provider::redo() {
        if (canRedo())
            this->m_patchTreeOffset--;
    }

    bool Provider::canUndo() const {
        return this->m_patchTreeOffset < this->m_patches.size() - 1;
    }

    bool Provider::canRedo() const {
        return this->m_patchTreeOffset > 0;
    }


    bool Provider::hasLoadInterface() const {
        return false;
    }

    bool Provider::hasInterface() const {
        return false;
    }

    void Provider::drawLoadInterface() {
    }

    void Provider::drawInterface() {
    }

    nlohmann::json Provider::storeSettings(nlohmann::json settings) const {
        settings["baseAddress"] = this->m_baseAddress;
        settings["currPage"]    = this->m_currPage;

        return settings;
    }

    void Provider::loadSettings(const nlohmann::json &settings) {
        this->m_baseAddress = settings["baseAddress"];
        this->m_currPage    = settings["currPage"];
    }

}
