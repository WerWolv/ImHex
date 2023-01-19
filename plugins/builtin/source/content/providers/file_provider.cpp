#include "content/providers/file_provider.hpp"

#include <cstring>

#include <hex/api/imhex_api.hpp>
#include <hex/api/localization.hpp>
#include <hex/api/project_file_manager.hpp>

#include <hex/helpers/utils.hpp>
#include <hex/helpers/file.hpp>
#include <hex/helpers/fmt.hpp>

#include <nlohmann/json.hpp>

namespace hex::plugin::builtin {

    bool FileProvider::isAvailable() const {
        return this->m_mappedFile != nullptr;
    }

    bool FileProvider::isReadable() const {
        return isAvailable() && this->m_readable;
    }

    bool FileProvider::isWritable() const {
        return isAvailable() && this->m_writable;
    }

    bool FileProvider::isResizable() const {
        return isAvailable() && isWritable();
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
                reinterpret_cast<u8 *>(buffer)[i] = getPatches()[offset + i];

        if (overlays)
            this->applyOverlays(offset, buffer, size);
    }

    void FileProvider::write(u64 offset, const void *buffer, size_t size) {
        if ((offset - this->getBaseAddress()) > (this->getActualSize() - size) || buffer == nullptr || size == 0)
            return;

        addPatch(offset, buffer, size, true);
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

    void FileProvider::saveAs(const std::fs::path &path) {
        fs::File file(path, fs::File::Mode::Create);

        if (file.isValid()) {
            auto provider = ImHexApi::Provider::get();

            std::vector<u8> buffer(std::min<size_t>(0xFF'FFFF, provider->getActualSize()), 0x00);
            size_t bufferSize = buffer.size();

            for (u64 offset = 0; offset < provider->getActualSize(); offset += bufferSize) {
                if (bufferSize > provider->getActualSize() - offset)
                    bufferSize = provider->getActualSize() - offset;

                provider->read(offset + this->getBaseAddress(), buffer.data(), bufferSize);
                file.write(buffer.data(), bufferSize);
            }
        }
    }

    void FileProvider::resize(size_t newSize) {
        this->close();

        {
            fs::File file(this->m_path, fs::File::Mode::Write);

            file.setSize(newSize);
            this->m_fileSize = file.getSize();
        }

        (void)this->open();
    }

    void FileProvider::insert(u64 offset, size_t size) {
        auto oldSize = this->getActualSize();
        this->resize(oldSize + size);

        std::vector<u8> buffer(0x1000);
        const std::vector<u8> zeroBuffer(0x1000);

        auto position = oldSize;
        while (position > offset) {
            const auto readSize = std::min<size_t>(position - offset, buffer.size());

            position -= readSize;

            this->readRaw(position, buffer.data(), readSize);
            this->writeRaw(position, zeroBuffer.data(), readSize);
            this->writeRaw(position + size, buffer.data(), readSize);
        }

        Provider::insert(offset, size);
    }

    void FileProvider::remove(u64 offset, size_t size) {
        auto oldSize = this->getActualSize();
        this->resize(oldSize + size);

        std::vector<u8> buffer(0x1000);

        const auto newSize = oldSize - size;
        auto position = offset;
        while (position < newSize) {
            const auto readSize = std::min<size_t>(newSize - position, buffer.size());

            this->readRaw(position + size, buffer.data(), readSize);
            this->writeRaw(position, buffer.data(), readSize);

            position += readSize;
        }

        this->resize(newSize);

        Provider::insert(offset, size);
        Provider::remove(offset, size);
    }

    size_t FileProvider::getActualSize() const {
        return this->m_fileSize;
    }

    std::string FileProvider::getName() const {
        return hex::toUTF8String(this->m_path.filename());
    }

    std::vector<std::pair<std::string, std::string>> FileProvider::getDataInformation() const {
        std::vector<std::pair<std::string, std::string>> result;

        result.emplace_back("hex.builtin.provider.file.path"_lang, hex::toUTF8String(this->m_path));
        result.emplace_back("hex.builtin.provider.file.size"_lang, hex::toByteString(this->getActualSize()));

        if (this->m_fileStatsValid) {
            result.emplace_back("hex.builtin.provider.file.creation"_lang, hex::format("{:%Y-%m-%d %H:%M:%S}", fmt::localtime(this->m_fileStats.st_ctime)));
            result.emplace_back("hex.builtin.provider.file.access"_lang, hex::format("{:%Y-%m-%d %H:%M:%S}", fmt::localtime(this->m_fileStats.st_atime)));
            result.emplace_back("hex.builtin.provider.file.modification"_lang, hex::format("{:%Y-%m-%d %H:%M:%S}", fmt::localtime(this->m_fileStats.st_mtime)));
        }

        return result;
    }

    bool FileProvider::handleFilePicker() {
        return fs::openFileBrowser(fs::DialogMode::Open, {}, [this](const auto &path) {
            this->setPath(path);
        });
    }

    void FileProvider::setPath(const std::fs::path &path) {
        this->m_path = path;
    }

    bool FileProvider::open() {
        this->m_readable = true;
        this->m_writable = true;

        #if defined(OS_WINDOWS)
            const auto &path = this->m_path.native();

            this->m_fileStatsValid = wstat(path.c_str(), &this->m_fileStats) == 0;

            LARGE_INTEGER fileSize = {};
            auto file = reinterpret_cast<HANDLE>(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));

            GetFileSizeEx(file, &fileSize);
            this->m_fileSize = fileSize.QuadPart;
            CloseHandle(file);

            file = reinterpret_cast<HANDLE>(CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
            if (file == nullptr || file == INVALID_HANDLE_VALUE) {
                file = reinterpret_cast<HANDLE>(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
                this->m_writable = false;
            }

            if (file == nullptr || file == INVALID_HANDLE_VALUE) {
                return false;
            }

            ON_SCOPE_EXIT { CloseHandle(file); };

            if (this->m_fileSize > 0) {
                HANDLE mapping = CreateFileMapping(file, nullptr, PAGE_READWRITE, 0, 0, nullptr);
                ON_SCOPE_EXIT { CloseHandle(mapping); };

                if (mapping == nullptr || mapping == INVALID_HANDLE_VALUE) {
                    mapping = CreateFileMapping(file, nullptr, PAGE_READONLY, 0, 0, nullptr);

                    if (mapping == nullptr || mapping == INVALID_HANDLE_VALUE)
                        return false;
                }

                this->m_mappedFile = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, this->m_fileSize);
                if (this->m_mappedFile == nullptr) {

                    this->m_mappedFile = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, this->m_fileSize);
                    if (this->m_mappedFile == nullptr) {
                        this->m_readable = false;

                        return false;
                    }
                }
            } else if (!this->m_emptyFile) {
                this->m_emptyFile = true;
                this->resize(1);
            } else {
                return false;
            }
        #else

            const auto &path       = this->m_path.native();
            this->m_fileStatsValid = stat(path.c_str(), &this->m_fileStats) == 0;

            int mmapprot = PROT_READ | PROT_WRITE;

            auto file = ::open(path.c_str(), O_RDWR);
            if (file == -1) {
                file = ::open(path.c_str(), O_RDONLY);
                this->m_writable = false;
                mmapprot &= ~(PROT_WRITE);
            }

            if (file == -1) {
                this->m_readable = false;
                return false;
            }

            ON_SCOPE_EXIT { ::close(file); };

            this->m_fileSize = this->m_fileStats.st_size;

            this->m_mappedFile = ::mmap(nullptr, this->m_fileSize, mmapprot, MAP_SHARED, file, 0);
            if (this->m_mappedFile == MAP_FAILED)
                return false;

        #endif

        return true;
    }

    void FileProvider::close() {
        #if defined(OS_WINDOWS)

            if (this->m_mappedFile != nullptr)
                ::UnmapViewOfFile(this->m_mappedFile);

        #else

            if (this->m_mappedFile != nullptr)
                ::munmap(this->m_mappedFile, this->m_fileSize);

        #endif
    }

    void FileProvider::loadSettings(const nlohmann::json &settings) {
        Provider::loadSettings(settings);

        auto pathString = settings["path"].get<std::string>();
        std::fs::path path = std::u8string(pathString.begin(), pathString.end());

        if (auto projectPath = ProjectFile::getPath(); !projectPath.empty())
            this->setPath(std::filesystem::weakly_canonical(projectPath.parent_path() / path));
        else
            this->setPath(path);
    }

    nlohmann::json FileProvider::storeSettings(nlohmann::json settings) const {
        std::string path;
        if (auto projectPath = ProjectFile::getPath(); !projectPath.empty())
            path = hex::toUTF8String(std::fs::proximate(this->m_path, projectPath.parent_path()));
        if (path.empty())
            path = hex::toUTF8String(this->m_path);

        settings["path"] = path;

        return Provider::storeSettings(settings);
    }

    std::pair<Region, bool> FileProvider::getRegionValidity(u64 address) const {
        address -= this->getBaseAddress();

        if (address < this->getActualSize())
            return { Region { this->getBaseAddress() + address, this->getActualSize() - address }, true };
        else
            return { Region::Invalid(), false };
    }

}
