/**
 * A simple utility to strip version resources from Windows executables.
 *
 * Usage: version_stripper <input path> <output path>
 *
 * This program copies the input executable to the output path and removes
 * its version resource information.
 *
 * Based on https://github.com/shewitt-au/nuke_version
 */

#include <filesystem>
#include <tchar.h>
#include <windows.h>
#include <string>
#include <stdexcept>
#include <vector>

using LangIds = std::vector<WORD>;

BOOL CALLBACK EnumResLangProc(
    HMODULE  hModule,
    LPCTSTR  lpszType,
    LPCTSTR  lpszName,
    WORD     wIDLanguage,
    LONG_PTR lParam
) {
    auto& langs = *reinterpret_cast<LangIds*>(lParam);
    langs.push_back(wIDLanguage);
    return true;
}

LangIds getLangIDs(LPCTSTR pExe) {
    LangIds langs;

    HMODULE hMod = LoadLibrary(pExe);
    if (hMod == nullptr)
        throw std::runtime_error("LoadLibrary failed!");

    BOOL bOK = EnumResourceLanguages(
        hMod,                                   // HMODULE hModule
        RT_VERSION,                             // LPCTSTR lpType
        MAKEINTRESOURCE(1),                     // LPCTSTR lpName
        EnumResLangProc,                        // ENUMRESLANGPROC lpEnumFunc
        reinterpret_cast<LONG_PTR>(&langs)      // LONG_PTR lParam
    );

    FreeLibrary(hMod);

    if (!bOK)
        throw std::runtime_error("EnumResourceLanguages failed!");

    return langs;
}

void nukeVersionResource(LPCTSTR pExe) {
    LangIds langs = getLangIDs(pExe);

    HANDLE hResUpdate = BeginUpdateResource(pExe, FALSE);
    if (hResUpdate == nullptr)
        throw std::runtime_error("BeginUpdateResource failed!");

    for (WORD langID : langs) {
        BOOL bOK = UpdateResource(
            hResUpdate,         // HANDLE hUpdate
            RT_VERSION,         // LPCTSTR lpType
            MAKEINTRESOURCE(1), // LPCTSTR lpName
            langID,             // WORD wLanguage
            nullptr,            // LPVOID lpData
            0                   // DWORD cb
        );

        if (!bOK) {
            EndUpdateResource(
                hResUpdate, // HANDLE hUpdate
                TRUE        // BOOL fDiscard
            );

            throw std::runtime_error("UpdateResource failed! Nothing done!");
        }

    }

    EndUpdateResource(
        hResUpdate, // HANDLE hUpdate
        FALSE       // BOOL fDiscard
    );
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Usage: %s <input path> <output path>\n", argv[0]);
        return 1;
    }

    std::filesystem::path inputPath(argv[1]);
    std::filesystem::path outputPath(argv[2]);

    std::filesystem::copy(inputPath, outputPath);

    try {
        nukeVersionResource(outputPath.c_str());
    } catch (const std::exception& e) {
        fprintf(stderr, "%s", e.what());
        std::filesystem::remove(outputPath);
        return 1;
    }

    return 0;
}