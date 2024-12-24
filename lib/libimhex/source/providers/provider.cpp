#include <hex/providers/provider.hpp>

#include <hex.hpp>
#include <hex/api/event_manager.hpp>

#include <cmath>
#include <cstring>
#include <optional>

#include <hex/helpers/magic.hpp>
#include <wolv/io/file.hpp>
#include <wolv/literals.hpp>

#include <nlohmann/json.hpp>

namespace hex::prv {

    using namespace wolv::literals;

    namespace {

        u32 s_idCounter = 0;

    }


    Provider::Provider() : m_undoRedoStack(this), m_id(s_idCounter++) {

    }

    Provider::~Provider() {
        m_overlays.clear();

        if (auto selection = ImHexApi::HexEditor::getSelection(); selection.has_value() && selection->provider == this)
            EventRegionSelected::post(ImHexApi::HexEditor::ProviderRegion { { 0x00, 0x00 }, nullptr });
    }

    void Provider::read(u64 offset, void *buffer, size_t size, bool overlays) {
        this->readRaw(offset - this->getBaseAddress(), buffer, size);

        if (overlays)
            this->applyOverlays(offset, buffer, size);
    }

    void Provider::write(u64 offset, const void *buffer, size_t size) {
        if (!this->isWritable())
            return;

        EventProviderDataModified::post(this, offset, size, static_cast<const u8*>(buffer));
        this->markDirty();
    }

    void Provider::save() {
        if (!this->isWritable())
            return;
        
        EventProviderSaved::post(this);
    }
    void Provider::saveAs(const std::fs::path &path) {
        wolv::io::File file(path, wolv::io::File::Mode::Create);

        if (file.isValid()) {
            std::vector<u8> buffer(std::min<size_t>(2_MiB, this->getActualSize()), 0x00);
            size_t bufferSize = 0;

            for (u64 offset = 0; offset < this->getActualSize(); offset += bufferSize) {
                bufferSize = std::min<size_t>(buffer.size(), this->getActualSize() - offset);

                this->read(this->getBaseAddress() + offset, buffer.data(), bufferSize, true);
                file.writeBuffer(buffer.data(), bufferSize);
            }

            EventProviderSaved::post(this);
        }
    }

    bool Provider::resize(u64 newSize) {
        if (newSize >> 63) {
            log::error("new provider size is very large ({}). Is it a negative number ?", newSize);
            return false;
        }
        i64 difference = newSize - this->getActualSize();

        if (difference > 0)
            EventProviderDataInserted::post(this, this->getActualSize(), difference);
        else if (difference < 0)
            EventProviderDataRemoved::post(this, this->getActualSize() + difference, -difference);

        this->markDirty();
        return true;
    }

    void Provider::insert(u64 offset, u64 size) {
        EventProviderDataInserted::post(this, offset, size);

        this->markDirty();
    }

    void Provider::remove(u64 offset, u64 size) {
        EventProviderDataRemoved::post(this, offset, size);

        this->markDirty();
    }

    void Provider::insertRaw(u64 offset, u64 size) {
        auto oldSize = this->getActualSize();
        auto newSize = oldSize + size;
        this->resizeRaw(newSize);

        std::vector<u8> buffer(0x1000);
        const std::vector<u8> zeroBuffer(0x1000);

        auto position = oldSize;
        while (position > offset) {
            const auto readSize = std::min<size_t>(position - offset, buffer.size());

            position -= readSize;

            this->readRaw(position, buffer.data(), readSize);
            this->writeRaw(position, zeroBuffer.data(), newSize - oldSize);
            this->writeRaw(position + size, buffer.data(), readSize);
        }
    }

    void Provider::removeRaw(u64 offset, u64 size) {
        if (offset > this->getActualSize() || size == 0)
            return;

        if ((offset + size) > this->getActualSize())
            size = this->getActualSize() - offset;

        auto oldSize = this->getActualSize();

        std::vector<u8> buffer(0x1000);

        const auto newSize = oldSize - size;
        auto position = offset;
        while (position < newSize) {
            const auto readSize = std::min<size_t>(newSize - position, buffer.size());

            this->readRaw(position + size, buffer.data(), readSize);
            this->writeRaw(position, buffer.data(), readSize);

            position += readSize;
        }

        this->resizeRaw(newSize);
    }



    void Provider::applyOverlays(u64 offset, void *buffer, size_t size) const {
        for (auto &overlay : m_overlays) {
            auto overlayOffset = overlay->getAddress();
            auto overlaySize   = overlay->getSize();

            i128 overlapMin = std::max(offset, overlayOffset);
            i128 overlapMax = std::min(offset + size, overlayOffset + overlaySize);
            if (overlapMax > overlapMin)
                std::memcpy(static_cast<u8 *>(buffer) + std::max<i128>(0, overlapMin - offset), overlay->getData().data() + std::max<i128>(0, overlapMin - overlayOffset), overlapMax - overlapMin);
        }
    }

    Overlay *Provider::newOverlay() {
        return m_overlays.emplace_back(std::make_unique<Overlay>()).get();
    }

    void Provider::deleteOverlay(Overlay *overlay) {
        m_overlays.remove_if([overlay](const auto &item) {
            return item.get() == overlay;
        });
    }

    const std::list<std::unique_ptr<Overlay>> &Provider::getOverlays() const {
        return m_overlays;
    }


    u64 Provider::getPageSize() const {
        return m_pageSize;
    }

    void Provider::setPageSize(u64 pageSize) {
        if (pageSize > MaxPageSize)
            pageSize = MaxPageSize;
        if (pageSize == 0)
            return;

        m_pageSize = pageSize;
    }

    u32 Provider::getPageCount() const {
        return (this->getActualSize() / this->getPageSize()) + (this->getActualSize() % this->getPageSize() != 0 ? 1 : 0);
    }

    u32 Provider::getCurrentPage() const {
        return m_currPage;
    }

    void Provider::setCurrentPage(u32 page) {
        if (page < getPageCount())
            m_currPage = page;
    }


    void Provider::setBaseAddress(u64 address) {
        m_baseAddress = address;
        this->markDirty();
    }

    u64 Provider::getBaseAddress() const {
        return m_baseAddress;
    }

    u64 Provider::getCurrentPageAddress() const {
        return this->getPageSize() * this->getCurrentPage();
    }

    u64 Provider::getSize() const {
        return std::min<u64>(this->getActualSize() - this->getPageSize() * m_currPage, this->getPageSize());
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
        m_undoRedoStack.undo();
    }

    void Provider::redo() {
        m_undoRedoStack.redo();
    }

    bool Provider::canUndo() const {
        return m_undoRedoStack.canUndo();
    }

    bool Provider::canRedo() const {
        return m_undoRedoStack.canRedo();
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

        settings["baseAddress"] = m_baseAddress;
        settings["currPage"]    = m_currPage;

        return settings;
    }

    void Provider::loadSettings(const nlohmann::json &settings) {
        m_baseAddress = settings["baseAddress"];
        m_currPage    = settings["currPage"];
    }

    std::pair<Region, bool> Provider::getRegionValidity(u64 address) const {
        u64 absoluteAddress = address - this->getBaseAddress();

        if (absoluteAddress < this->getActualSize())
            return { Region { this->getBaseAddress() + absoluteAddress, this->getActualSize() - absoluteAddress }, true };


        bool insideValidRegion = false;

        std::optional<u64> nextRegionAddress;
        for (const auto &overlay : m_overlays) {
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
        return m_id;
    }

    void Provider::setID(u32 id) {
        m_id = id;
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
