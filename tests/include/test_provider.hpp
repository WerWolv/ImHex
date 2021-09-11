#include <hex/providers/provider.hpp>

#include <hex/helpers/file.hpp>
#include <hex/helpers/logger.hpp>
#include <stdexcept>

namespace hex::test {
    using namespace hex::prv;

    class TestProvider : public prv::Provider {
    public:
        TestProvider() : Provider() {
            this->m_testFile = File("test_data", File::Mode::Read);
            if (!this->m_testFile.isValid()) {
                hex::log::fatal("Failed to open test data!");
                throw std::runtime_error("");
            }
        }
        ~TestProvider() override = default;

        bool isAvailable() override { return true; }
        bool isReadable() override { return true; }
        bool isWritable() override { return false; }
        bool isResizable() override { return false; }
        bool isSavable() override { return false; }

        std::vector<std::pair<std::string, std::string>> getDataInformation() override {
            return { };
        }

        void readRaw(u64 offset, void *buffer, size_t size) override {
            this->m_testFile.seek(offset);
            this->m_testFile.readBuffer(static_cast<u8*>(buffer), size);
        }

        void writeRaw(u64 offset, const void *buffer, size_t size) override {
            this->m_testFile.seek(offset);
            this->m_testFile.write(static_cast<const u8*>(buffer), size);
        }

        size_t getActualSize() override {
            return m_testFile.getSize();
        }

    private:
        File m_testFile;
    };

}