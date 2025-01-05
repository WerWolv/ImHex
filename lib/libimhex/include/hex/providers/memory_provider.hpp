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
        explicit MemoryProvider(std::vector<u8> data, std::string name = "") : m_data(std::move(data)), m_name(std::move(name)) { }
        ~MemoryProvider() override = default;

        MemoryProvider(const MemoryProvider&) = delete;
        MemoryProvider& operator=(const MemoryProvider&) = delete;

        MemoryProvider(MemoryProvider &&provider) noexcept = default;
        MemoryProvider& operator=(MemoryProvider &&provider) noexcept = default;

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

        [[nodiscard]] std::string getName() const override { return m_name; }

        [[nodiscard]] UnlocalizedString getTypeName() const override { return "MemoryProvider"; }
    private:
        void renameFile();

    private:
        std::vector<u8> m_data;
        std::string m_name;
    };

}