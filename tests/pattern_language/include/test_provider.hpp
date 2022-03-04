#include <hex/providers/provider.hpp>

#include <hex/helpers/file.hpp>
#include <hex/helpers/logger.hpp>
#include <stdexcept>

namespace hex::test {
    using namespace hex::prv;

    class TestProvider : public prv::Provider {
    public:
        TestProvider() : Provider(), m_testFile(fs::File("test_data", fs::File::Mode::Read)) {
            if (!this->m_testFile.isValid() || this->m_testFile.getSize() == 0) {
                hex::log::fatal("Failed to open test data!");
                throw std::runtime_error("");
            }
        }
        ~TestProvider() override = default;

        [[nodiscard]] bool isAvailable() const override { return true; }
        [[nodiscard]] bool isReadable() const override { return true; }
        [[nodiscard]] bool isWritable() const override { return false; }
        [[nodiscard]] bool isResizable() const override { return false; }
        [[nodiscard]] bool isSavable() const override { return false; }

        [[nodiscard]] std::string getName() const override {
            return "";
        }

        [[nodiscard]] std::vector<std::pair<std::string, std::string>> getDataInformation() const override {
            return {};
        }

        void readRaw(u64 offset, void *buffer, size_t size) override {
            this->m_testFile.seek(offset);
            this->m_testFile.readBuffer(static_cast<u8 *>(buffer), size);
        }

        void writeRaw(u64 offset, const void *buffer, size_t size) override {
            this->m_testFile.seek(offset);
            this->m_testFile.write(static_cast<const u8 *>(buffer), size);
        }

        size_t getActualSize() const override {
            return this->m_testFile.getSize();
        }

        bool open() override { return true; }
        void close() override { }

    private:
        fs::File m_testFile;
    };

}