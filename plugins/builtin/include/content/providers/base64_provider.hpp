#pragma once

#include <content/providers/file_provider.hpp>

namespace hex::plugin::builtin {

    class Base64Provider : public FileProvider {
    public:
        explicit Base64Provider() = default;
        ~Base64Provider() override = default;

        void readRaw(u64 offset, void *buffer, size_t size) override;
        void writeRaw(u64 offset, const void *buffer, size_t size) override;
        [[nodiscard]] u64 getActualSize() const override { return (3 * m_file.getSize()) / 4; }

        void resizeRaw(u64 newSize) override;
        void insertRaw(u64 offset, u64 size) override;
        void removeRaw(u64 offset, u64 size) override;

        [[nodiscard]] UnlocalizedString getTypeName() const override {
            return "hex.builtin.provider.base64";
        }
    };

}