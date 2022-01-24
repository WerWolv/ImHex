#include <hex/providers/provider.hpp>

#include <hex/helpers/file.hpp>
#include <hex/helpers/logger.hpp>
#include <stdexcept>

namespace hex::test {
    using namespace hex::prv;

    class TestProvider : public prv::Provider {
    public:
        TestProvider(std::vector<u8> *data) : Provider() {
            this->setData(data);
        }
        ~TestProvider() override = default;

        [[nodiscard]] bool isAvailable() const override { return true; }
        [[nodiscard]] bool isReadable() const override { return true; }
        [[nodiscard]] bool isWritable() const override { return false; }
        [[nodiscard]] bool isResizable() const override { return false; }
        [[nodiscard]] bool isSavable() const override { return false; }

        void setData(std::vector<u8> *data) {
            this->m_data = data;
        }

        [[nodiscard]] std::string getName() const override {
            return "";
        }

        [[nodiscard]] std::vector<std::pair<std::string, std::string>> getDataInformation() const override {
            return {};
        }

        void readRaw(u64 offset, void *buffer, size_t size) override {
            if (offset + size > this->m_data->size()) return;

            std::memcpy(buffer, m_data->data() + offset, size);
        }

        void writeRaw(u64 offset, const void *buffer, size_t size) override {
            if (offset + size > this->m_data->size()) return;

            std::memcpy(m_data->data() + offset, buffer, size);
        }

        size_t getActualSize() const override {
            return this->m_data->size();
        }

        bool open() override { return true; }
        void close() override { }

    private:
        std::vector<u8> *m_data;
    };

}
