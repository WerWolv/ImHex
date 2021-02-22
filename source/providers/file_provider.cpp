#include "providers/file_provider.hpp"

#include <time.h>
#include <cstring>

#include "helpers/project_file_handler.hpp"

#if defined(OS_WINDOWS)
#include <locale>
#include <codecvt>
#endif

namespace hex::prv {

    FileProvider::FileProvider(std::string_view path) : Provider(), m_path(path) {
        this->m_fileStatsValid = stat(path.data(), &this->m_fileStats) == 0;

        this->m_readable = true;
        this->m_writable = true;

        #if defined(OS_WINDOWS)
        std::wstring widePath;
        {
            auto length = path.length() + 1;
            auto wideLength = MultiByteToWideChar(CP_UTF8, 0, path.data(), length, 0, 0);
            wchar_t* buffer = new wchar_t[wideLength];
            MultiByteToWideChar(CP_UTF8, 0, path.data(), length, buffer, wideLength);
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

        ScopeExit fileCleanup([this]{
            this->m_readable = false;
            this->m_file = nullptr;
            CloseHandle(this->m_file);
        });
        if (this->m_file == nullptr || this->m_file == INVALID_HANDLE_VALUE) {
            return;
        }

        this->m_mapping = CreateFileMapping(this->m_file, nullptr, PAGE_READWRITE, fileSize.HighPart, fileSize.LowPart, nullptr);
        if (this->m_mapping == nullptr || this->m_mapping == INVALID_HANDLE_VALUE) {
            return;
        }

        ScopeExit mappingCleanup([this]{
            this->m_readable = false;
            this->m_mapping = nullptr;
            CloseHandle(this->m_mapping);
        });

        this->m_mappedFile = MapViewOfFile(this->m_mapping, FILE_MAP_ALL_ACCESS, 0, 0, this->m_fileSize);
        if (this->m_mappedFile == nullptr) {
            this->m_readable = false;
            return;
        }

        fileCleanup.release();
        mappingCleanup.release();

        ProjectFile::setFilePath(path);

        #else
            this->m_file = open(path.data(), O_RDWR);
            if (this->m_file == -1) {
                this->m_file = open(path.data(), O_RDONLY);
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

    FileProvider::~FileProvider() {
        #if defined(OS_WINDOWS)
        if (this->m_mappedFile != nullptr)
            UnmapViewOfFile(this->m_mappedFile);
        if (this->m_mapping != nullptr)
            CloseHandle(this->m_mapping);
        if (this->m_file != nullptr)
            CloseHandle(this->m_file);
        #else
        munmap(this->m_mappedFile, this->m_fileSize);
        close(this->m_file);
        #endif
    }


    bool FileProvider::isAvailable() {
        #if defined(OS_WINDOWS)
        return this->m_file != nullptr && this->m_mapping != nullptr && this->m_mappedFile != nullptr;
        #else
        return this->m_file != -1 && this->m_mappedFile != nullptr;
        #endif
    }

    bool FileProvider::isReadable() {
        return isAvailable() && this->m_readable;
    }

    bool FileProvider::isWritable() {
        return isAvailable() && this->m_writable;
    }


    void FileProvider::read(u64 offset, void *buffer, size_t size) {
        if ((offset + size) > this->getSize() || buffer == nullptr || size == 0)
            return;

        std::memcpy(buffer, reinterpret_cast<u8*>(this->m_mappedFile) + PageSize * this->m_currPage + offset, size);

        for (u64 i = 0; i < size; i++)
            if (this->m_patches.back().contains(offset + i))
                reinterpret_cast<u8*>(buffer)[i] = this->m_patches.back()[offset + PageSize * this->m_currPage + i];
    }

    void FileProvider::write(u64 offset, const void *buffer, size_t size) {
        if ((offset + size) > this->getSize() || buffer == nullptr || size == 0)
            return;

        this->m_patches.push_back(this->m_patches.back());

        for (u64 i = 0; i < size; i++)
            this->m_patches.back()[offset + this->getBaseAddress() + i] = reinterpret_cast<const u8*>(buffer)[i];
    }

    void FileProvider::readRaw(u64 offset, void *buffer, size_t size) {
        if ((offset + size) > this->getSize() || buffer == nullptr || size == 0)
            return;

        std::memcpy(buffer, reinterpret_cast<u8*>(this->m_mappedFile) + PageSize * this->m_currPage + offset, size);
    }

    void FileProvider::writeRaw(u64 offset, const void *buffer, size_t size) {
        if ((offset + size) > this->getSize() || buffer == nullptr || size == 0)
            return;

        std::memcpy(reinterpret_cast<u8*>(this->m_mappedFile) + PageSize * this->m_currPage + offset, buffer, size);
    }
    size_t FileProvider::getActualSize() {
        return this->m_fileSize;
    }

    std::vector<std::pair<std::string, std::string>> FileProvider::getDataInformation() {
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

}