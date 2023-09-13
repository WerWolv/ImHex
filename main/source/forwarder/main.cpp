#include <windows.h>
#include <wolv/io/fs.hpp>

int main() {
    HWND consoleWindow = ::GetConsoleWindow();
    DWORD processId = 0;
    ::GetWindowThreadProcessId(consoleWindow, &processId);
    if (GetCurrentProcessId() == processId) {
        FreeConsole();
    } else {
        auto hConsole = ::GetStdHandle(STD_OUTPUT_HANDLE);
        if (hConsole != INVALID_HANDLE_VALUE) {
            DWORD mode = 0;
            if (::GetConsoleMode(hConsole, &mode) == TRUE) {
                mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING | ENABLE_PROCESSED_OUTPUT;
                ::SetConsoleMode(hConsole, mode);
            }
        }
    }

    auto executablePath = wolv::io::fs::getExecutablePath();
    auto executableFullPath = executablePath->parent_path() / "imhex-gui.exe";
    
    PROCESS_INFORMATION process;
    STARTUPINFOW startupInfo;
    ZeroMemory(&process, sizeof(PROCESS_INFORMATION));
    ZeroMemory(&startupInfo, sizeof(STARTUPINFOW));
    startupInfo.cb = sizeof(STARTUPINFOW);

    if (CreateProcessW(executableFullPath.wstring().c_str(), GetCommandLineW(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startupInfo, &process) == FALSE) {
        auto error = GetLastError();

        wchar_t errorMessageString[1024];
        FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, error, 0, errorMessageString, 1024, nullptr);

        std::wstring errorMessage = L"Failed to start ImHex:\n\nError code: ";

        // Format error code to have 8 digits hex
        wchar_t errorCodeString[11];
        swprintf_s(errorCodeString, 11, L"0x%08X", error);

        errorMessage += errorCodeString;
        errorMessage += L"\n\n";
        errorMessage += errorMessageString;

        MessageBoxW(nullptr, errorMessage.c_str(), L"ImHex Forwarder", MB_OK | MB_ICONERROR);
        return 1;
    }

    WaitForSingleObject(process.hProcess, INFINITE);
    CloseHandle(process.hProcess);

    return 0;
}