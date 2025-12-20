#include "content/providers/file_provider.hpp"
#include "content/providers/memory_file_provider.hpp"

#include <hex/api/content_registry/settings.hpp>
#include <hex/api/imhex_api/provider.hpp>
#include <hex/api/localization_manager.hpp>
#include <hex/api/project_file_manager.hpp>
#include <hex/api/task_manager.hpp>
#include <hex/api/events/requests_gui.hpp>
#include <hex/api/events/events_interaction.hpp>

#include <banners/banner_button.hpp>
#include <toasts/toast_notification.hpp>

#include <hex/helpers/utils.hpp>
#include <hex/helpers/fmt.hpp>
#include <fmt/chrono.h>

#include <wolv/utils/string.hpp>
#include <wolv/literals.hpp>

#include <nlohmann/json.hpp>
#include <cstring>

#if defined(OS_WINDOWS)
    #include <windows.h>
#endif

namespace hex::plugin::builtin {

    static std::mutex s_openCloseMutex;

    using namespace wolv::literals;

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
        else {
            this->createBackupIfNeeded(m_file.getPath());
            m_file.writeBufferAtomic(offset, static_cast<const u8*>(buffer), size);
        }
    }

    void FileProvider::save() {
        if (m_loadedIntoMemory) {
            m_ignoreNextChangeEvent = true;
            this->createBackupIfNeeded(m_file.getPath());
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
        else {
            this->createBackupIfNeeded(m_file.getPath());
            m_file.setSize(newSize);
        }

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

            try { creationTime = fmt::format("{:%Y-%m-%d %H:%M:%S}", *std::localtime(&m_fileStats->st_ctime)); }
            catch (const fmt::format_error&) { creationTime = "???"; }

            try { accessTime = fmt::format("{:%Y-%m-%d %H:%M:%S}", *std::localtime(&m_fileStats->st_atime)); }
            catch (const fmt::format_error&) { accessTime = "???"; }

            try { modificationTime = fmt::format("{:%Y-%m-%d %H:%M:%S}", *std::localtime(&m_fileStats->st_mtime)); }
            catch (const fmt::format_error&) { modificationTime = "???"; }

            result.emplace_back("hex.builtin.provider.file.creation"_lang,      creationTime);
            result.emplace_back("hex.builtin.provider.file.access"_lang,        accessTime);
            result.emplace_back("hex.builtin.provider.file.modification"_lang,  modificationTime);
        }

        return result;
    }

    std::variant<std::string, i128> FileProvider::queryInformation(const std::string &category, const std::string &argument) {
        if (category == "file_path")
            return wolv::io::fs::toNormalizedPathString(m_path);
        else if (category == "file_name")
            return wolv::io::fs::toNormalizedPathString(m_path.filename());
        else if (category == "file_extension")
            return wolv::io::fs::toNormalizedPathString(m_path.extension());
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

    std::vector<FileProvider::MenuEntry> FileProvider::getMenuEntries() {
        MenuEntry loadMenuItem;

        if (m_loadedIntoMemory)
            loadMenuItem = { .name="hex.builtin.provider.file.menu.direct_access"_lang, .icon=ICON_VS_ARROW_SWAP, .callback=[this] { this->convertToDirectAccess(); } };
        else
            loadMenuItem = { .name="hex.builtin.provider.file.menu.into_memory"_lang, .icon=ICON_VS_ARROW_SWAP, .callback=[this] { this->convertToMemoryFile(); } };

        return {
            { "hex.builtin.provider.file.menu.open_folder"_lang, ICON_VS_FOLDER_OPENED, [this] { fs::openFolderWithSelectionExternal(m_path); } },
            { "hex.builtin.provider.file.menu.open_file"_lang,   ICON_VS_FILE, [this] { fs::openFileExternal(m_path); } },
            loadMenuItem
        };
    }

    void FileProvider::setPath(const std::fs::path &path) {
        m_path = path;
        m_path.make_preferred();
    }

    prv::Provider::OpenResult FileProvider::open() {
        const size_t maxMemoryFileSize = ContentRegistry::Settings::read<u64>("hex.builtin.setting.general", "hex.builtin.setting.general.max_mem_file_size", 128_MiB);

        size_t fileSize = 0x00;
        {
            wolv::io::File file(m_path, wolv::io::File::Mode::Read);
            if (!file.isValid()) {
                return OpenResult::failure(fmt::format("hex.builtin.provider.file.error.open"_lang, m_path.string(), formatSystemError(file.getOpenError().value_or(0))));
            }

            fileSize = file.getSize();
        }

        const bool directAccess = fileSize >= maxMemoryFileSize;
        const auto result = open(directAccess);

        if (result.isSuccess() && directAccess) {
            m_writable = false;

            ui::BannerButton::open(ICON_VS_WARNING, "hex.builtin.provider.file.too_large", ImColor(135, 116, 66), "hex.builtin.provider.file.too_large.allow_write", [this]{
                m_writable = true;
                RequestUpdateWindowTitle::post();
            });
        }

        return result;
    }

    prv::Provider::OpenResult FileProvider::open(bool directAccess) {
        m_readable = true;
        m_writable = true;

        wolv::io::File file(m_path, wolv::io::File::Mode::Write);
        if (!file.isValid()) {
            m_writable = false;

            file = wolv::io::File(m_path, wolv::io::File::Mode::Read);
            if (!file.isValid()) {
                m_readable = false;
                return OpenResult::failure(fmt::format("hex.builtin.provider.file.error.open"_lang, m_path.string(), formatSystemError(file.getOpenError().value_or(0))));
            }

            ui::ToastInfo::open("hex.builtin.popup.error.read_only"_lang);
        }

        std::scoped_lock lock(s_openCloseMutex);

        m_file      = std::move(file);
        m_fileStats = m_file.getFileInfo();
        m_fileSize  = m_file.getSize();

        // Make sure the current file is not already opened
        {
            auto alreadyOpenedFileProvider = std::ranges::find_if(s_openedFiles, [this](const FileProvider *provider) {
                return provider->m_path == m_path;
            });

            if (alreadyOpenedFileProvider != s_openedFiles.end()) {
                return OpenResult::redirect(*alreadyOpenedFileProvider);
            } else {
                s_openedFiles.insert(this);
            }
        }

        if (m_writable) {
            if (directAccess) {
                m_loadedIntoMemory = false;
            } else {
                m_data = m_file.readVectorAtomic(0x00, m_fileSize);
                if (!m_data.empty()) {
                    m_changeTracker = wolv::io::ChangeTracker(m_file);
                    m_changeTracker.startTracking([this]{ this->handleFileChange(); });
                    m_file.close();
                    m_loadedIntoMemory = true;
                }
            }
        }

        m_changeEventAcknowledgementPending = false;

        return {};
    }


    void FileProvider::close() {
        std::scoped_lock lock(s_openCloseMutex);

        m_file.close();
        m_data.clear();
        s_openedFiles.erase(this);
        m_changeTracker.stopTracking();
        m_readable = false;
        m_writable = false;
    }

    void FileProvider::loadSettings(const nlohmann::json &settings) {
        Provider::loadSettings(settings);

        auto pathString = settings.at("path").get<std::string>();
        std::fs::path path = std::u8string(pathString.begin(), pathString.end());

        if (auto projectPath = ProjectFile::getPath(); !projectPath.empty()) {
            std::fs::path fullPath;
            try {
                fullPath = std::fs::weakly_canonical(projectPath.parent_path() / path);
            } catch (const std::fs::filesystem_error &) {
                fullPath = projectPath.parent_path() / path;
            }

            if (!wolv::io::fs::exists(fullPath))
                fullPath = path;

            path = std::move(fullPath);
        }

        this->setPath(path);
    }

    nlohmann::json FileProvider::storeSettings(nlohmann::json settings) const {
        std::fs::path path;

        if (m_path.u8string().starts_with(u8"//")) {
            path = m_path;
        } else {
            if (auto projectPath = ProjectFile::getPath(); !projectPath.empty())
                path = std::fs::proximate(m_path, projectPath.parent_path());
            if (path.empty())
                path = m_path;
        }

        settings["path"] = wolv::io::fs::toNormalizedPathString(path);

        return Provider::storeSettings(settings);
    }

    std::pair<Region, bool> FileProvider::getRegionValidity(u64 address) const {
        address -= this->getBaseAddress();

        if (address < this->getActualSize())
            return { Region { .address=this->getBaseAddress() + address, .size=this->getActualSize() - address }, true };
        else
            return { Region::Invalid(), false };
    }

    void FileProvider::convertToMemoryFile() {
        this->close();
        this->open(false);
    }

    void FileProvider::convertToDirectAccess() {
        this->close();
        this->open(true);
    }

    void FileProvider::handleFileChange() {
        if (m_ignoreNextChangeEvent) {
            m_ignoreNextChangeEvent = false;
            return;
        }

        if (m_changeEventAcknowledgementPending) {
            return;
        }

        m_changeEventAcknowledgementPending = true;
        ui::BannerButton::open(ICON_VS_INFO, "hex.builtin.provider.file.reload_changes", ImColor(66, 104, 135), "hex.builtin.provider.file.reload_changes.reload", [this] {
            this->close();
            (void)this->open(!m_loadedIntoMemory);

            getUndoStack().reapply();
            m_changeEventAcknowledgementPending = false;
            EventDataChanged::post(this);
        });
    }


}
