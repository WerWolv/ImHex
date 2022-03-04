#include "content/providers/disk_provider.hpp"

#include <hex/api/localization.hpp>

#include <hex/helpers/fmt.hpp>
#include <hex/helpers/utils.hpp>

#include <bitset>
#include <filesystem>

#include <imgui.h>
#include <hex/ui/imgui_imhex_extensions.h>

#if defined(OS_LINUX)
    #include <fcntl.h>
    #include <unistd.h>
    #include <sys/stat.h>
    #include <sys/types.h>

    #define lseek lseek64
#elif defined(OS_MACOS)
    #include <fcntl.h>
    #include <unistd.h>
    #include <sys/stat.h>
    #include <sys/types.h>
#endif

namespace hex::plugin::builtin::prv {

    DiskProvider::DiskProvider() : Provider() {
        this->reloadDrives();
    }

    DiskProvider::~DiskProvider() {
        this->close();
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
            DISK_GEOMETRY_EX diskGeometry = { 0 };
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

        struct stat driveStat;

        ::stat(path.c_str(), &driveStat) == 0;
        this->m_diskSize   = driveStat.st_size;
        this->m_sectorSize = 0;

        this->m_diskHandle = ::open(path.c_str(), O_RDWR);
        if (this->m_diskHandle == -1) {
            this->m_diskHandle = ::open(path.c_str(), O_RDONLY);
            this->m_writable   = false;
        }

        if (this->m_diskHandle == -1) {
            this->m_readable = false;
            return false;
        }

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

            if (this->m_sectorBufferAddress != seekPosition.QuadPart) {
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

            if (this->m_sectorBufferAddress != seekPosition) {
                ::lseek(this->m_diskHandle, seekPosition, SEEK_SET);
                ::read(this->m_diskHandle, buffer, size);
                this->m_sectorBufferAddress = seekPosition;
            }

            std::memcpy(reinterpret_cast<u8 *>(buffer) + (offset - startOffset), this->m_sectorBuffer.data() + (offset & (this->m_sectorSize - 1)), std::min(this->m_sectorSize, size));

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
            ::write(this->m_diskHandle, modifiedSectorBuffer.data(), modifiedSectorBuffer.size());

            offset += currSize;
            size -= currSize;
        }

#endif
    }

    size_t DiskProvider::getActualSize() const {
        return this->m_diskSize;
    }

    std::string DiskProvider::getName() const {
        return this->m_path.string();
    }

    std::vector<std::pair<std::string, std::string>> DiskProvider::getDataInformation() const {
        return {
            {"hex.builtin.provider.disk.selected_disk"_lang, this->m_path.string()                },
            { "hex.builtin.provider.disk.disk_size"_lang,    hex::toByteString(this->m_diskSize)  },
            { "hex.builtin.provider.disk.sector_size"_lang,  hex::toByteString(this->m_sectorSize)}
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

            VOLUME_DISK_EXTENTS diskExtents = { 0 };
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

    void DiskProvider::drawLoadInterface() {
#if defined(OS_WINDOWS)
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

        if (ImGui::InputText("hex.builtin.provider.disk.selected_disk"_lang, this->m_pathBuffer))
            this->m_path = this->m_pathBuffer;

#endif
    }

}