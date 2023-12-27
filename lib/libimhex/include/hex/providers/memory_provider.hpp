#pragma once

#include <hex/providers/provider.hpp>

namespace hex::prv {

    /**
     * This is a simple mock provider that can be used to pass in-memory data to APIs that require a provider.
     * It's NOT a provider that can be loaded by the user.
     */
    class MemoryProvider : public hex::prv::Provider {
    public:
        MemoryProvider() = default;
        explicit MemoryProvider(std::vector<u8> data) : m_data(std::move(data)) { }
        ~MemoryProvider() override = default;

        [[nodiscard]] bool isAvailable()        const override { return true;           }
        [[nodiscard]] bool isReadable()         const override { return true;           }
        [[nodiscard]] bool isWritable()         const override { return true;           }
        [[nodiscard]] bool isResizable()        const override { return true;           }
        [[nodiscard]] bool isSavable()          const override { return m_name.empty(); }
        [[nodiscard]] bool isSavableAsRecent()  const override { return false;          }

        [[nodiscard]] bool open() override;
        void close() override { }

        void readRaw(u64 offset, void *buffer, size_t size) override;
        void writeRaw(u64 offset, const void *buffer, size_t size) override;
        [[nodiscard]] u64 getActualSize() const override { return m_data.size(); }

        void resizeRaw(u64 newSize) override;
        void insertRaw(u64 offset, u64 size) override;
        void removeRaw(u64 offset, u64 size) override;

        [[nodiscard]] std::string getName() const override { return ""; }

        [[nodiscard]] std::string getTypeName() const override { return "MemoryProvider"; }

        std::pair<Region, bool> getRegionValidity(u64 address) const {
            address -= this->getBaseAddress();

            if (address < this->getActualSize())
                return { Region { this->getBaseAddress() + address, this->getActualSize() - address }, true };
            else
                return { Region::Invalid(), false };
        }
    private:
        void renameFile();

    private:
        std::vector<u8> m_data;
        std::string m_name;
    };

}