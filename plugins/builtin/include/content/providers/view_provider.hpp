#pragma once

#include <hex/providers/provider.hpp>
#include <hex/helpers/fmt.hpp>

namespace hex::plugin::builtin {

    class ViewProvider : public hex::prv::Provider {
    public:
        explicit ViewProvider() = default;
        ~ViewProvider() override = default;

        [[nodiscard]] bool isAvailable() const override { return this->m_provider != nullptr; }
        [[nodiscard]] bool isReadable()  const override { return this->m_provider->isReadable(); }
        [[nodiscard]] bool isWritable()  const override { return this->m_provider->isWritable(); }
        [[nodiscard]] bool isResizable() const override { return true; }
        [[nodiscard]] bool isSavable()   const override { return true; }

        void save() override {
            this->m_provider->save();
        }

        [[nodiscard]] bool open() override { return true; }
        void close() override { }

        void resize(size_t newSize) override {
            this->m_size = newSize;
        }
        void insert(u64 offset, size_t size) override {
            this->m_size += size;
            this->m_provider->insert(offset + this->m_startAddress, size);
        }

        void remove(u64 offset, size_t size) override {
            this->m_size -= size;
            this->m_provider->remove(offset + this->m_startAddress, size);
        }

        void readRaw(u64 offset, void *buffer, size_t size) override { this->m_provider->read(offset + this->m_startAddress, buffer, size); }
        void writeRaw(u64 offset, const void *buffer, size_t size) override { this->m_provider->write(offset + this->m_startAddress, buffer, size); }
        [[nodiscard]] size_t getActualSize() const override { return this->m_size; }

        [[nodiscard]] std::string getName() const override { return hex::format("{} View", this->m_provider->getName()); }
        [[nodiscard]] std::vector<std::pair<std::string, std::string>> getDataInformation() const override { return this->m_provider->getDataInformation(); }

        void loadSettings(const nlohmann::json &settings) override { hex::unused(settings); }
        [[nodiscard]] nlohmann::json storeSettings(nlohmann::json settings) const override { return settings; }

        [[nodiscard]] std::string getTypeName() const override {
            return "hex.builtin.provider.view";
        }

        void setProvider(u64 startAddress, size_t size, hex::prv::Provider *provider) {
            this->m_startAddress = startAddress;
            this->m_size = size;
            this->m_provider = provider;
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