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
#include <array>

void setupConsoleWindow() {
    // Get the handle of the console window
    HWND consoleWindow = ::GetConsoleWindow();

    // Get console process ID
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

        // Set the __IMHEX_FORWARD_CONSOLE__ environment variable,
        // to let ImHex know that it was launched from the forwarder
        // and that it should forward it's console output to us
        ::SetEnvironmentVariableA("__IMHEX_FORWARD_CONSOLE__", "1");
    }
}

int launchExecutable() {
    // Get the path of the main ImHex executable
    auto executablePath = wolv::io::fs::getExecutablePath();
    auto executableFullPath = executablePath->parent_path() / "imhex-gui.exe";

    // Handles for the pipes
    HANDLE hChildStdoutRead, hChildStdoutWrite;

    // Security attributes to allow the pipes to be inherited
    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.lpSecurityDescriptor = nullptr;
    saAttr.bInheritHandle = TRUE;

    // Create pipes for stdout redirection
    if (!::CreatePipe(&hChildStdoutRead, &hChildStdoutWrite, &saAttr, 0)) {
        return EXIT_FAILURE;
    }

    // Set up the STARTUPINFO structure for the child process
    STARTUPINFO si;
    ::ZeroMemory(&si, sizeof(STARTUPINFO));
    si.cb = sizeof(STARTUPINFO);
    si.hStdOutput = hChildStdoutWrite; // Redirect stdout to the parent process
    si.dwFlags |= STARTF_USESTDHANDLES; // Enable redirection of stdin, stdout, stderr

    PROCESS_INFORMATION pi;
    ::ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));

    // Create the child process
    if (!::CreateProcessW(
        executableFullPath.c_str(),
        ::GetCommandLineW(),    // Command line
        nullptr,                // Process security attributes
        nullptr,                // Thread security attributes
        TRUE,                   // Inherit handles
        0,                      // Creation flags
        nullptr,                // Environment
        nullptr,                // Current directory
        &si,                    // STARTUPINFO
        &pi                     // PROCESS_INFORMATION
    )) {
        return EXIT_FAILURE;
    }

    // Close unnecessary pipe handles in the parent process
    ::CloseHandle(hChildStdoutWrite);

    // Read the child process's stdout and stderr and redirect them to the parent's stdout
    DWORD bytesRead;
    std::array<char, 4096> buffer;

    while (true) {
        // Read from stdout
        if (::ReadFile(hChildStdoutRead, buffer.data(), buffer.size(), &bytesRead, nullptr)) {
            // Write to the parent's stdout
            if (bytesRead > 0)
                ::WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buffer.data(), bytesRead, &bytesRead, nullptr);
        } else {
            break;
        }
    }

    // Wait for the child process to exit
   ::WaitForSingleObject(pi.hProcess, INFINITE);

    // Get the exit code of the child process
    DWORD exitCode = 0;
    if (!::GetExitCodeProcess(pi.hProcess, &exitCode)) {
        exitCode = EXIT_FAILURE;
    }

    // Clean up
    ::CloseHandle(pi.hProcess);
    ::CloseHandle(pi.hThread);
    ::CloseHandle(hChildStdoutRead);

    return static_cast<int>(exitCode);
}

int main() {
    setupConsoleWindow();

    return launchExecutable();
}