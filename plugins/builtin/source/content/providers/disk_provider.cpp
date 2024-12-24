#if !defined(OS_WEB)


#include "content/providers/disk_provider.hpp"

#include <hex/api/localization_manager.hpp>

#include <hex/helpers/logger.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/ui/imgui_imhex_extensions.h>

#include <wolv/utils/string.hpp>

#include <bitset>
#include <filesystem>

#include <imgui.h>
#include <fonts/vscode_icons.hpp>

#include <nlohmann/json.hpp>

#if defined(OS_WINDOWS)
    #include <windows.h>
    #include <winioctl.h>
    #include <setupapi.h>
    #include <cfgmgr32.h>
#elif defined(OS_LINUX)
    #include <fcntl.h>
    #include <unistd.h>
    #if !defined(OS_FREEBSD)
        #include <linux/fs.h>
    #endif
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

#if defined(OS_LINUX) && !defined(OS_FREEBSD)
    #define lseek lseek64
#elif defined(OS_FREEBSD)
    #include <sys/disk.h>
    #define DEFAULT_SECTOR_SIZE 512
#endif

namespace hex::plugin::builtin {

    bool DiskProvider::isAvailable() const {
        #if defined(OS_WINDOWS)
            return m_diskHandle != INVALID_HANDLE_VALUE;
        #else
            return m_diskHandle != -1;
        #endif
    }

    bool DiskProvider::isReadable() const {
        return m_readable;
    }

    bool DiskProvider::isWritable() const {
        return m_writable;
    }

    bool DiskProvider::isResizable() const {
        return false;
    }

    bool DiskProvider::isSavable() const {
        return false;
    }


    void DiskProvider::setPath(const std::fs::path &path) {
        m_path = path;
    }

#if defined (OS_LINUX)
    #ifdef BLKSSZGET
        int blkdev_get_sector_size(int fd, int *sector_size) {
            if (ioctl(fd, BLKSSZGET, sector_size) < 0)
                return -1;
            return 0;
        }
    #elif defined(OS_FREEBSD) && defined(DIOCGSECTORSIZE)
        int blkdev_get_sector_size(int fd, int *sector_size) {
            if (ioctl(fd, DIOCGSECTORSIZE, sector_size) < 0)
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
    #elif defined(OS_FREEBSD) && defined(DIOCGMEDIASIZE)
        int blkdev_get_size(int fd, u64 *bytes) {
            if (ioctl(fd, DIOCGMEDIASIZE, bytes) < 0)
                return -1;
            return 0;
        }
    #else
        int blkdev_get_size(int fd, u64 *bytes) {
            struct stat st;

            if (fstat(fd, &st) < 0)
                return -1;

            if (st.st_size == 0) {
                // Try BLKGETSIZE
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
        m_readable = true;
        m_writable = true;

        #if defined(OS_WINDOWS)

            const auto &path = m_path.native();

            m_diskHandle = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (m_diskHandle == INVALID_HANDLE_VALUE) {
                m_diskHandle = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
                m_writable   = false;

                if (m_diskHandle == INVALID_HANDLE_VALUE) {
                    this->setErrorMessage(hex::formatSystemError(::GetLastError()));
                    return false;
                }
            }

            {
                DISK_GEOMETRY_EX diskGeometry = { };
                DWORD bytesRead               = 0;
                if (DeviceIoControl(
                        m_diskHandle,
                        IOCTL_DISK_GET_DRIVE_GEOMETRY_EX,
                        nullptr,
                        0,
                        &diskGeometry,
                        sizeof(DISK_GEOMETRY_EX),
                        &bytesRead,
                        nullptr)) {
                    m_diskSize   = diskGeometry.DiskSize.QuadPart;
                    m_sectorSize = diskGeometry.Geometry.BytesPerSector;
                    m_sectorBuffer.resize(m_sectorSize);
                }
            }

            if (m_diskHandle == nullptr || m_diskHandle == INVALID_HANDLE_VALUE) {
                this->setErrorMessage(hex::formatSystemError(::GetLastError()));
                m_readable   = false;
                m_diskHandle = nullptr;
                CloseHandle(m_diskHandle);

                return false;
            }

        #else

            const auto &path = m_path.native();

            m_diskHandle = ::open(path.c_str(), O_RDWR);
            if (m_diskHandle == -1) {
                this->setErrorMessage(hex::format("hex.builtin.provider.disk.error.read_rw"_lang, path, std::system_category().message(errno)));
                log::warn(this->getErrorMessage());
                m_diskHandle = ::open(path.c_str(), O_RDONLY);
                m_writable   = false;
            }

            if (m_diskHandle == -1) {
                this->setErrorMessage(hex::format("hex.builtin.provider.disk.error.read_ro"_lang, path, std::system_category().message(errno)));
                log::warn(this->getErrorMessage());
                m_readable = false;
                return false;
            }

            u64 diskSize = 0;
            blkdev_get_size(m_diskHandle, &diskSize);
            m_diskSize = diskSize;
            blkdev_get_sector_size(m_diskHandle, reinterpret_cast<int *>(&m_sectorSize));

            m_sectorBuffer.resize(m_sectorSize);

        #endif

        return true;
    }

    void DiskProvider::close() {
        #if defined(OS_WINDOWS)

            if (m_diskHandle != INVALID_HANDLE_VALUE)
                ::CloseHandle(m_diskHandle);

            m_diskHandle = INVALID_HANDLE_VALUE;

        #else

            if (m_diskHandle != -1)
                ::close(m_diskHandle);

            m_diskHandle = -1;

        #endif
    }

    void DiskProvider::readRaw(u64 offset, void *buffer, size_t size) {
        #if defined(OS_WINDOWS)

            DWORD bytesRead = 0;

            u64 startOffset = offset;

            while (size > 0) {
                LARGE_INTEGER seekPosition;
                seekPosition.LowPart  = (offset & 0xFFFF'FFFF) - (offset % m_sectorSize);
                seekPosition.HighPart = LONG(offset >> 32);

                if (m_sectorBufferAddress != static_cast<u64>(seekPosition.QuadPart)) {
                    ::SetFilePointer(m_diskHandle, seekPosition.LowPart, &seekPosition.HighPart, FILE_BEGIN);
                    ::ReadFile(m_diskHandle, m_sectorBuffer.data(), m_sectorBuffer.size(), &bytesRead, nullptr);
                    m_sectorBufferAddress = seekPosition.QuadPart;
                }

                std::memcpy(static_cast<u8 *>(buffer) + (offset - startOffset), m_sectorBuffer.data() + (offset & (m_sectorSize - 1)), std::min<u64>(m_sectorSize, size));

                size = std::max<ssize_t>(static_cast<ssize_t>(size) - m_sectorSize, 0);
                offset += m_sectorSize;
            }

        #else

            u64 startOffset    = offset;

            while (size > 0) {
                u64 seekPosition = offset - (offset % m_sectorSize);

                if (m_sectorBufferAddress != seekPosition || m_sectorBufferAddress == 0) {
                    ::lseek(m_diskHandle, seekPosition, SEEK_SET);
                    if (::read(m_diskHandle, m_sectorBuffer.data(), m_sectorBuffer.size()) == -1)
                        break;

                    m_sectorBufferAddress = seekPosition;
                }

                std::memcpy(reinterpret_cast<u8 *>(buffer) + (offset - startOffset),
                            m_sectorBuffer.data() + (offset & (m_sectorSize - 1)),
                            std::min<u64>(m_sectorSize, size));

                size = std::max<ssize_t>(static_cast<ssize_t>(size) - m_sectorSize, 0);
                offset += m_sectorSize;
            }

        #endif
    }

    void DiskProvider::writeRaw(u64 offset, const void *buffer, size_t size) {
        #if defined(OS_WINDOWS)

            DWORD bytesWritten = 0;

            u64 startOffset = offset;

            std::vector<u8> modifiedSectorBuffer;
            modifiedSectorBuffer.resize(m_sectorSize);

            while (size > 0) {
                u64 sectorBase  = offset - (offset % m_sectorSize);
                size_t currSize = std::min<u64>(size, m_sectorSize);

                this->readRaw(sectorBase, modifiedSectorBuffer.data(), modifiedSectorBuffer.size());
                std::memcpy(modifiedSectorBuffer.data() + ((offset - sectorBase) % m_sectorSize), static_cast<const u8 *>(buffer) + (startOffset - offset), currSize);

                LARGE_INTEGER seekPosition;
                seekPosition.LowPart  = (offset & 0xFFFF'FFFF) - (offset % m_sectorSize);
                seekPosition.HighPart = offset >> 32;

                ::SetFilePointer(m_diskHandle, seekPosition.LowPart, &seekPosition.HighPart, FILE_BEGIN);
                ::WriteFile(m_diskHandle, modifiedSectorBuffer.data(), modifiedSectorBuffer.size(), &bytesWritten, nullptr);

                //Print last error
                log::error("{}", hex::formatSystemError(::GetLastError()));

                offset += currSize;
                size -= currSize;
            }

        #else

            u64 startOffset = offset;

            std::vector<u8> modifiedSectorBuffer;
            modifiedSectorBuffer.resize(m_sectorSize);

            while (size > 0) {
                u64 sectorBase  = offset - (offset % m_sectorSize);
                size_t currSize = std::min<u64>(size, m_sectorSize);

                this->readRaw(sectorBase, modifiedSectorBuffer.data(), modifiedSectorBuffer.size());
                std::memcpy(modifiedSectorBuffer.data() + ((offset - sectorBase) % m_sectorSize), reinterpret_cast<const u8 *>(buffer) + (startOffset - offset), currSize);

                ::lseek(m_diskHandle, sectorBase, SEEK_SET);
                if (::write(m_diskHandle, modifiedSectorBuffer.data(), modifiedSectorBuffer.size()) < 0)
                    break;

                offset += currSize;
                size -= currSize;
            }

        #endif
    }

    u64 DiskProvider::getActualSize() const {
        return m_diskSize;
    }

    std::string DiskProvider::getName() const {
        if (m_friendlyName.empty())
            return wolv::util::toUTF8String(m_path);
        else
            return m_friendlyName;
    }

    std::vector<DiskProvider::Description> DiskProvider::getDataDescription() const {
        return {
            { "hex.builtin.provider.disk.selected_disk"_lang, wolv::util::toUTF8String(m_path) },
            { "hex.builtin.provider.disk.disk_size"_lang,     hex::toByteString(m_diskSize)    },
            { "hex.builtin.provider.disk.sector_size"_lang,   hex::toByteString(m_sectorSize)  }
        };
    }


    void DiskProvider::reloadDrives() {
#if defined(OS_WINDOWS)

        m_availableDrives.clear();

        std::array<WCHAR, MAX_DEVICE_ID_LEN> deviceInstanceId = {};
        std::array<WCHAR, 1024> description = {};

        const GUID hddClass = GUID_DEVINTERFACE_DISK;

        HDEVINFO hDevInfo = SetupDiGetClassDevs(&hddClass, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
        if (hDevInfo == INVALID_HANDLE_VALUE)
            return;

        // Add all physical drives
        for (u32 i = 0; ; i++) {
            SP_DEVINFO_DATA deviceInfoData;
            deviceInfoData.cbSize = sizeof(deviceInfoData);

            if (SetupDiEnumDeviceInfo(hDevInfo, i, &deviceInfoData) == FALSE)
                break;

            SP_DEVICE_INTERFACE_DATA interfaceData;
            interfaceData.cbSize = sizeof(SP_INTERFACE_DEVICE_DATA);

            if (!SetupDiEnumInterfaceDevice(hDevInfo, nullptr, &hddClass, i, &interfaceData))
                break;

            if (CM_Get_Device_IDW(deviceInfoData.DevInst, deviceInstanceId.data(), MAX_PATH, 0) != CR_SUCCESS)
                continue;

            // Get the required size of the device path
            DWORD requiredSize = 0;
            SetupDiGetDeviceInterfaceDetail(hDevInfo, &interfaceData, nullptr, 0, &requiredSize, nullptr);

            // Query the device path
            std::vector<u8> dataBuffer(requiredSize);
            auto data = reinterpret_cast<SP_INTERFACE_DEVICE_DETAIL_DATA*>(dataBuffer.data());
            data->cbSize = sizeof(SP_INTERFACE_DEVICE_DETAIL_DATA);

            if (!SetupDiGetDeviceInterfaceDetail(hDevInfo, &interfaceData, data, requiredSize, nullptr, nullptr))
                continue;

            auto path = reinterpret_cast<const WCHAR*>(data->DevicePath);

            // Query the friendly name of the device
            DWORD size = 0;
            DWORD propertyRegDataType = SPDRP_PHYSICAL_DEVICE_OBJECT_NAME;
            SetupDiGetDeviceRegistryPropertyW(hDevInfo, &deviceInfoData, SPDRP_FRIENDLYNAME,
                                             &propertyRegDataType, reinterpret_cast<BYTE*>(description.data()),
                                             sizeof(description),
                                             &size);

            auto friendlyName = description.data();

            m_availableDrives.insert({ utf16ToUtf8(path), utf16ToUtf8(friendlyName) });
        }

        // Add all logical drives
        std::bitset<32> drives = ::GetLogicalDrives();
        for (char i = 0; i < 26; i++) {
            if (drives[i]) {
                char letter = 'A' + i;
                m_availableDrives.insert({ hex::format(R"(\\.\{:c}:)", letter), hex::format(R"({:c}:/)", letter) });
            }
        }

#endif
    }

    bool DiskProvider::drawLoadInterface() {
        #if defined(OS_WINDOWS)

            if (m_availableDrives.empty()) {
                this->reloadDrives();
                m_elevated = hex::isProcessElevated();
            }

            if (!m_elevated) {
                ImGui::PushTextWrapPos(0);
                ImGuiExt::TextFormattedColored(ImGuiExt::GetCustomColorU32(ImGuiCustomCol_LoggerError), ICON_VS_SHIELD "{}", "hex.builtin.provider.disk.elevation"_lang);
                ImGui::PopTextWrapPos();
                ImGui::NewLine();
            }

            ImGui::PushItemWidth(300_scaled);
            if (ImGui::BeginListBox("hex.builtin.provider.disk.selected_disk"_lang)) {
                ImGui::PushID(1);
                for (const auto &[path, friendlyName] : m_availableDrives) {
                    if (ImGui::Selectable(friendlyName.c_str(), m_path == path)) {
                        m_path = path;
                        m_friendlyName = friendlyName;
                    }

                    ImGuiExt::InfoTooltip(path.c_str());
                }
                ImGui::PopID();

                ImGui::EndListBox();
            }
            ImGui::PopItemWidth();

            ImGui::SameLine();

            if (ImGui::Button("hex.builtin.provider.disk.reload"_lang)) {
                this->reloadDrives();
        }

        #else

            if (ImGui::InputText("hex.builtin.provider.disk.selected_disk"_lang, m_pathBuffer.data(), m_pathBuffer.size(), ImGuiInputTextFlags_CallbackResize, ImGuiExt::UpdateStringSizeCallback, &m_pathBuffer)) {
                m_path = m_pathBuffer;
                m_friendlyName = m_pathBuffer;
            }

        #endif

        return !m_path.empty();
    }

    nlohmann::json DiskProvider::storeSettings(nlohmann::json settings) const {
        settings["path"] = wolv::io::fs::toNormalizedPathString(m_path);

        settings["friendly_name"] = m_friendlyName;

        return Provider::storeSettings(settings);
    }

    void DiskProvider::loadSettings(const nlohmann::json &settings) {
        Provider::loadSettings(settings);

        auto path = settings.at("path").get<std::string>();

        if (settings.contains("friendly_name"))
            m_friendlyName = settings.at("friendly_name").get<std::string>();

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
            return wolv::io::fs::toNormalizedPathString(m_path);
        else if (category == "sector_size")
            return m_sectorSize;
        else if (category == "friendly_name")
            return m_friendlyName;
        else
            return Provider::queryInformation(category, argument);
    }

}
#endif