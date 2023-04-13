#include <hex/providers/provider.hpp>

#include <hex.hpp>
#include <hex/api/event.hpp>

#include <cmath>
#include <cstring>
#include <map>
#include <optional>

#include <hex/helpers/magic.hpp>
#include <wolv/io/file.hpp>

namespace hex::prv {

    u32 Provider::s_idCounter = 0;

    Provider::Provider() : m_id(s_idCounter++) {
        this->m_patches.emplace_back();
        this->m_currPatches = this->m_patches.begin();
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

    void Provider::save() {
        EventManager::post<EventProviderSaved>(this);
    }
    void Provider::saveAs(const std::fs::path &path) {
        wolv::io::File file(path, wolv::io::File::Mode::Create);

        if (file.isValid()) {
            std::vector<u8> buffer(std::min<size_t>(0xFF'FFFF, this->getActualSize()), 0x00);
            size_t bufferSize = 0;

            for (u64 offset = 0; offset < this->getActualSize(); offset += bufferSize) {
                bufferSize = buffer.size();

                auto [region, valid] = this->getRegionValidity(offset + this->getBaseAddress());
                if (!valid)
                    offset = region.getEndAddress() + 1;

                auto [newRegion, newValid] = this->getRegionValidity(offset + this->getBaseAddress());
                bufferSize = std::min<size_t>(bufferSize, (newRegion.getEndAddress() - offset) + 1);
                bufferSize = std::min<size_t>(bufferSize, this->getActualSize() - offset);

                this->read(offset + this->getBaseAddress(), buffer.data(), bufferSize, true);
                file.writeBuffer(buffer.data(), bufferSize);
            }

            EventManager::post<EventProviderSaved>(this);
        }
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
        return *this->m_currPatches;
    }

    const std::map<u64, u8> &Provider::getPatches() const {
        return *this->m_currPatches;
    }

    void Provider::applyPatches() {
        for (auto &[patchAddress, patch] : getPatches()) {
            this->writeRaw(patchAddress - this->getBaseAddress(), &patch, 1);
        }

        if (!this->isWritable())
            return;

        this->markDirty();

        this->m_patches.emplace_back();
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
        return (this->getActualSize() / PageSize) + (this->getActualSize() % PageSize != 0 ? 1 : 0);
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
        if (createUndo) {
            // Delete all patches after the current one if a modification is made while
            // the current patch list is not at the end of the undo stack
            if (std::next(this->m_currPatches) != this->m_patches.end())
                this->m_patches.erase(std::next(this->m_currPatches), this->m_patches.end());

            createUndoPoint();
        }

        for (u64 i = 0; i < size; i++) {
            u8 patch         = reinterpret_cast<const u8 *>(buffer)[i];
            u8 originalValue = 0x00;
            this->readRaw(offset + i, &originalValue, sizeof(u8));

            if (patch == originalValue)
                getPatches().erase(offset + i);
            else
                getPatches()[offset + i] = patch;

            EventManager::post<EventPatchCreated>(offset, originalValue, patch);
        }

        this->markDirty();

    }

    void Provider::createUndoPoint() {
        this->m_patches.push_back(getPatches());
        this->m_currPatches = std::prev(this->m_patches.end());
    }

    void Provider::undo() {
        if (canUndo())
            this->m_currPatches--;
    }

    void Provider::redo() {
        if (canRedo())
            this->m_currPatches++;
    }

    bool Provider::canUndo() const {
        return this->m_currPatches != this->m_patches.begin();
    }

    bool Provider::canRedo() const {
        return std::next(this->m_currPatches) != this->m_patches.end();
    }

    bool Provider::hasFilePicker() const {
        return false;
    }

    bool Provider::handleFilePicker() {
        return false;
    }

    bool Provider::hasLoadInterface() const {
        return false;
    }

    bool Provider::hasInterface() const {
        return false;
    }

    bool Provider::drawLoadInterface() {
        return true;
    }

    void Provider::drawInterface() {
    }

    nlohmann::json Provider::storeSettings(nlohmann::json settings) const {
        settings["displayName"] = this->getName();
        settings["type"]        = this->getTypeName();

        settings["baseAddress"] = this->m_baseAddress;
        settings["currPage"]    = this->m_currPage;

        return settings;
    }

    void Provider::loadSettings(const nlohmann::json &settings) {
        this->m_baseAddress = settings["baseAddress"];
        this->m_currPage    = settings["currPage"];
    }

    std::pair<Region, bool> Provider::getRegionValidity(u64 address) const {
        if ((address - this->getBaseAddress()) > this->getActualSize())
            return { Region::Invalid(), false };

        bool insideValidRegion = false;

        std::optional<u64> nextRegionAddress;
        for (const auto &overlay : this->m_overlays) {
            Region overlayRegion = { overlay->getAddress(), overlay->getSize() };
            if (!nextRegionAddress.has_value() || overlay->getAddress() < nextRegionAddress) {
                nextRegionAddress = overlayRegion.getStartAddress();
            }

            if (Region { address, 1 }.overlaps(overlayRegion)) {
                insideValidRegion = true;
            }
        }

        for (const auto &[patchAddress, value] : this->m_patches.back()) {
            if (!nextRegionAddress.has_value() || patchAddress < nextRegionAddress)
                nextRegionAddress = patchAddress;

            if (address == patchAddress)
                insideValidRegion = true;
        }

        if (!nextRegionAddress.has_value())
            return { Region::Invalid(), false };
        else
            return { Region { address, *nextRegionAddress - address }, insideValidRegion };
    }


    u32 Provider::getID() const {
        return this->m_id;
    }

    void Provider::setID(u32 id) {
        this->m_id = id;
        if (id > s_idCounter)
            s_idCounter = id + 1;
    }


    [[nodiscard]] std::variant<std::string, i128> Provider::queryInformation(const std::string &category, const std::string &) {
        if (category == "mime")
            return magic::getMIMEType(this);
        else if (category == "description")
            return magic::getDescription(this);
        else if (category == "provider_type")
            return this->getTypeName();
        else
            return 0;
    }

}
