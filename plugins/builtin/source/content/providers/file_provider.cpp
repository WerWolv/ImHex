#include "content/providers/file_provider.hpp"

#include <cstring>

#include <hex/api/localization.hpp>
#include <hex/api/project_file_manager.hpp>
#include <hex/api/achievement_manager.hpp>

#include <hex/helpers/utils.hpp>
#include <hex/helpers/fmt.hpp>

#include <wolv/utils/string.hpp>

#include <nlohmann/json.hpp>

namespace hex::plugin::builtin {

    bool FileProvider::isAvailable() const {
        return true;
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
        this->readRaw(offset - this->getBaseAddress(), buffer, size);

        if (overlays) [[likely]] {
            for (const auto&[patchOffset, patchData] : getPatches()) {
                if (patchOffset >= offset && patchOffset < (offset + size))
                    reinterpret_cast<u8 *>(buffer)[patchOffset - offset] = patchData;
            }

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

        std::memcpy(buffer, this->m_file.getMapping() + offset, size);
    }

    void FileProvider::writeRaw(u64 offset, const void *buffer, size_t size) {
        if ((offset + size) > this->getActualSize() || buffer == nullptr || size == 0)
            return;

        std::memcpy(this->m_file.getMapping() + offset, buffer, size);
    }

    void FileProvider::save() {
        this->applyPatches();
        Provider::save();
    }

    void FileProvider::saveAs(const std::fs::path &path) {
        if (path == this->m_path)
            this->save();
        else
            Provider::saveAs(path);
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
        if (offset > this->getActualSize() || size == 0)
            return;

        if ((offset + size) > this->getActualSize())
            size = this->getActualSize() - offset;

        auto oldSize = this->getActualSize();

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
        return wolv::util::toUTF8String(this->m_path.filename());
    }

    std::vector<FileProvider::Description> FileProvider::getDataDescription() const {
        std::vector<Description> result;

        result.emplace_back("hex.builtin.provider.file.path"_lang, wolv::util::toUTF8String(this->m_path));
        result.emplace_back("hex.builtin.provider.file.size"_lang, hex::toByteString(this->getActualSize()));

        if (this->m_fileStats.has_value()) {
            std::string creationTime, accessTime, modificationTime;

            try { creationTime = hex::format("{:%Y-%m-%d %H:%M:%S}", fmt::localtime(this->m_fileStats->st_ctime)); }
            catch (const std::exception&) { creationTime = "???"; }

            try { accessTime = hex::format("{:%Y-%m-%d %H:%M:%S}", fmt::localtime(this->m_fileStats->st_atime)); }
            catch (const std::exception&) { accessTime = "???"; }

            try { modificationTime = hex::format("{:%Y-%m-%d %H:%M:%S}", fmt::localtime(this->m_fileStats->st_mtime)); }
            catch (const std::exception&) { modificationTime = "???"; }

            result.emplace_back("hex.builtin.provider.file.creation"_lang,      creationTime);
            result.emplace_back("hex.builtin.provider.file.access"_lang,        accessTime);
            result.emplace_back("hex.builtin.provider.file.modification"_lang,  modificationTime);
        }

        return result;
    }

    std::variant<std::string, i128> FileProvider::queryInformation(const std::string &category, const std::string &argument) {
        if (category == "file_path")
            return wolv::util::toUTF8String(this->m_path);
        else if (category == "file_name")
            return wolv::util::toUTF8String(this->m_path.filename());
        else if (category == "file_extension")
            return wolv::util::toUTF8String(this->m_path.extension());
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

    std::vector<FileProvider::MenuEntry> FileProvider::getMenuEntries(){
        return {
            {"hex.builtin.provider.file.menu.open_folder"_lang, [path = this->m_path] {
                fs::openFolderWithSelectionExternal(path);
            }},
            
            {"hex.builtin.provider.file.menu.open_file"_lang, [path = this->m_path] {
                fs::openFileExternal(path);
            }},
            
        };
    }

    void FileProvider::setPath(const std::fs::path &path) {
        this->m_path = path;
    }

    bool FileProvider::open() {
        this->m_readable = true;
        this->m_writable = true;

        if (!std::fs::exists(this->m_path)) {
            this->setErrorMessage(hex::format("hex.builtin.provider.file.error.open"_lang, this->m_path.string(), ::strerror(ENOENT)));
            return false;
        }

        wolv::io::File file(this->m_path, wolv::io::File::Mode::Write);
        if (!file.isValid()) {
            this->m_writable = false;

            file = wolv::io::File(this->m_path, wolv::io::File::Mode::Read);
            if (!file.isValid()) {
                this->m_readable = false;
                this->setErrorMessage(hex::format("hex.builtin.provider.file.error.open"_lang, this->m_path.string(), ::strerror(errno)));
                return false;
            }
        }

        this->m_fileStats = file.getFileInfo();
        this->m_file      = std::move(file);

        this->m_file.map();
        this->m_fileSize = this->m_file.getSize();

        this->m_file.close();

        AchievementManager::unlockAchievement("hex.builtin.achievement.starting_out", "hex.builtin.achievement.starting_out.open_file.name");

        return true;
    }

    void FileProvider::close() {
        this->m_file.unmap();
    }

    void FileProvider::loadSettings(const nlohmann::json &settings) {
        Provider::loadSettings(settings);

        auto pathString = settings.at("path").get<std::string>();
        std::fs::path path = std::u8string(pathString.begin(), pathString.end());

        if (auto projectPath = ProjectFile::getPath(); !projectPath.empty()) {
            try {
                this->setPath(std::fs::weakly_canonical(projectPath.parent_path() / path));
            } catch (const std::fs::filesystem_error &) {
                try {
                    this->setPath(projectPath.parent_path() / path);
                } catch (const std::fs::filesystem_error &e) {
                    this->setErrorMessage(hex::format("hex.builtin.provider.file.error.open"_lang, this->m_path.string(), e.what()));
                }
            }
        }
        else
            this->setPath(path);
    }

    nlohmann::json FileProvider::storeSettings(nlohmann::json settings) const {
        std::string path;
        if (auto projectPath = ProjectFile::getPath(); !projectPath.empty())
            path = wolv::util::toUTF8String(std::fs::proximate(this->m_path, projectPath.parent_path()));
        if (path.empty())
            path = wolv::util::toUTF8String(this->m_path);

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
