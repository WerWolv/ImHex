#include <script_api.hpp>
#include <hex/api/content_registry.hpp>

#include <hex/api/imhex_api.hpp>
#include <hex/providers/provider.hpp>

#define VERSION V1

SCRIPT_API(void readMemory, u64 address, size_t size, void *buffer) {
    auto provider = hex::ImHexApi::Provider::get();

    if (provider == nullptr) {
        return;
    }

    provider->read(address, buffer, size);
}

SCRIPT_API(void writeMemory, u64 address, size_t size, const void *buffer) {
    auto provider = hex::ImHexApi::Provider::get();

    if (provider == nullptr) {
        return;
    }

    provider->write(address, buffer, size);
}

SCRIPT_API(u64 getBaseAddress) {
    return hex::ImHexApi::Provider::get()->getBaseAddress();
}

SCRIPT_API(u64 getDataSize) {
    return hex::ImHexApi::Provider::get()->getSize();
}

SCRIPT_API(bool getSelection, u64 *start, u64 *end) {
    if (start == nullptr || end == nullptr)
        return false;

    if (!hex::ImHexApi::Provider::isValid())
        return false;

    if (!hex::ImHexApi::HexEditor::isSelectionValid())
        return false;

    auto selection = hex::ImHexApi::HexEditor::getSelection();

    *start = selection->getStartAddress();
    *end = selection->getEndAddress();

    return true;
}

class ScriptDataProvider : public hex::prv::Provider {
public:
    using ReadFunction = void(*)(u64, void*, u64);
    using WriteFunction = void(*)(u64, const void*, u64);
    using GetSizeFunction = u64(*)();
    using GetNameFunction = std::string(*)();

    bool open() override { return true; }
    void close() override { }

    [[nodiscard]] bool isAvailable() const override { return true; }
    [[nodiscard]] bool isReadable()  const override { return true; }
    [[nodiscard]] bool isWritable()  const override { return true; }
    [[nodiscard]] bool isResizable() const override { return true; }
    [[nodiscard]] bool isSavable()   const override { return true; }
    [[nodiscard]] bool isDumpable()  const override { return true; }

    void readRaw(u64 offset, void *buffer, size_t size) override {
        m_readFunction(offset, buffer, size);
    }

    void writeRaw(u64 offset, const void *buffer, size_t size) override {
        m_writeFunction(offset, const_cast<void*>(buffer), size);
    }

    void setFunctions(ReadFunction readFunc, WriteFunction writeFunc, GetSizeFunction getSizeFunc) {
        m_readFunction    = readFunc;
        m_writeFunction   = writeFunc;
        m_getSizeFunction = getSizeFunc;
    }

    [[nodiscard]] u64 getActualSize() const override { return m_getSizeFunction(); }

    void setTypeName(std::string typeName) { m_typeName = std::move(typeName);}
    void setName(std::string name) { m_name = std::move(name);}
    [[nodiscard]] hex::UnlocalizedString getTypeName() const override { return m_typeName; }
    [[nodiscard]] std::string getName() const override { return m_name; }

private:
    ReadFunction m_readFunction = nullptr;
    WriteFunction m_writeFunction = nullptr;
    GetSizeFunction m_getSizeFunction = nullptr;

    std::string m_typeName, m_name;
};

SCRIPT_API(void registerProvider, const char *typeName, const char *name, ScriptDataProvider::ReadFunction readFunc, ScriptDataProvider::WriteFunction writeFunc, ScriptDataProvider::GetSizeFunction getSizeFunc) {
    auto typeNameString = std::string(typeName);
    auto nameString = std::string(name);

    hex::ContentRegistry::Provider::impl::add(typeNameString, [typeNameString, nameString, readFunc, writeFunc, getSizeFunc] -> std::unique_ptr<hex::prv::Provider> {
        auto provider = std::make_unique<ScriptDataProvider>();
        provider->setTypeName(typeNameString);
        provider->setName(nameString);
        provider->setFunctions(readFunc, writeFunc, getSizeFunc);
        return provider;
    });
    hex::ContentRegistry::Provider::impl::addProviderName(typeNameString);
}