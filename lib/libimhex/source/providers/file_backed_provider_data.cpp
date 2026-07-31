#include <hex/providers/file_backed_provider_data.hpp>

#include <map>
#include <mutex>
#include <stdexcept>

namespace hex {

    namespace {

        using Registry = std::map<std::string, FileBackedProviderDataBase *, std::less<>>;

        Registry &getRegistry() {
            static Registry registry;
            return registry;
        }

        std::mutex &getRegistryMutex() {
            static std::mutex mutex;
            return mutex;
        }

    }

    FileBackedProviderDataBase::FileBackedProviderDataBase(FileBackedProviderDataType type)
        : m_type(std::move(type)) {
        if (m_type.typeId.empty())
            throw std::invalid_argument("FileBackedProviderData type ID cannot be empty");

        FileBackedProviderDataRegistry::add(this);
    }

    FileBackedProviderDataBase::~FileBackedProviderDataBase() {
        FileBackedProviderDataRegistry::remove(this);
    }

    std::vector<FileBackedProviderDataBase *> FileBackedProviderDataRegistry::getTypes() {
        std::lock_guard lock(getRegistryMutex());
        std::vector<FileBackedProviderDataBase *> result;
        result.reserve(getRegistry().size());
        for (const auto &[typeId, data] : getRegistry()) {
            std::ignore = typeId;
            result.push_back(data);
        }
        return result;
    }

    FileBackedProviderDataBase *FileBackedProviderDataRegistry::get(std::string_view typeId) {
        std::lock_guard lock(getRegistryMutex());
        const auto it = getRegistry().find(typeId);
        return it == getRegistry().end() ? nullptr : it->second;
    }

    bool FileBackedProviderDataRegistry::bind(prv::Provider *provider, std::string_view typeId, const std::fs::path &path) {
        auto *data = get(typeId);
        return data != nullptr && provider != nullptr && data->bind(provider, path);
    }

    bool FileBackedProviderDataRegistry::createFile(std::string_view typeId, const std::fs::path &path) {
        auto *data = get(typeId);
        return data != nullptr && data->createFile(path);
    }

    bool FileBackedProviderDataRegistry::relocate(prv::Provider *provider, std::string_view typeId, const std::fs::path &path) {
        auto *data = get(typeId);
        return data != nullptr && provider != nullptr && data->relocate(provider, path);
    }

    bool FileBackedProviderDataRegistry::unbind(prv::Provider *provider, std::string_view typeId) {
        auto *data = get(typeId);
        if (data == nullptr || provider == nullptr)
            return false;

        data->unbind(provider);
        return true;
    }

    bool FileBackedProviderDataRegistry::isBound(const prv::Provider *provider, std::string_view typeId) {
        auto *data = get(typeId);
        return data != nullptr && provider != nullptr && data->isBound(provider);
    }

    std::optional<std::fs::path> FileBackedProviderDataRegistry::getBinding(const prv::Provider *provider, std::string_view typeId) {
        auto *data = get(typeId);
        if (data == nullptr || provider == nullptr)
            return std::nullopt;

        return data->getBinding(provider);
    }

    void FileBackedProviderDataRegistry::add(FileBackedProviderDataBase *data) {
        std::lock_guard lock(getRegistryMutex());
        const auto &[it, inserted] = getRegistry().emplace(data->getType().typeId, data);
        if (!inserted)
            throw std::invalid_argument("FileBackedProviderData type ID is already registered: " + it->first);
    }

    void FileBackedProviderDataRegistry::remove(FileBackedProviderDataBase *data) noexcept {
        std::lock_guard lock(getRegistryMutex());
        const auto it = getRegistry().find(data->getType().typeId);
        if (it != getRegistry().end() && it->second == data)
            getRegistry().erase(it);
    }

}
