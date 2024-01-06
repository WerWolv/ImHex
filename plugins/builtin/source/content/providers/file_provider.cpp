#include "content/providers/file_provider.hpp"
#include "content/providers/memory_file_provider.hpp"

#include <cstring>

#include <hex/api/imhex_api.hpp>
#include <hex/api/localization_manager.hpp>
#include <hex/api/project_file_manager.hpp>
#include <hex/api/task_manager.hpp>

#include <hex/helpers/utils.hpp>
#include <hex/helpers/fmt.hpp>
#include <fmt/chrono.h>

#include <wolv/utils/string.hpp>

#include <nlohmann/json.hpp>

#if defined(OS_WINDOWS)
    #include <windows.h>
#endif

namespace hex::plugin::builtin {

    bool FileProvider::isAvailable() const {
        return true;
    }

    bool FileProvider::isReadable() const {
        return isAvailable() && m_readable;
    }

    bool FileProvider::isWritable() const {
        return isAvailable() && m_writable;
    }

    bool FileProvider::isResizable() const {
        return isAvailable() && isWritable();
    }

    bool FileProvider::isSavable() const {
        return m_undoRedoStack.canUndo();
    }

    void FileProvider::readRaw(u64 offset, void *buffer, size_t size) {
        if (m_fileSize == 0 || (offset + size) > m_fileSize || buffer == nullptr || size == 0)
            return;

        m_file.readBufferAtomic(offset, static_cast<u8*>(buffer), size);
    }

    void FileProvider::writeRaw(u64 offset, const void *buffer, size_t size) {
        if ((offset + size) > this->getActualSize() || buffer == nullptr || size == 0)
            return;

        m_file.writeBufferAtomic(offset, static_cast<const u8*>(buffer), size);
    }

    void FileProvider::save() {
        #if defined(OS_WINDOWS)
            FILETIME ft;
            SYSTEMTIME st;

            wolv::io::File file(m_path, wolv::io::File::Mode::Write);
            if (file.isValid()) {
                GetSystemTime(&st);
                if (SystemTimeToFileTime(&st, &ft)) {
                    auto fileHandle = HANDLE(_get_osfhandle(_fileno(file.getHandle())));
                    SetFileTime(fileHandle, nullptr, nullptr, &ft);
                }
            }
        #endif

        Provider::save();
    }

    void FileProvider::saveAs(const std::fs::path &path) {
        if (path == m_path)
            this->save();
        else
            Provider::saveAs(path);
    }

    void FileProvider::resizeRaw(u64 newSize) {
        this->close();

        {
            wolv::io::File file(m_path, wolv::io::File::Mode::Write);

            file.setSize(newSize);
        }

        (void)this->open();
    }

    void FileProvider::insertRaw(u64 offset, u64 size) {
        auto oldSize = this->getActualSize();
        this->resizeRaw(oldSize + size);

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
    }

    void FileProvider::removeRaw(u64 offset, u64 size) {
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

        this->resizeRaw(newSize);
    }

    u64 FileProvider::getActualSize() const {
        return m_fileSize;
    }

    std::string FileProvider::getName() const {
        return wolv::util::toUTF8String(m_path.filename());
    }

    std::vector<FileProvider::Description> FileProvider::getDataDescription() const {
        std::vector<Description> result;

        result.emplace_back("hex.builtin.provider.file.path"_lang, wolv::util::toUTF8String(m_path));
        result.emplace_back("hex.builtin.provider.file.size"_lang, hex::toByteString(this->getActualSize()));

        if (m_fileStats.has_value()) {
            std::string creationTime, accessTime, modificationTime;

            try { creationTime = hex::format("{:%Y-%m-%d %H:%M:%S}", fmt::localtime(m_fileStats->st_ctime)); }
            catch (const std::exception&) { creationTime = "???"; }

            try { accessTime = hex::format("{:%Y-%m-%d %H:%M:%S}", fmt::localtime(m_fileStats->st_atime)); }
            catch (const std::exception&) { accessTime = "???"; }

            try { modificationTime = hex::format("{:%Y-%m-%d %H:%M:%S}", fmt::localtime(m_fileStats->st_mtime)); }
            catch (const std::exception&) { modificationTime = "???"; }

            result.emplace_back("hex.builtin.provider.file.creation"_lang,      creationTime);
            result.emplace_back("hex.builtin.provider.file.access"_lang,        accessTime);
            result.emplace_back("hex.builtin.provider.file.modification"_lang,  modificationTime);
        }

        return result;
    }

    std::variant<std::string, i128> FileProvider::queryInformation(const std::string &category, const std::string &argument) {
        if (category == "file_path")
            return wolv::util::toUTF8String(m_path);
        else if (category == "file_name")
            return wolv::util::toUTF8String(m_path.filename());
        else if (category == "file_extension")
            return wolv::util::toUTF8String(m_path.extension());
        else if (category == "creation_time")
            return m_fileStats->st_ctime;
        else if (category == "access_time")
            return m_fileStats->st_atime;
        else if (category == "modification_time")
            return m_fileStats->st_mtime;
        else if (category == "permissions")
            return m_fileStats->st_mode & 0777;
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
            { "hex.builtin.provider.file.menu.open_folder"_lang, [this] { fs::openFolderWithSelectionExternal(m_path); } },
            { "hex.builtin.provider.file.menu.open_file"_lang,   [this] { fs::openFileExternal(m_path); } },
            { "hex.builtin.provider.file.menu.into_memory"_lang, [this] { this->convertToMemoryFile(); } }
        };
    }

    void FileProvider::setPath(const std::fs::path &path) {
        m_path = path;
    }

    bool FileProvider::open() {
        m_readable = true;
        m_writable = true;

        if (!std::fs::exists(m_path)) {
            this->setErrorMessage(hex::format("hex.builtin.provider.file.error.open"_lang, m_path.string(), ::strerror(ENOENT)));
            return false;
        }

        wolv::io::File file(m_path, wolv::io::File::Mode::Write);
        if (!file.isValid()) {
            m_writable = false;

            file = wolv::io::File(m_path, wolv::io::File::Mode::Read);
            if (!file.isValid()) {
                m_readable = false;
                this->setErrorMessage(hex::format("hex.builtin.provider.file.error.open"_lang, m_path.string(), ::strerror(errno)));
                return false;
            }
        }

        m_file      = std::move(file);
        m_fileStats = m_file.getFileInfo();
        m_fileSize  = m_file.getSize();

        return true;
    }

    void FileProvider::close() {
        m_file.close();
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
                    this->setErrorMessage(hex::format("hex.builtin.provider.file.error.open"_lang, m_path.string(), e.what()));
                }
            }
        } else {
            this->setPath(path);
        }
    }

    nlohmann::json FileProvider::storeSettings(nlohmann::json settings) const {
        std::string path;
        if (auto projectPath = ProjectFile::getPath(); !projectPath.empty())
            path = wolv::util::toUTF8String(std::fs::proximate(m_path, projectPath.parent_path()));
        if (path.empty())
            path = wolv::util::toUTF8String(m_path);

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

    void FileProvider::convertToMemoryFile() {
        auto newProvider = hex::ImHexApi::Provider::createProvider("hex.builtin.provider.mem_file", true);

        if (auto memoryProvider = dynamic_cast<MemoryFileProvider*>(newProvider); memoryProvider != nullptr) {
            if (!memoryProvider->open()) {
                ImHexApi::Provider::remove(newProvider);
            } else {
                const auto size = this->getActualSize();
                TaskManager::createTask("Loading into memory", size, [this, size, memoryProvider](Task &task) {
                    task.setInterruptCallback([memoryProvider]{
                        ImHexApi::Provider::remove(memoryProvider);
                    });

                    constexpr static size_t BufferSize = 0x10000;
                    std::vector<u8> buffer(BufferSize);

                    memoryProvider->resize(size);

                    for (u64 i = 0; i < size; i += BufferSize) {
                        auto copySize = std::min<size_t>(BufferSize, size - i);
                        this->read(i, buffer.data(), copySize, true);
                        memoryProvider->writeRaw(i, buffer.data(), copySize);
                        task.update(i);
                    }

                    memoryProvider->markDirty(true);
                    memoryProvider->getUndoStack().reset();

                    TaskManager::runWhenTasksFinished([this]{
                        ImHexApi::Provider::remove(this, false);
                    });
                });
            }
        }
    }

}
