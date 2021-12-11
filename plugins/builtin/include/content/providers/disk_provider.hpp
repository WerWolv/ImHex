#pragma once

#include <hex/providers/provider.hpp>

#include "content/providers/file_provider.hpp"

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

        void setPath(const std::string &path);

        [[nodiscard]] bool open() override;
        void close() override;

        [[nodiscard]] virtual std::string getName() const;
        [[nodiscard]] virtual std::vector<std::pair<std::string, std::string>> getDataInformation() const;

        [[nodiscard]] bool hasLoadInterface() const override { return true; }
        void drawLoadInterface() override;
    protected:
        std::set<std::string> m_availableDrives;
        std::string m_path;

        #if defined(OS_WINDOWS)
            HANDLE m_diskHandle = INVALID_HANDLE_VALUE;
        #else
            int m_diskHandle = -1;
        #endif

        size_t m_diskSize;
        size_t m_sectorSize;

        u64 m_sectorBufferAddress;
        std::vector<u8> m_sectorBuffer;

        bool m_readable = false;
        bool m_writable = false;
    };

}