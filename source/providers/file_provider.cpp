#include "providers/file_provider.hpp"

#include <ctime>
#include <cstring>

#include <hex/helpers/utils.hpp>
#include <hex/helpers/file.hpp>
#include "helpers/project_file_handler.hpp"

namespace hex::prv {

    FileProvider::FileProvider(std::string path) : Provider(), m_path(std::move(path)) {
        this->open();
    }

    FileProvider::~FileProvider() {
        this->close();
    }


    bool FileProvider::isAvailable() const {
        #if defined(OS_WINDOWS)
        return this->m_file != nullptr && this->m_mapping != nullptr && this->m_mappedFile != nullptr;
        #else
        return this->m_file != -1 && this->m_mappedFile != nullptr;
        #endif
    }

    bool FileProvider::isReadable() const {
        return isAvailable() && this->m_readable;
    }

    bool FileProvider::isWritable() const {
        return isAvailable() && this->m_writable;
    }

    bool FileProvider::isResizable() const {
        return true;
    }

    bool FileProvider::isSavable() const {
        return !this->getPatches().empty();
    }


    void FileProvider::read(u64 offset, void *buffer, size_t size, bool overlays) {

        if ((offset - this->getBaseAddress()) > (this->getSize() - size) || buffer == nullptr || size == 0)
            return;

        std::memcpy(buffer, reinterpret_cast<u8*>(this->m_mappedFile) + PageSize * this->m_currPage + offset - this->getBaseAddress(), size);

        for (u64 i = 0; i < size; i++)
            if (getPatches().contains(offset + i))
                reinterpret_cast<u8*>(buffer)[i] = getPatches()[offset + PageSize * this->m_currPage + i];

        if (overlays)
            this->applyOverlays(offset, buffer, size);
    }

    void FileProvider::write(u64 offset, const void *buffer, size_t size) {
        if (((offset - this->getBaseAddress()) + size) > this->getSize() || buffer == nullptr || size == 0)
            return;

        addPatch(offset, buffer, size);
    }

    void FileProvider::readRaw(u64 offset, void *buffer, size_t size) {
        offset -= this->getBaseAddress();

        if ((offset + size) > this->getSize() || buffer == nullptr || size == 0)
            return;

        std::memcpy(buffer, reinterpret_cast<u8*>(this->m_mappedFile) + PageSize * this->m_currPage + offset, size);
    }

    void FileProvider::writeRaw(u64 offset, const void *buffer, size_t size) {
        offset -= this->getBaseAddress();

        if ((offset + size) > this->getSize() || buffer == nullptr || size == 0)
            return;

        std::memcpy(reinterpret_cast<u8*>(this->m_mappedFile) + PageSize * this->m_currPage + offset, buffer, size);
    }

    void FileProvider::save() {
        this->applyPatches();
    }

    void FileProvider::saveAs(const std::string &path) {
        File file(path, File::Mode::Create);

        if (file.isValid()) {
            auto provider = ImHexApi::Provider::get();

            std::vector<u8> buffer(std::min<size_t>(0xFF'FFFF, provider->getActualSize()), 0x00);
            size_t bufferSize = buffer.size();

            for (u64 offset = 0; offset < provider->getActualSize(); offset += bufferSize) {
                if (bufferSize > provider->getActualSize() - offset)
                    bufferSize = provider->getActualSize() - offset;

                provider->readRelative(offset, buffer.data(), bufferSize);
                file.write(buffer);
            }
        }
    }

    void FileProvider::resize(ssize_t newSize) {
        this->close();

    #if defined(OS_WINDOWS)
        std::wstring widePath;
        {
            auto length = static_cast<int>(this->m_path.length() + 1);
            auto wideLength = MultiByteToWideChar(CP_UTF8, 0, this->m_path.data(), length, nullptr, 0);
            auto buffer = new wchar_t[wideLength];
            MultiByteToWideChar(CP_UTF8, 0, this->m_path.data(), length, buffer, wideLength);
            widePath = buffer;
            delete[] buffer;
        }

        auto handle = ::CreateFileW(widePath.data(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

        if (handle != INVALID_HANDLE_VALUE) {
            ::SetFilePointerEx(handle, LARGE_INTEGER { .QuadPart = newSize }, nullptr, FILE_BEGIN);
            ::SetEndOfFile(handle);
            ::CloseHandle(handle);
        }
    #else
        auto handle = ::open(this->m_path.data(), 0644);

        ::ftruncate(handle, newSize - 1);

        ::close(handle);
    #endif

        this->open();
    }

    size_t FileProvider::getActualSize() const {
        return this->m_fileSize;
    }

    std::string FileProvider::getName() const {
        return std::filesystem::path(this->m_path).filename().string();
    }

    std::vector<std::pair<std::string, std::string>> FileProvider::getDataInformation() const {
        std::vector<std::pair<std::string, std::string>> result;

        result.emplace_back("hex.builtin.provider.file.path"_lang, this->m_path);
        result.emplace_back("hex.builtin.provider.file.size"_lang, hex::toByteString(this->getActualSize()));

        if (this->m_fileStatsValid) {
            result.emplace_back("hex.builtin.provider.file.creation"_lang, ctime(&this->m_fileStats.st_ctime));
            result.emplace_back("hex.builtin.provider.file.access"_lang, ctime(&this->m_fileStats.st_atime));
            result.emplace_back("hex.builtin.provider.file.modification"_lang, ctime(&this->m_fileStats.st_mtime));
        }

        return result;
    }

    void FileProvider::open() {
        this->m_fileStatsValid = stat(this->m_path.data(), &this->m_fileStats) == 0;

        this->m_readable = true;
        this->m_writable = true;

        #if defined(OS_WINDOWS)
        std::wstring widePath;
        {
            auto length = this->m_path.length() + 1;
            auto wideLength = MultiByteToWideChar(CP_UTF8, 0, this->m_path.data(), length, 0, 0);
            auto buffer = new wchar_t[wideLength];
            MultiByteToWideChar(CP_UTF8, 0, this->m_path.data(), length, buffer, wideLength);
            widePath = buffer;
            delete[] buffer;
        }

        LARGE_INTEGER fileSize = { 0 };
        this->m_file = reinterpret_cast<HANDLE>(CreateFileW(widePath.data(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));

        GetFileSizeEx(this->m_file, &fileSize);
        this->m_fileSize = fileSize.QuadPart;
        CloseHandle(this->m_file);

        this->m_file = reinterpret_cast<HANDLE>(CreateFileW(widePath.data(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
        if (this->m_file == nullptr || this->m_file == INVALID_HANDLE_VALUE) {
            this->m_file = reinterpret_cast<HANDLE>(CreateFileW(widePath.data(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
            this->m_writable = false;
        }

        auto fileCleanup = SCOPE_GUARD {
            this->m_readable = false;
            this->m_file = nullptr;
            CloseHandle(this->m_file);
        };

        if (this->m_file == nullptr || this->m_file == INVALID_HANDLE_VALUE) {
            return;
        }

        this->m_mapping = CreateFileMapping(this->m_file, nullptr, PAGE_READWRITE, fileSize.HighPart, fileSize.LowPart, nullptr);
        if (this->m_mapping == nullptr || this->m_mapping == INVALID_HANDLE_VALUE) {
            return;
        }

        auto mappingCleanup = SCOPE_GUARD {
            this->m_readable = false;
            this->m_mapping = nullptr;
            CloseHandle(this->m_mapping);
        };

        this->m_mappedFile = MapViewOfFile(this->m_mapping, FILE_MAP_ALL_ACCESS, 0, 0, this->m_fileSize);
        if (this->m_mappedFile == nullptr) {
            this->m_readable = false;
            return;
        }

        fileCleanup.release();
        mappingCleanup.release();

        ProjectFile::setFilePath(this->m_path);

        #else
        this->m_file = ::open(this->m_path.data(), O_RDWR);
            if (this->m_file == -1) {
                this->m_file = ::open(this->m_path.data(), O_RDONLY);
                this->m_writable = false;
            }

            if (this->m_file == -1) {
                this->m_readable = false;
                return;
            }

            this->m_fileSize = this->m_fileStats.st_size;

            this->m_mappedFile = mmap(nullptr, this->m_fileSize, PROT_READ | PROT_WRITE, MAP_PRIVATE, this->m_file, 0);

    #endif
    }

    void FileProvider::close() {
    #if defined(OS_WINDOWS)
        if (this->m_mappedFile != nullptr)
            ::UnmapViewOfFile(this->m_mappedFile);
        if (this->m_mapping != nullptr)
            ::CloseHandle(this->m_mapping);
        if (this->m_file != nullptr)
            ::CloseHandle(this->m_file);
    #else
        ::munmap(this->m_mappedFile, this->m_fileSize);
        ::close(this->m_file);
    #endif
    }

}