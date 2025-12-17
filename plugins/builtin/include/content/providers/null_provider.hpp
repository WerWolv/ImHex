#pragma once

#include <hex/api/imhex_api/provider.hpp>
#include <hex/providers/provider.hpp>
#include <hex/api/events/events_provider.hpp>

#include <nlohmann/json.hpp>

namespace hex::plugin::builtin {

    class NullProvider : public hex::prv::Provider {
    public:
        NullProvider() {
            EventProviderOpened::subscribe([this](auto *newProvider) {
                if (newProvider == this)
                    return;

                ImHexApi::Provider::remove(this, true);
            });
        }

        ~NullProvider() override {
            EventProviderOpened::unsubscribe(this);
        }

        [[nodiscard]] bool isAvailable() const override { return true; }
        [[nodiscard]] bool isReadable() const override { return true; }
        [[nodiscard]] bool isWritable() const override { return false; }
        [[nodiscard]] bool isResizable() const override { return false; }
        [[nodiscard]] bool isSavable() const override { return false; }

        [[nodiscard]] OpenResult open() override { return {}; }
        void close() override { }

        void readRaw(u64 offset, void *buffer, size_t size) override {
            std::ignore = offset;
            std::ignore = buffer;
            std::ignore = size;
        }
        void writeRaw(u64 offset, const void *buffer, size_t size) override {
            std::ignore = offset;
            std::ignore = buffer;
            std::ignore = size;
        }
        [[nodiscard]] u64 getActualSize() const override { return 0x00; }

        [[nodiscard]] std::string getName() const override { return "ImHex"; }

        [[nodiscard]] const char* getIcon() const override {
            return "";
        }

        void loadSettings(const nlohmann::json &settings) override { std::ignore = settings; }
        [[nodiscard]] nlohmann::json storeSettings(nlohmann::json settings) const override { return settings; }

        [[nodiscard]] UnlocalizedString getTypeName() const override {
            return "hex.builtin.provider.null";
        }
    };

}