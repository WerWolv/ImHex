#include <hex/helpers/logger.hpp>

#include "content/providers/disk_provider.hpp"

#include <hex/api/localization.hpp>

#include <hex/helpers/fmt.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/ui/imgui_imhex_extensions.h>

#include <wolv/utils/string.hpp>

#include <bitset>
#include <filesystem>

#include <imgui.h>

#if defined(OS_WINDOWS)
#include <winioctl.h>
#elif defined(OS_LINUX)
#include <fcntl.h>
#include <unistd.h>
#include <linux/fs.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#elif defined(OS_MACOS)
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/disk.h>
#endif

#if defined(OS_LINUX)
#define lseek lseek64
#endif

namespace hex::plugin::builtin {

    DiskProvider::DiskProvider() : Provider() {

    }

    bool DiskProvider::isAvailable() const {
#if defined(OS_WINDOWS)

        return this->m_diskHandle != INVALID_HANDLE_VALUE;

#else

        return this->m_diskHandle != -1;

#endif
    }

    bool DiskProvider::isReadable() const {
        return this->m_readable;
    }

    bool DiskProvider::isWritable() const {
        return this->m_writable;
    }

    bool DiskProvider::isResizable() const {
        return false;
    }

    bool DiskProvider::isSavable() const {
        return false;
    }


    void DiskProvider::setPath(const std::fs::path &path) {
        this->m_path = path;
    }

#if defined (OS_LINUX)
    #ifdef BLKSSZGET
        int blkdev_get_sector_size(int fd, int *sector_size) {
            if (ioctl(fd, BLKSSZGET, sector_size) < 0)
                return -1;
            return 0;
        }
    #else
        int blkdev_get_sector_size(int fd, int *sector_size) {
            (void)fd;
            *sector_size = DEFAULT_SECTOR_SIZE;
            return 0;
        }
    #endif

    #ifdef BLKGETSIZE64
        int blkdev_get_size(int fd, u64 *bytes) {
            if (ioctl(fd, BLKGETSIZE64, bytes) < 0)
                return -1;
            return 0;
        }
    #else
        int blkdev_get_size(int fd, u64 *bytes) {
            struct stat st;

            if (fstat(fd, &st) < 0)
                return -1;

            if (st.st_size == 0) {
                // try BLKGETSIZE
                unsigned long long bytes64;
                if (ioctl(fd, BLKGETSIZE, &bytes64) >= 0) {
                    *bytes = bytes64;
                    return 0;
                }
            }

            *bytes = st.st_size;
            return 0;
        }
    #endif
#elif defined(OS_MACOS)
    int blkdev_get_sector_size(int fd, int *sector_size) {
        if (ioctl(fd, DKIOCGETBLOCKSIZE, sector_size) >= 0)
            return 0;
        return -1;
    }

    int blkdev_get_size(int fd, u64 *bytes) {
        int sectorSize = 0;
        if (blkdev_get_sector_size(fd, &sectorSize) < 0)
            return -1;

        if (ioctl(fd, DKIOCGETBLOCKCOUNT, bytes) < 0)
            return -1;

        *bytes *= sectorSize;

        return 0;
    }
#endif

    bool DiskProvider::open() {
        this->m_readable = true;
        this->m_writable = true;

#if defined(OS_WINDOWS)

        const auto &path = this->m_path.native();

            this->m_diskHandle = reinterpret_cast<HANDLE>(CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
            if (this->m_diskHandle == INVALID_HANDLE_VALUE) {
                this->m_diskHandle = reinterpret_cast<HANDLE>(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
                this->m_writable   = false;

                if (this->m_diskHandle == INVALID_HANDLE_VALUE)
                    return false;
            }

            {
                DISK_GEOMETRY_EX diskGeometry = { };
                DWORD bytesRead               = 0;
                if (DeviceIoControl(
                        this->m_diskHandle,
                        IOCTL_DISK_GET_DRIVE_GEOMETRY_EX,
                        nullptr,
                        0,
                        &diskGeometry,
                        sizeof(DISK_GEOMETRY_EX),
                        &bytesRead,
                        nullptr)) {
                    this->m_diskSize   = diskGeometry.DiskSize.QuadPart;
                    this->m_sectorSize = diskGeometry.Geometry.BytesPerSector;
                    this->m_sectorBuffer.resize(this->m_sectorSize);
                }
            }

            if (this->m_diskHandle == nullptr || this->m_diskHandle == INVALID_HANDLE_VALUE) {
                this->m_readable   = false;
                this->m_diskHandle = nullptr;
                CloseHandle(this->m_diskHandle);

                return false;
            }

#else

        const auto &path = this->m_path.native();

        this->m_diskHandle = ::open(path.c_str(), O_RDWR);
        if (this->m_diskHandle == -1) {
            this->setErrorMessage(hex::format("hex.builtin.provider.disk.error.read_rw"_lang, path, ::strerror(errno)));
            log::warn(this->getErrorMessage());
            this->m_diskHandle = ::open(path.c_str(), O_RDONLY);
            this->m_writable   = false;
        }

        if (this->m_diskHandle == -1) {
            this->setErrorMessage(hex::format("hex.builtin.provider.disk.error.read_ro"_lang, path, ::strerror(errno)));
            log::warn(this->getErrorMessage());
            this->m_readable = false;
            return false;
        }

        u64 diskSize = 0;
        blkdev_get_size(this->m_diskHandle, &diskSize);
        this->m_diskSize = diskSize;
        blkdev_get_sector_size(this->m_diskHandle, reinterpret_cast<int *>(&this->m_sectorSize));

        this->m_sectorBuffer.resize(this->m_sectorSize);

#endif

        return true;
    }

    void DiskProvider::close() {
#if defined(OS_WINDOWS)

        if (this->m_diskHandle != INVALID_HANDLE_VALUE)
                ::CloseHandle(this->m_diskHandle);

            this->m_diskHandle = INVALID_HANDLE_VALUE;

#else

        if (this->m_diskHandle != -1)
            ::close(this->m_diskHandle);

        this->m_diskHandle = -1;

#endif
    }

    void DiskProvider::readRaw(u64 offset, void *buffer, size_t size) {
#if defined(OS_WINDOWS)

        DWORD bytesRead = 0;

            u64 startOffset = offset;

            while (size > 0) {
                LARGE_INTEGER seekPosition;
                seekPosition.LowPart  = (offset & 0xFFFF'FFFF) - (offset % this->m_sectorSize);
                seekPosition.HighPart = offset >> 32;

                if (this->m_sectorBufferAddress != static_cast<u64>(seekPosition.QuadPart)) {
                    ::SetFilePointer(this->m_diskHandle, seekPosition.LowPart, &seekPosition.HighPart, FILE_BEGIN);
                    ::ReadFile(this->m_diskHandle, this->m_sectorBuffer.data(), this->m_sectorBuffer.size(), &bytesRead, nullptr);
                    this->m_sectorBufferAddress = seekPosition.QuadPart;
                }

                std::memcpy(reinterpret_cast<u8 *>(buffer) + (offset - startOffset), this->m_sectorBuffer.data() + (offset & (this->m_sectorSize - 1)), std::min(this->m_sectorSize, size));

                size = std::max<ssize_t>(static_cast<ssize_t>(size) - this->m_sectorSize, 0);
                offset += this->m_sectorSize;
            }

#else

        u64 startOffset    = offset;

        while (size > 0) {
            u64 seekPosition = offset - (offset % this->m_sectorSize);

            if (this->m_sectorBufferAddress != seekPosition || this->m_sectorBufferAddress == 0) {
                ::lseek(this->m_diskHandle, seekPosition, SEEK_SET);
                if (::read(this->m_diskHandle, this->m_sectorBuffer.data(), this->m_sectorBuffer.size()) == -1)
                    break;

                this->m_sectorBufferAddress = seekPosition;
            }

            std::memcpy(reinterpret_cast<u8 *>(buffer) + (offset - startOffset),
                        this->m_sectorBuffer.data() + (offset & (this->m_sectorSize - 1)),
                        std::min(this->m_sectorSize, size));

            size = std::max<ssize_t>(static_cast<ssize_t>(size) - this->m_sectorSize, 0);
            offset += this->m_sectorSize;
        }

#endif
    }

    void DiskProvider::writeRaw(u64 offset, const void *buffer, size_t size) {
#if defined(OS_WINDOWS)

        DWORD bytesWritten = 0;

            u64 startOffset = offset;

            std::vector<u8> modifiedSectorBuffer;
            modifiedSectorBuffer.resize(this->m_sectorSize);

            while (size > 0) {
                u64 sectorBase  = offset - (offset % this->m_sectorSize);
                size_t currSize = std::min(size, this->m_sectorSize);

                this->readRaw(sectorBase, modifiedSectorBuffer.data(), modifiedSectorBuffer.size());
                std::memcpy(modifiedSectorBuffer.data() + ((offset - sectorBase) % this->m_sectorSize), reinterpret_cast<const u8 *>(buffer) + (startOffset - offset), currSize);

                LARGE_INTEGER seekPosition;
                seekPosition.LowPart  = (offset & 0xFFFF'FFFF) - (offset % this->m_sectorSize);
                seekPosition.HighPart = offset >> 32;

                ::SetFilePointer(this->m_diskHandle, seekPosition.LowPart, &seekPosition.HighPart, FILE_BEGIN);
                ::WriteFile(this->m_diskHandle, modifiedSectorBuffer.data(), modifiedSectorBuffer.size(), &bytesWritten, nullptr);

                offset += currSize;
                size -= currSize;
            }

#else

        u64 startOffset = offset;

        std::vector<u8> modifiedSectorBuffer;
        modifiedSectorBuffer.resize(this->m_sectorSize);

        while (size > 0) {
            u64 sectorBase  = offset - (offset % this->m_sectorSize);
            size_t currSize = std::min(size, this->m_sectorSize);

            this->readRaw(sectorBase, modifiedSectorBuffer.data(), modifiedSectorBuffer.size());
            std::memcpy(modifiedSectorBuffer.data() + ((offset - sectorBase) % this->m_sectorSize), reinterpret_cast<const u8 *>(buffer) + (startOffset - offset), currSize);

            ::lseek(this->m_diskHandle, sectorBase, SEEK_SET);
            if (::write(this->m_diskHandle, modifiedSectorBuffer.data(), modifiedSectorBuffer.size()) < 0)
                break;

            offset += currSize;
            size -= currSize;
        }

#endif
    }

    size_t DiskProvider::getActualSize() const {
        return this->m_diskSize;
    }

    std::string DiskProvider::getName() const {
        return wolv::util::toUTF8String(this->m_path);
    }

    std::vector<DiskProvider::Description> DiskProvider::getDataDescription() const {
        return {
                { "hex.builtin.provider.disk.selected_disk"_lang, wolv::util::toUTF8String(this->m_path)       },
                { "hex.builtin.provider.disk.disk_size"_lang,     hex::toByteString(this->m_diskSize)    },
                { "hex.builtin.provider.disk.sector_size"_lang,   hex::toByteString(this->m_sectorSize)  }
        };
    }


    void DiskProvider::reloadDrives() {
#if defined(OS_WINDOWS)

        this->m_availableDrives.clear();
            std::bitset<32> drives = ::GetLogicalDrives();
            for (char i = 0; i < 26; i++) {
                if (drives[i])
                    this->m_availableDrives.insert(hex::format(R"(\\.\{:c}:)", 'A' + i));
            }

            auto logicalDrives = this->m_availableDrives;
            for (const auto &drive : logicalDrives) {
                auto handle = reinterpret_cast<HANDLE>(::CreateFile(drive.data(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr));

                if (handle == INVALID_HANDLE_VALUE) continue;

                VOLUME_DISK_EXTENTS diskExtents = { };
                DWORD bytesRead                 = 0;
                auto result                     = ::DeviceIoControl(
                    handle,
                    IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
                    nullptr,
                    0,
                    &diskExtents,
                    sizeof(VOLUME_DISK_EXTENTS),
                    &bytesRead,
                    nullptr);

                if (result) {
                    auto diskPath = hex::format(R"(\\.\PhysicalDrive{})", diskExtents.Extents[0].DiskNumber);
                    this->m_availableDrives.insert(diskPath);
                }

                ::CloseHandle(handle);
            }

#endif
    }

    bool DiskProvider::drawLoadInterface() {
#if defined(OS_WINDOWS)

        if (this->m_availableDrives.empty())
                this->reloadDrives();

            if (ImGui::BeginListBox("hex.builtin.provider.disk.selected_disk"_lang)) {

                for (const auto &drive : this->m_availableDrives) {
                    if (ImGui::Selectable(drive.c_str(), this->m_path == drive))
                        this->m_path = drive;
                }

                ImGui::EndListBox();
            }

            ImGui::SameLine();

            if (ImGui::Button("hex.builtin.provider.disk.reload"_lang)) {
                this->reloadDrives();
        }

#else

        if (ImGui::InputText("hex.builtin.provider.disk.selected_disk"_lang, this->m_pathBuffer.data(), this->m_pathBuffer.size(), ImGuiInputTextFlags_CallbackResize, ImGui::UpdateStringSizeCallback, &this->m_pathBuffer))
            this->m_path = this->m_pathBuffer;

#endif

        return !this->m_path.empty();
    }

    nlohmann::json DiskProvider::storeSettings(nlohmann::json settings) const {
        settings["path"] = wolv::util::toUTF8String(this->m_path);

        return Provider::storeSettings(settings);
    }

    void DiskProvider::loadSettings(const nlohmann::json &settings) {
        Provider::loadSettings(settings);

        auto path = settings.at("path").get<std::string>();
        this->setPath(std::u8string(path.begin(), path.end()));
        this->reloadDrives();
    }

    std::pair<Region, bool> DiskProvider::getRegionValidity(u64 address) const {
        address -= this->getBaseAddress();

        if (address < this->getActualSize())
            return { Region { this->getBaseAddress() + address, this->getActualSize() - address }, true };
        else
            return { Region::Invalid(), false };
    }

    std::variant<std::string, i128> DiskProvider::queryInformation(const std::string &category, const std::string &argument) {
        if (category == "file_path")
            return wolv::util::toUTF8String(this->m_path);
        else if (category == "sector_size")
            return this->m_sectorSize;
        else
            return Provider::queryInformation(category, argument);
    }

}