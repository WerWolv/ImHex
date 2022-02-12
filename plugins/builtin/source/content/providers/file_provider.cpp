#include "content/providers/file_provider.hpp"

#include <ctime>
#include <cstring>

#include <hex/api/localization.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/helpers/file.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/helpers/project_file_handler.hpp>

namespace hex::plugin::builtin::prv {

    FileProvider::FileProvider() : Provider() {
    }

    FileProvider::~FileProvider() {
        this->close();
    }


    bool FileProvider::isAvailable() const {
#if defined(OS_WINDOWS)
        return this->m_file != INVALID_HANDLE_VALUE && this->m_mapping != INVALID_HANDLE_VALUE && this->m_mappedFile != nullptr;
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
        if ((offset - this->getBaseAddress()) > (this->getActualSize() - size) || buffer == nullptr || size == 0)
            return;

        this->readRaw(offset - this->getBaseAddress(), buffer, size);

        for (u64 i = 0; i < size; i++)
            if (getPatches().contains(offset + i))
                reinterpret_cast<u8 *>(buffer)[i] = getPatches()[offset + PageSize * this->m_currPage + i];

        if (overlays)
            this->applyOverlays(offset, buffer, size);
    }

    void FileProvider::write(u64 offset, const void *buffer, size_t size) {
        if ((offset - this->getBaseAddress()) > (this->getActualSize() - size) || buffer == nullptr || size == 0)
            return;

        addPatch(offset, buffer, size);
    }

    void FileProvider::readRaw(u64 offset, void *buffer, size_t size) {
        if ((offset + size) > this->getActualSize() || buffer == nullptr || size == 0)
            return;

        std::memcpy(buffer, reinterpret_cast<u8 *>(this->m_mappedFile) + offset, size);
    }

    void FileProvider::writeRaw(u64 offset, const void *buffer, size_t size) {
        if ((offset + size) > this->getActualSize() || buffer == nullptr || size == 0)
            return;

        std::memcpy(reinterpret_cast<u8 *>(this->m_mappedFile) + offset, buffer, size);
    }

    void FileProvider::save() {
        this->applyPatches();
    }

    void FileProvider::saveAs(const fs::path &path) {
        File file(path, File::Mode::Create);

        if (file.isValid()) {
            auto provider = ImHexApi::Provider::get();

            std::vector<u8> buffer(std::min<size_t>(0xFF'FFFF, provider->getActualSize()), 0x00);
            size_t bufferSize = buffer.size();

            for (u64 offset = 0; offset < provider->getActualSize(); offset += bufferSize) {
                if (bufferSize > provider->getActualSize() - offset)
                    bufferSize = provider->getActualSize() - offset;

                provider->read(offset + this->getBaseAddress(), buffer.data(), bufferSize);
                file.write(buffer);
            }
        }
    }

    void FileProvider::resize(size_t newSize) {
        this->close();

        {
            auto file = File(this->m_path, File::Mode::Write);

            file.setSize(newSize);
            this->m_fileSize = file.getSize();
        }

        this->open();
    }

    void FileProvider::insert(u64 offset, size_t size) {
        auto oldSize = this->getActualSize();
        this->resize(oldSize + size);

        std::vector<u8> buffer(0x1000);
        const std::vector<u8> zeroBuffer(0x1000);

        auto position = oldSize;
        while (position > offset) {
            size_t readSize = (position >= (offset + buffer.size())) ? buffer.size() : (position - offset);

            position -= readSize;

            this->readRaw(position, buffer.data(), readSize);
            this->writeRaw(position, zeroBuffer.data(), readSize);
            this->writeRaw(position + size, buffer.data(), readSize);
        }

        Provider::insert(offset, size);
    }

    size_t FileProvider::getActualSize() const {
        return this->m_fileSize;
    }

    std::string FileProvider::getName() const {
        return fs::path(this->m_path).filename().string();
    }

    std::vector<std::pair<std::string, std::string>> FileProvider::getDataInformation() const {
        std::vector<std::pair<std::string, std::string>> result;

        result.emplace_back("hex.builtin.provider.file.path"_lang, this->m_path.string());
        result.emplace_back("hex.builtin.provider.file.size"_lang, hex::toByteString(this->getActualSize()));

        if (this->m_fileStatsValid) {
            result.emplace_back("hex.builtin.provider.file.creation"_lang, hex::format("{}", fmt::localtime(this->m_fileStats.st_ctime)));
            result.emplace_back("hex.builtin.provider.file.access"_lang, hex::format("{}", fmt::localtime(this->m_fileStats.st_atime)));
            result.emplace_back("hex.builtin.provider.file.modification"_lang, hex::format("{}", fmt::localtime(this->m_fileStats.st_mtime)));
        }

        return result;
    }

    void FileProvider::setPath(const fs::path &path) {
        this->m_path = path;
    }

    bool FileProvider::open() {
        this->m_fileStatsValid = stat(this->m_path.string().c_str(), &this->m_fileStats) == 0;

        this->m_readable = true;
        this->m_writable = true;

#if defined(OS_WINDOWS)
        const auto &path = this->m_path.native();

        LARGE_INTEGER fileSize = { 0 };
        this->m_file           = reinterpret_cast<HANDLE>(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));

        GetFileSizeEx(this->m_file, &fileSize);
        this->m_fileSize = fileSize.QuadPart;
        CloseHandle(this->m_file);

        this->m_file = reinterpret_cast<HANDLE>(CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
        if (this->m_file == nullptr || this->m_file == INVALID_HANDLE_VALUE) {
            this->m_file     = reinterpret_cast<HANDLE>(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
            this->m_writable = false;
        }

        auto fileCleanup = SCOPE_GUARD {
            CloseHandle(this->m_file);

            this->m_readable = false;
            this->m_file     = nullptr;
        };

        if (this->m_file == nullptr || this->m_file == INVALID_HANDLE_VALUE) {
            return false;
        }

        if (this->m_fileSize > 0) {
            this->m_mapping = CreateFileMapping(this->m_file, nullptr, PAGE_READWRITE, 0, 0, nullptr);
            if (this->m_mapping == nullptr || this->m_mapping == INVALID_HANDLE_VALUE) {

                this->m_mapping = CreateFileMapping(this->m_file, nullptr, PAGE_READONLY, 0, 0, nullptr);

                if (this->m_mapping == nullptr || this->m_mapping == INVALID_HANDLE_VALUE)
                    return false;
            }

            auto mappingCleanup = SCOPE_GUARD {
                CloseHandle(this->m_mapping);

                this->m_mapping  = nullptr;
                this->m_readable = false;
            };

            this->m_mappedFile = MapViewOfFile(this->m_mapping, FILE_MAP_ALL_ACCESS, 0, 0, this->m_fileSize);
            if (this->m_mappedFile == nullptr) {

                this->m_mappedFile = MapViewOfFile(this->m_mapping, FILE_MAP_READ, 0, 0, this->m_fileSize);
                if (this->m_mappedFile == nullptr) {
                    this->m_readable = false;

                    return false;
                }
            }

            mappingCleanup.release();

            ProjectFile::setFilePath(this->m_path);
        } else if (!this->m_emptyFile) {
            this->m_emptyFile = true;
            this->resize(1);
        } else {
            return false;
        }

        fileCleanup.release();

#else
        const auto &path = this->m_path.native();
        int mmapprot = PROT_READ | PROT_WRITE;

        this->m_file = ::open(path.c_str(), O_RDWR);
        if (this->m_file == -1) {
            this->m_file     = ::open(path.c_str(), O_RDONLY);
            this->m_writable = false;
            mmapprot &= ~(PROT_WRITE);
        }

        if (this->m_file == -1) {
            this->m_readable = false;
            return false;
        }

        this->m_fileSize = this->m_fileStats.st_size;

        this->m_mappedFile = ::mmap(nullptr, this->m_fileSize, mmapprot, MAP_SHARED, this->m_file, 0);
        if (this->m_mappedFile == MAP_FAILED) {
            ::close(this->m_file);
            this->m_file = -1;

            return false;
        }
#endif

        return true;
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
