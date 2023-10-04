/**
 * This is a simple forwarder that launches the main ImHex executable with the
 * same command line as the current process. The reason for this is that even
 * though ImHex is a GUI application in general, it also has a command line
 * interface that can be used to perform various tasks. The issue with this is
 * that kernel32 will automatically allocate a console for us which will float
 * around in the background if we launch ImHex from the explorer. This forwarder
 * will get rid of the console window if ImHex was launched from the explorer and
 * enables ANSI escape sequences if ImHex was launched from the command line.
 *
 * The main reason this is done in a separate executable is because we use FreeConsole()
 * to get rid of the console window. Due to bugs in older versions of Windows (Windows 10 and older)
 * this will also close the process's standard handles (stdin, stdout, stderr) in
 * a way that cannot be recovered from. This means that if we were to do this in the
 * main application, if any code would try to interact with these handles (duplicate them
 * or modify them in any way), the application would crash.
 *
 * None of this would be necessary if Windows had a third type of application (besides
 * console and GUI) that would act like a console application but would not allocate
 * a console window. This would allow us to have a single executable that would work
 * the same as on all other platforms. There are plans to add this to Windows in the
 * future, but it is not yet available. Also even if it was available, it would not
 * be available on older versions of Windows, so we would still need this forwarder
 */

#include <windows.h>
#include <wolv/io/fs.hpp>

#include <fmt/format.h>

void handleConsoleWindow() {
    HWND consoleWindow = ::GetConsoleWindow();
    DWORD processId = 0;
    ::GetWindowThreadProcessId(consoleWindow, &processId);

    // Check if ImHex was launched from the explorer or from the command line
    if (::GetCurrentProcessId() == processId) {
        // If it was launched from the explorer, kernel32 has allocated a console for us
        // Get rid of it to avoid having a useless console window floating around
        ::FreeConsole();
    } else {
        // If it was launched from the command line, enable ANSI escape sequences to have colored output
        auto hConsole = ::GetStdHandle(STD_OUTPUT_HANDLE);
        if (hConsole != INVALID_HANDLE_VALUE) {
            DWORD mode = 0;
            if (::GetConsoleMode(hConsole, &mode) == TRUE) {
                mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING | ENABLE_PROCESSED_OUTPUT;
                ::SetConsoleMode(hConsole, mode);
            }
        }
    }
}

int launchExecutable() {
    // Get the path of the main ImHex executable
    auto executablePath = wolv::io::fs::getExecutablePath();
    auto executableFullPath = executablePath->parent_path() / "imhex-gui.exe";

    ::PROCESS_INFORMATION process = { };
    ::STARTUPINFOW startupInfo = { };
    startupInfo.cb = sizeof(STARTUPINFOW);

    // Create a new process for imhex-gui.exe with the same command line as the current process
    if (::CreateProcessW(executableFullPath.wstring().c_str(), ::GetCommandLineW(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startupInfo, &process) == FALSE) {
        // Handle error if the process could not be created

        auto errorCode = ::GetLastError();
        auto errorMessageString = std::system_category().message(errorCode);

        auto errorMessage = fmt::format("Failed to start ImHex:\n\nError code: 0x{:08X}\n\n{}", errorCode, errorMessageString);

        ::MessageBoxA(nullptr, errorMessage.c_str(), "ImHex Forwarder", MB_OK | MB_ICONERROR);

        return EXIT_FAILURE;
    }

    ::WaitForSingleObject(process.hProcess, INFINITE);
    ::CloseHandle(process.hProcess);

    return EXIT_SUCCESS;
}

int main() {
    handleConsoleWindow();
    auto result = launchExecutable();

    return result;
}