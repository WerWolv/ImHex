#if !defined(OS_WEB)


#include "content/providers/disk_provider.hpp"

#include <hex/api/localization_manager.hpp>

#include <hex/helpers/logger.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/helpers/scaling.hpp>
#include <hex/ui/imgui_imhex_extensions.h>

#include <wolv/utils/string.hpp>

#include <bitset>
#include <filesystem>

#include <imgui.h>
#include <mutex>
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

    prv::Provider::OpenResult DiskProvider::open() {
        OpenResult result;
        m_readable = true;
        m_writable = true;

        #if defined(OS_WINDOWS)

            const auto &path = m_path.native();

            m_diskHandle = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr);
            if (m_diskHandle == INVALID_HANDLE_VALUE) {
                m_diskHandle = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr);
                m_writable   = false;

                if (m_diskHandle == INVALID_HANDLE_VALUE) {
                    return OpenResult::failure(hex::formatSystemError(::GetLastError()));
                }
            }

            {
                DISK_GEOMETRY_EX diskGeometry = { };
                DWORD bytesRead               = 0;
                OVERLAPPED operation           = { };
                operation.hEvent               = ::CreateEvent(nullptr, TRUE, FALSE, nullptr);

                auto success = operation.hEvent != nullptr && DeviceIoControl(
                        m_diskHandle,
                        IOCTL_DISK_GET_DRIVE_GEOMETRY_EX,
                        nullptr,
                        0,
                        &diskGeometry,
                        sizeof(DISK_GEOMETRY_EX),
                        &bytesRead,
                        &operation);
                if (!success && operation.hEvent != nullptr && ::GetLastError() == ERROR_IO_PENDING)
                    success = ::GetOverlappedResult(m_diskHandle, &operation, &bytesRead, TRUE);

                if (operation.hEvent != nullptr)
                    ::CloseHandle(operation.hEvent);

                if (success) {
                    m_diskSize   = diskGeometry.DiskSize.QuadPart;
                    m_sectorSize = diskGeometry.Geometry.BytesPerSector;
                }
            }

            if (m_diskHandle == nullptr || m_diskHandle == INVALID_HANDLE_VALUE) {
                auto error = ::GetLastError();
                m_readable   = false;
                m_diskHandle = nullptr;
                CloseHandle(m_diskHandle);

                return OpenResult::failure(hex::formatSystemError(error));
            }

        #else

            const auto &path = m_path.native();

            m_diskHandle = ::open(path.c_str(), O_RDWR);
            if (m_diskHandle == -1) {
                result = OpenResult::warning(fmt::format("hex.builtin.provider.disk.error.read_rw"_lang, path, formatSystemError(errno)));
                m_diskHandle = ::open(path.c_str(), O_RDONLY);
                m_writable   = false;
            }

            if (m_diskHandle == -1) {
                m_readable = false;
                return OpenResult::failure(fmt::format("hex.builtin.provider.disk.error.read_ro"_lang, path, formatSystemError(errno)));
            }

            u64 diskSize = 0;
            blkdev_get_size(m_diskHandle, &diskSize);
            m_diskSize = diskSize;
            blkdev_get_sector_size(m_diskHandle, reinterpret_cast<int *>(&m_sectorSize));

        #endif

        this->setCacheBlockSize(m_sectorSize);
        return result;
    }

    void DiskProvider::close() {
        CachedProvider::close();

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

    void DiskProvider::readFromSource(u64 offset, void *buffer, size_t size) {
        #if defined(OS_WINDOWS)
            OVERLAPPED operation = { };
            operation.Offset     = static_cast<DWORD>(offset & 0xFFFF'FFFF);
            operation.OffsetHigh = static_cast<DWORD>(offset >> 32);
            operation.hEvent     = ::CreateEvent(nullptr, TRUE, FALSE, nullptr);
            if (operation.hEvent == nullptr)
                return;

            DWORD bytesRead = 0;
            auto success = ::ReadFile(m_diskHandle, buffer, size, &bytesRead, &operation);
            if (!success && ::GetLastError() == ERROR_IO_PENDING)
                std::ignore = ::GetOverlappedResult(m_diskHandle, &operation, &bytesRead, TRUE);

            ::CloseHandle(operation.hEvent);

        #else
            std::ignore = ::pread(m_diskHandle, buffer, size, static_cast<off_t>(offset));

        #endif
    }

    void DiskProvider::writeToSource(u64 offset, const void *buffer, size_t size) {
        const auto sectorOffset = offset % m_sectorSize;
        const auto sectorBase   = offset - sectorOffset;
        std::vector<u8> sectorData(m_sectorSize);
        if (sectorOffset != 0 || size != m_sectorSize)
            this->readFromSource(sectorBase, sectorData.data(), sectorData.size());
        std::memcpy(sectorData.data() + sectorOffset, buffer, size);

        #if defined(OS_WINDOWS)
            OVERLAPPED operation = { };
            operation.Offset     = static_cast<DWORD>(sectorBase & 0xFFFF'FFFF);
            operation.OffsetHigh = static_cast<DWORD>(sectorBase >> 32);
            operation.hEvent     = ::CreateEvent(nullptr, TRUE, FALSE, nullptr);
            if (operation.hEvent == nullptr)
                return;

            DWORD bytesWritten = 0;
            auto success = ::WriteFile(m_diskHandle, sectorData.data(), sectorData.size(), &bytesWritten, &operation);
            if (!success && ::GetLastError() == ERROR_IO_PENDING)
                std::ignore = ::GetOverlappedResult(m_diskHandle, &operation, &bytesWritten, TRUE);

            ::CloseHandle(operation.hEvent);

        #else
            std::ignore = ::pwrite(m_diskHandle, sectorData.data(), sectorData.size(), static_cast<off_t>(sectorBase));

        #endif
    }

    u64 DiskProvider::getSourceSize() const {
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
                    m_availableDrives.insert({ fmt::format(R"(\\.\{:c}:)", letter), fmt::format(R"({:c}:/)", letter) });
                }
            }

        #else
            for (const auto &basePath : { "/dev", "/dev/mapper" }) {
                if (!std::fs::is_directory(basePath))
                    continue;

                for (const auto &path : std::fs::directory_iterator(basePath)) {
                    if (std::fs::is_block_file(path)) {
                        m_availableDrives.insert({ path.path().string(), path.path().string() });
                    }
                }
            }
        #endif
    }

    bool DiskProvider::drawLoadInterface() {
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
                for (const auto &[path, friendlyName] : m_availableDrives) {
                    ImGui::PushID(path.c_str());
                    if (ImGui::Selectable(friendlyName.c_str(), m_path == path)) {
                        m_path = path;
                        m_friendlyName = friendlyName;
                    }
                    ImGui::PopID();

                    ImGuiExt::InfoTooltip(path.c_str());
                }

                ImGui::EndListBox();
            }
            ImGui::PopItemWidth();

            ImGui::SameLine();

            if (ImGui::Button("hex.builtin.provider.disk.reload"_lang)) {
                this->reloadDrives();
        }

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
            return { Region { .address=this->getBaseAddress() + address, .size=this->getActualSize() - address }, true };
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
