#pragma once

#include <hex/providers/provider.hpp>
#include <hex/helpers/fmt.hpp>

#include <hex/api/event_manager.hpp>

namespace hex::plugin::builtin {

    class ViewProvider : public hex::prv::Provider {
    public:
        explicit ViewProvider() {
            EventProviderClosing::subscribe(this, [this](const prv::Provider *provider, bool*) {
                if (m_provider == provider)
                    ImHexApi::Provider::remove(this, false);
            });
        }
        ~ViewProvider() override {
            EventProviderClosing::unsubscribe(this);
        }

        [[nodiscard]] bool isAvailable() const override {
            if (m_provider == nullptr)
                return false;
            else
                return m_provider->isAvailable();
        }
        [[nodiscard]] bool isReadable() const override {
            if (m_provider == nullptr)
                return false;
            else
                return m_provider->isReadable();
        }
        [[nodiscard]] bool isWritable() const override {
            if (m_provider == nullptr)
                return false;
            else
                return m_provider->isWritable();
        }
        [[nodiscard]] bool isResizable() const override { return true; }

        [[nodiscard]] bool isSavable() const override { 
            if (m_provider == nullptr)
                return false;
            else
                return m_provider->isSavable();
        }

        void save() override {
            m_provider->save();
        }

        [[nodiscard]] bool open() override { return true; }
        void close() override { }

        void resizeRaw(u64 newSize) override {
            m_size = newSize;
        }
        void insertRaw(u64 offset, u64 size) override {
            if (m_provider == nullptr)
                return;

            m_size += size;
            m_provider->insert(offset + m_startAddress, size);
        }

        void removeRaw(u64 offset, u64 size) override {
            if (m_provider == nullptr)
                return;

            m_size -= size;
            m_provider->remove(offset + m_startAddress, size);
        }

        void readRaw(u64 offset, void *buffer, size_t size) override {
            if (m_provider == nullptr)
                return;

            m_provider->read(offset + m_startAddress, buffer, size);
        }

        void writeRaw(u64 offset, const void *buffer, size_t size) override {
            if (m_provider == nullptr)
                return;

            m_provider->write(offset + m_startAddress, buffer, size);
        }

        [[nodiscard]] u64 getActualSize() const override { return m_size; }

        [[nodiscard]] std::string getName() const override {
            if (m_provider == nullptr)
                return "View";
            else
                return hex::format("{} View", m_provider->getName());
        }
        [[nodiscard]] std::vector<Description> getDataDescription() const override {
            if (m_provider == nullptr)
                return { };

            return m_provider->getDataDescription();
        }

        void loadSettings(const nlohmann::json &settings) override { hex::unused(settings); }
        [[nodiscard]] nlohmann::json storeSettings(nlohmann::json settings) const override { return settings; }

        [[nodiscard]] std::string getTypeName() const override {
            return "hex.builtin.provider.view";
        }

        void setProvider(u64 startAddress, size_t size, hex::prv::Provider *provider) {
            m_startAddress = startAddress;
            m_size = size;
            m_provider = provider;
        }

        [[nodiscard]] std::pair<Region, bool> getRegionValidity(u64 address) const override {
            address -= this->getBaseAddress();

            if (address < this->getActualSize())
                return { Region { this->getBaseAddress() + address, this->getActualSize() - address }, true };
            else
                return { Region::Invalid(), false };
        }

    private:
        u64 m_startAddress = 0x00;
        size_t m_size = 0x00;
        prv::Provider *m_provider = nullptr;
    };

}