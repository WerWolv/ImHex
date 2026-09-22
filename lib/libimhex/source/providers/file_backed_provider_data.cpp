#include <hex/providers/file_backed_provider_data.hpp>

#include <map>
#include <mutex>
#include <stdexcept>
#include <algorithm>
#include <cerrno>
#include <limits>

#if defined(OS_WINDOWS)
    #include <windows.h>
#else
    #include <fcntl.h>
    #include <unistd.h>
#endif

namespace hex {

    bool impl::writeFileAtomically(const std::fs::path &path, std::span<const u8> data) {
        static std::atomic<u64> counter = 0;
        std::fs::path temporaryPath;

        #if defined(OS_WINDOWS)
            HANDLE handle = INVALID_HANDLE_VALUE;
            do {
                temporaryPath = path;
                temporaryPath += L".imhex-" + std::to_wstring(++counter);
                handle = CreateFileW(temporaryPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
            } while (handle == INVALID_HANDLE_VALUE && GetLastError() == ERROR_FILE_EXISTS);
            if (handle == INVALID_HANDLE_VALUE)
                return false;

            size_t offset = 0;
            bool result = true;
            while (offset < data.size()) {
                DWORD written = 0;
                const auto chunkSize = static_cast<DWORD>(std::min<size_t>(data.size() - offset, std::numeric_limits<DWORD>::max()));
                if (!WriteFile(handle, data.data() + offset, chunkSize, &written, nullptr) || written == 0) {
                    result = false;
                    break;
                }
                offset += written;
            }
            result = result && FlushFileBuffers(handle);
            CloseHandle(handle);
            if (result)
                result = MoveFileExW(temporaryPath.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
        #else
            int handle = -1;
            do {
                temporaryPath = path;
                temporaryPath += ".imhex-" + std::to_string(++counter);
                handle = ::open(temporaryPath.c_str(), O_CREAT | O_EXCL | O_WRONLY, 0666);
            } while (handle < 0 && errno == EEXIST);
            if (handle < 0)
                return false;

            size_t offset = 0;
            bool result = true;
            while (offset < data.size()) {
                const auto written = ::write(handle, data.data() + offset, data.size() - offset);
                if (written <= 0) {
                    result = false;
                    break;
                }
                offset += written;
            }
            result = result && ::fsync(handle) == 0;
            result = ::close(handle) == 0 && result;
            if (result) {
                std::error_code error;
                std::fs::rename(temporaryPath, path, error);
                result = !error;
            }
        #endif

        if (!result) {
            std::error_code error;
            std::fs::remove(temporaryPath, error);
        }
        return result;
    }

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
