#include "windows_api.hpp"

#include <cstring>
#include <psapi.h>

#include <helpers/utils.hpp>

namespace hex::plugin::win {

    std::vector<DWORD> getProcessIDs() {
        std::vector<DWORD> processIDs;

        DWORD validBytes = 0;
        DWORD entries = 1024;
        do {
            processIDs.resize(entries);
            entries *= 2;

            if (!EnumProcesses(processIDs.data(), processIDs.size() * sizeof(DWORD), &validBytes))
                return { };

        } while (validBytes == processIDs.size() * sizeof(DWORD));

        return processIDs;
    }

    std::string getProcessName(DWORD pid) {
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);

        TCHAR processName[MAX_PATH] = "< ??? >";

        if (hProcess != nullptr) {
            HMODULE hModule;
            DWORD validBytes = 0;

            if (EnumProcessModules(hProcess, &hModule, sizeof(HMODULE), &validBytes))
                GetModuleBaseName(hProcess, hModule, processName, sizeof(processName) / sizeof(TCHAR));
        }

        CloseHandle(hProcess);

        return processName;
    }

    std::string getProcessFilePath(DWORD pid) {
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);

        TCHAR processPath[MAX_PATH] = "...";

        if (hProcess != nullptr) {
            GetProcessImageFileName(hProcess, processPath, sizeof(processPath));
        }

        CloseHandle(hProcess);

        return processPath;
    }

    Icon getProcessIcon(DWORD pid) {
        WORD iconIndex = 0;
        HICON hIcon = ExtractAssociatedIconA(GetModuleHandle(nullptr), getProcessFilePath(pid).data(), &iconIndex);

        std::vector<u8> bitmap, maskBits;
        u32 width = 0, height = 0;

        if (hIcon != nullptr) {
            ICONINFO iconInfo;
            GetIconInfo(hIcon, &iconInfo);
            HBITMAP hDib = iconInfo.hbmColor;

            DIBSECTION ds;
            int nSizeDS = ::GetObject(hDib, sizeof(ds), &ds);
            if (sizeof(ds) != nSizeDS)
            {
                hDib = (HBITMAP)::CopyImage(iconInfo.hbmColor, IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
                nSizeDS = ::GetObject(hDib, sizeof(ds), &ds);
            }

            width = ds.dsBm.bmWidth;
            height = ds.dsBm.bmHeight;

            bitmap.resize(ds.dsBmih.biSizeImage);
            std::memcpy(bitmap.data(), ds.dsBm.bmBits, ds.dsBmih.biSizeImage);
            if (hDib && hDib != iconInfo.hbmColor)
                ::DeleteObject(hDib);
        }

        return { bitmap, width, height };
    }

}