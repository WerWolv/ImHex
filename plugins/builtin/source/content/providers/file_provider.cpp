#include "content/providers/file_provider.hpp"

#include <cstring>

#include <hex/api/imhex_api.hpp>
#include <hex/api/localization.hpp>
#include <hex/api/project_file_manager.hpp>

#include <hex/helpers/utils.hpp>
#include <hex/helpers/fmt.hpp>

#include <nlohmann/json.hpp>

namespace hex::plugin::builtin {

    bool FileProvider::isAvailable() const {
        return this->m_file.isValid();
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

        if (overlays) {
            for (u64 i = 0; i < size; i++)
                if (getPatches().contains(offset + i))
                    reinterpret_cast<u8 *>(buffer)[i] = getPatches()[offset + i];

            this->applyOverlays(offset, buffer, size);
        }
    }

    void FileProvider::write(u64 offset, const void *buffer, size_t size) {
        if ((offset - this->getBaseAddress()) > (this->getActualSize() - size) || buffer == nullptr || size == 0)
            return;

        addPatch(offset, buffer, size, true);
    }

    void FileProvider::readRaw(u64 offset, void *buffer, size_t size) {
        if (offset > (this->getActualSize() - size) || buffer == nullptr || size == 0)
            return;

        std::scoped_lock lock(this->m_mutex);
        if (offset < this->m_bufferStartAddress || offset + size > this->m_bufferStartAddress + this->m_buffer.size() || this->m_buffer.size() < size) {
            this->m_buffer.resize(std::min<size_t>(std::max<size_t>(0x100, size), this->getActualSize() - offset));
            this->m_file.seek(offset);
            this->m_file.readBuffer(this->m_buffer.data(), this->m_buffer.size());
            this->m_bufferStartAddress = offset;
        }

        std::memcpy(buffer, this->m_buffer.data() + (offset - this->m_bufferStartAddress), std::min<size_t>(size, this->m_buffer.size()));
    }

    void FileProvider::writeRaw(u64 offset, const void *buffer, size_t size) {
        if ((offset + size) > this->getActualSize() || buffer == nullptr || size == 0)
            return;

        std::scoped_lock lock(this->m_mutex);
        wolv::io::File writeFile(this->m_path, wolv::io::File::Mode::Write);
        if (!writeFile.isValid())
            return;
        
        writeFile.seek(offset);
        writeFile.write(reinterpret_cast<const u8*>(buffer), size);
        this->m_buffer.clear();
    }

    void FileProvider::save() {
        this->applyPatches();
    }

    void FileProvider::saveAs(const std::fs::path &path) {
        wolv::io::File file(path, wolv::io::File::Mode::Create);

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
            wolv::io::File file(this->m_path, wolv::io::File::Mode::Write);

            file.setSize(newSize);
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

    std::vector<std::pair<std::string, std::string>> FileProvider::getDataDescription() const {
        std::vector<std::pair<std::string, std::string>> result;

        result.emplace_back("hex.builtin.provider.file.path"_lang, hex::toUTF8String(this->m_path));
        result.emplace_back("hex.builtin.provider.file.size"_lang, hex::toByteString(this->getActualSize()));

        if (this->m_fileStats.has_value()) {
            result.emplace_back("hex.builtin.provider.file.creation"_lang, hex::format("{:%Y-%m-%d %H:%M:%S}", fmt::localtime(this->m_fileStats->st_ctime)));
            result.emplace_back("hex.builtin.provider.file.access"_lang, hex::format("{:%Y-%m-%d %H:%M:%S}", fmt::localtime(this->m_fileStats->st_atime)));
            result.emplace_back("hex.builtin.provider.file.modification"_lang, hex::format("{:%Y-%m-%d %H:%M:%S}", fmt::localtime(this->m_fileStats->st_mtime)));
        }

        return result;
    }

    std::variant<std::string, i128> FileProvider::queryInformation(const std::string &category, const std::string &argument) {
        if (category == "file_path")
            return hex::toUTF8String(this->m_path);
        else if (category == "file_name")
            return hex::toUTF8String(this->m_path.filename());
        else if (category == "file_extension")
            return hex::toUTF8String(this->m_path.extension());
        else if (category == "creation_time")
            return this->m_fileStats->st_ctime;
        else if (category == "access_time")
            return this->m_fileStats->st_atime;
        else if (category == "modification_time")
            return this->m_fileStats->st_mtime;
        else if (category == "permissions")
            return this->m_fileStats->st_mode & 0777;
        else
            return Provider::queryInformation(category, argument);
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

        wolv::io::File file(this->m_path, wolv::io::File::Mode::Read);
        if (!file.isValid()) {
            this->m_writable = false;
            this->m_readable = false;
            return false;
        }

        this->m_fileStats = file.getFileInfo();
        this->m_fileSize = file.getSize();
        this->m_file = std::move(file);

        return true;
    }

    void FileProvider::close() {

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
