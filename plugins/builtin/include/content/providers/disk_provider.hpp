#pragma once

#include <hex/providers/provider.hpp>

#include <set>
#include <string>
#include <vector>

#if defined(OS_WINDOWS)
    #include <windows.h>
#endif

namespace hex::plugin::builtin::prv {

    class DiskProvider : public hex::prv::Provider {
    public:
        DiskProvider();
        ~DiskProvider() override;

        [[nodiscard]] bool isAvailable() const override;
        [[nodiscard]] bool isReadable() const override;
        [[nodiscard]] bool isWritable() const override;
        [[nodiscard]] bool isResizable() const override;
        [[nodiscard]] bool isSavable() const override;

        void readRaw(u64 offset, void *buffer, size_t size) override;
        void writeRaw(u64 offset, const void *buffer, size_t size) override;
        [[nodiscard]] size_t getActualSize() const override;

        void setPath(const std::fs::path &path);

        [[nodiscard]] bool open() override;
        void close() override;

        [[nodiscard]] std::string getName() const override;
        [[nodiscard]] std::vector<std::pair<std::string, std::string>> getDataInformation() const override;

        [[nodiscard]] bool hasLoadInterface() const override { return true; }
        void drawLoadInterface() override;

    protected:
        void reloadDrives();

        std::set<std::string> m_availableDrives;
        std::fs::path m_path;

#if defined(OS_WINDOWS)
        HANDLE m_diskHandle = INVALID_HANDLE_VALUE;
#else
        std::string m_pathBuffer;
        int m_diskHandle = -1;
#endif

        size_t m_diskSize   = 0;
        size_t m_sectorSize = 0;

        u64 m_sectorBufferAddress = 0;
        std::vector<u8> m_sectorBuffer;

        bool m_readable = false;
        bool m_writable = false;
    };

}