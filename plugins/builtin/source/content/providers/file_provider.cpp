#include "content/providers/file_provider.hpp"
#include "content/providers/memory_file_provider.hpp"

#include <hex/api/imhex_api.hpp>
#include <hex/api/localization_manager.hpp>
#include <hex/api/project_file_manager.hpp>
#include <hex/api/task_manager.hpp>

#include <toasts/toast_notification.hpp>

#include <hex/helpers/utils.hpp>
#include <hex/helpers/fmt.hpp>
#include <fmt/chrono.h>

#include <wolv/utils/string.hpp>

#include <nlohmann/json.hpp>
#include <cstring>
#include <popups/popup_question.hpp>

#if defined(OS_WINDOWS)
    #include <windows.h>
#endif

namespace hex::plugin::builtin {

    std::set<FileProvider*> FileProvider::s_openedFiles;

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
        return m_loadedIntoMemory;
    }

    void FileProvider::readRaw(u64 offset, void *buffer, size_t size) {
        if (m_fileSize == 0 || (offset + size) > m_fileSize || buffer == nullptr || size == 0)
            return;

        if (m_loadedIntoMemory)
            std::memcpy(buffer, m_data.data() + offset, size);
        else
            m_file.readBufferAtomic(offset, static_cast<u8*>(buffer), size);
    }

    void FileProvider::writeRaw(u64 offset, const void *buffer, size_t size) {
        if ((offset + size) > this->getActualSize() || buffer == nullptr || size == 0)
            return;

        if (m_loadedIntoMemory)
            std::memcpy(m_data.data() + offset, buffer, size);
        else
            m_file.writeBufferAtomic(offset, static_cast<const u8*>(buffer), size);
    }

    void FileProvider::save() {
        if (m_loadedIntoMemory) {
            m_ignoreNextChangeEvent = true;
            m_file.open();
            m_file.writeVectorAtomic(0x00, m_data);
            m_file.setSize(m_data.size());
        } else {
            m_file.flush();
        }

        #if defined(OS_WINDOWS)
            FILETIME ft;
            SYSTEMTIME st;

            if (m_file.isValid()) {
                GetSystemTime(&st);
                if (SystemTimeToFileTime(&st, &ft)) {
                    auto fileHandle = HANDLE(_get_osfhandle(_fileno(m_file.getHandle())));
                    SetFileTime(fileHandle, nullptr, nullptr, &ft);
                }
            }
        #endif

        if (m_loadedIntoMemory)
            m_file.close();

        Provider::save();
    }

    void FileProvider::saveAs(const std::fs::path &path) {
        if (path == m_path)
            this->save();
        else
            Provider::saveAs(path);
    }

    void FileProvider::resizeRaw(u64 newSize) {
        if (m_loadedIntoMemory)
            m_data.resize(newSize);
        else
            m_file.setSize(newSize);

        m_fileSize = newSize;
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

        if (!wolv::io::fs::exists(m_path)) {
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

            ui::ToastInfo::open("hex.builtin.popup.error.read_only"_lang);
        }

        m_file      = std::move(file);
        m_fileStats = m_file.getFileInfo();
        m_fileSize  = m_file.getSize();

        // Make sure the current file is not already opened
        {
            auto alreadyOpenedFileProvider = std::ranges::find_if(s_openedFiles, [this](const FileProvider *provider) {
                return provider->m_path == m_path;
            });

            if (alreadyOpenedFileProvider != s_openedFiles.end()) {
                ImHexApi::Provider::setCurrentProvider(*alreadyOpenedFileProvider);
                return false;
            } else {
                s_openedFiles.insert(this);
            }
        }

        if (m_writable) {
            if (m_fileSize < MaxMemoryFileSize) {
                m_data = m_file.readVectorAtomic(0x00, m_fileSize);
                if (!m_data.empty()) {
                    m_changeTracker = wolv::io::ChangeTracker(m_file);
                    m_changeTracker.startTracking([this]{ this->handleFileChange(); });
                    m_file.close();
                    m_loadedIntoMemory = true;
                }
            } else {
                m_writable = false;
                ui::PopupQuestion::open("hex.builtin.provider.file.too_large"_lang,
                    [this] {
                        m_writable = false;
                    },
                    [this] {
                        m_writable = true;
                        RequestUpdateWindowTitle::post();
                    });
            }
        }

        return true;
    }

    void FileProvider::close() {
        m_file.close();
        m_data.clear();
        s_openedFiles.erase(this);
        m_changeTracker.stopTracking();
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

    void FileProvider::handleFileChange() {
        if (m_ignoreNextChangeEvent) {
            m_ignoreNextChangeEvent = false;
            return;
        }

        if(m_changeEventAcknowledgementPending) {
            return;
        }

        m_changeEventAcknowledgementPending = true;

        ui::PopupQuestion::open("hex.builtin.provider.file.reload_changes"_lang, [this] {
            this->close();
            (void)this->open();
            getUndoStack().reapply();
            m_changeEventAcknowledgementPending = false;
        },
        [this]{
            m_changeEventAcknowledgementPending = false;
        });
    }


}
