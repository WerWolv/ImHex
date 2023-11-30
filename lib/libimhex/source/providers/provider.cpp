#include <hex/providers/provider.hpp>

#include <hex.hpp>
#include <hex/api/event_manager.hpp>

#include <cmath>
#include <cstring>
#include <optional>

#include <hex/helpers/magic.hpp>
#include <wolv/io/file.hpp>

#include <nlohmann/json.hpp>

namespace hex::prv {

    namespace {

        u32 s_idCounter = 0;

    }


    Provider::Provider() : m_undoRedoStack(this), m_id(s_idCounter++) {

    }

    Provider::~Provider() {
        this->m_overlays.clear();

        if (auto selection = ImHexApi::HexEditor::getSelection(); selection.has_value() && selection->provider == this)
            EventManager::post<EventRegionSelected>(ImHexApi::HexEditor::ProviderRegion { { 0x00, 0x00 }, nullptr });
    }

    void Provider::read(u64 offset, void *buffer, size_t size, bool overlays) {
        hex::unused(overlays);

        this->readRaw(offset - this->getBaseAddress(), buffer, size);
    }

    void Provider::write(u64 offset, const void *buffer, size_t size) {
        EventManager::post<EventProviderDataModified>(this, offset, size, static_cast<const u8*>(buffer));
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
        i64 difference = newSize - this->getActualSize();

        if (difference > 0)
            EventManager::post<EventProviderDataInserted>(this, this->getActualSize(), difference);
        else if (difference < 0)
            EventManager::post<EventProviderDataRemoved>(this, this->getActualSize(), -difference);

        this->markDirty();
    }

    void Provider::insert(u64 offset, size_t size) {
        EventManager::post<EventProviderDataInserted>(this, offset, size);

        this->markDirty();
    }

    void Provider::remove(u64 offset, size_t size) {
        EventManager::post<EventProviderDataRemoved>(this, offset, size);

        this->markDirty();
    }

    void Provider::applyOverlays(u64 offset, void *buffer, size_t size) const {
        for (auto &overlay : this->m_overlays) {
            auto overlayOffset = overlay->getAddress();
            auto overlaySize   = overlay->getSize();

            i128 overlapMin = std::max(offset, overlayOffset);
            i128 overlapMax = std::min(offset + size, overlayOffset + overlaySize);
            if (overlapMax > overlapMin)
                std::memcpy(static_cast<u8 *>(buffer) + std::max<i128>(0, overlapMin - offset), overlay->getData().data() + std::max<i128>(0, overlapMin - overlayOffset), overlapMax - overlapMin);
        }
    }

    Overlay *Provider::newOverlay() {
        return this->m_overlays.emplace_back(std::make_unique<Overlay>()).get();
    }

    void Provider::deleteOverlay(Overlay *overlay) {
        this->m_overlays.remove_if([overlay](const auto &item) {
            return item.get() == overlay;
        });
    }

    const std::list<std::unique_ptr<Overlay>> &Provider::getOverlays() const {
        return this->m_overlays;
    }


    size_t Provider::getPageSize() const {
        return this->m_pageSize;
    }

    void Provider::setPageSize(size_t pageSize) {
        if (pageSize > MaxPageSize)
            pageSize = MaxPageSize;
        if (pageSize == 0)
            return;

        this->m_pageSize = pageSize;
    }

    u32 Provider::getPageCount() const {
        return (this->getActualSize() / this->getPageSize()) + (this->getActualSize() % this->getPageSize() != 0 ? 1 : 0);
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
        return this->getPageSize() * this->getCurrentPage();
    }

    size_t Provider::getSize() const {
        return std::min(this->getActualSize() - this->getPageSize() * this->m_currPage, this->getPageSize());
    }

    std::optional<u32> Provider::getPageOfAddress(u64 address) const {
        u32 page = std::floor((address - this->getBaseAddress()) / double(this->getPageSize()));

        if (page >= this->getPageCount())
            return std::nullopt;

        return page;
    }

    std::vector<Provider::Description> Provider::getDataDescription() const {
        return { };
    }

    void Provider::undo() {
        this->m_undoRedoStack.undo();
    }

    void Provider::redo() {
        this->m_undoRedoStack.redo();
    }

    bool Provider::canUndo() const {
        return this->m_undoRedoStack.canUndo();
    }

    bool Provider::canRedo() const {
        return this->m_undoRedoStack.canRedo();
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

    [[nodiscard]] bool Provider::isDumpable() const {
        return true;
    }

}
