#pragma once

#include <hex/providers/provider.hpp>
#include <nlohmann/json.hpp>

namespace hex::test {
    using namespace hex::prv;

    class TestProvider : public prv::Provider {
    public:
        explicit TestProvider(std::vector<u8> *data) {
            this->setData(data);
        }
        ~TestProvider() override = default;

        [[nodiscard]] bool isAvailable() const override { return true; }
        [[nodiscard]] bool isReadable() const override { return true; }
        [[nodiscard]] bool isWritable() const override { return false; }
        [[nodiscard]] bool isResizable() const override { return false; }
        [[nodiscard]] bool isSavable() const override { return false; }

        void setData(std::vector<u8> *data) {
            m_data = data;
        }

        [[nodiscard]] std::string getName() const override {
            return "";
        }

        [[nodiscard]] std::vector<Description> getDataDescription() const override {
            return {};
        }

        void readRaw(u64 offset, void *buffer, size_t size) override {
            if (offset + size > m_data->size()) return;

            std::memcpy(buffer, m_data->data() + offset, size);
        }

        void writeRaw(u64 offset, const void *buffer, size_t size) override {
            if (offset + size > m_data->size()) return;

            std::memcpy(m_data->data() + offset, buffer, size);
        }

        [[nodiscard]] u64 getActualSize() const override {
            return m_data->size();
        }

        [[nodiscard]] std::string getTypeName() const override { return "hex.test.provider.test"; }

        bool open() override { return true; }
        void close() override { }

        nlohmann::json storeSettings(nlohmann::json) const override { return {}; }
        void loadSettings(const nlohmann::json &) override {};

    private:
        std::vector<u8> *m_data = nullptr;
    };

}
