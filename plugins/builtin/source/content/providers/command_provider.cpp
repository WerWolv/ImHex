#if !defined(OS_WEB)

#include "content/providers/command_provider.hpp"

#include <chrono>

#include <imgui.h>
#include <hex/ui/imgui_imhex_extensions.h>

#include <hex/helpers/fmt.hpp>
#include <hex/api/localization_manager.hpp>
#include <hex/helpers/logger.hpp>

#include <nlohmann/json.hpp>
#include <wolv/utils/charconv.hpp>

#if defined(OS_WINDOWS)
    #include <windows.h>
#else
    #include <unistd.h>
    #include <sys/types.h>
    #include <sys/wait.h>
    #include <fcntl.h>
#endif

namespace hex::plugin::builtin {

    using namespace std::chrono_literals;

    CommandProvider::CommandProvider() = default;

    bool CommandProvider::isAvailable() const {
        return m_open;
    }

    bool CommandProvider::isReadable() const {
        return true;
    }

    bool CommandProvider::isWritable() const {
        return !m_writeCommand.empty();
    }

    bool CommandProvider::isResizable() const {
        return !m_resizeCommand.empty();
    }

    bool CommandProvider::isSavable() const {
        return !m_saveCommand.empty();
    }

    static std::vector<u8> executeCommand(const std::string &command, std::span<const u8> stdinData = {}) {
        std::vector<u8> output;

        #if defined(_WIN32)

            HANDLE hStdinRead = nullptr, hStdinWrite = nullptr;
            HANDLE hStdoutRead = nullptr, hStdoutWrite = nullptr;

            SECURITY_ATTRIBUTES sa{};
            sa.nLength = sizeof(SECURITY_ATTRIBUTES);
            sa.bInheritHandle = TRUE;

            if (!CreatePipe(&hStdoutRead, &hStdoutWrite, &sa, 0)) {
                log::error("CreatePipe(stdout) failed");
                return {};
            }

            if (!SetHandleInformation(hStdoutRead, HANDLE_FLAG_INHERIT, 0)) {
                log::error("SetHandleInformation(stdout) failed");
                CloseHandle(hStdoutRead); CloseHandle(hStdoutWrite);
                return {};
            }

            if (!CreatePipe(&hStdinRead, &hStdinWrite, &sa, 0)) {
                log::error("CreatePipe(stdin) failed");
                CloseHandle(hStdoutRead); CloseHandle(hStdoutWrite);
                return {};
            }

            if (!SetHandleInformation(hStdinWrite, HANDLE_FLAG_INHERIT, 0)) {
                log::error("SetHandleInformation(stdin) failed");
                CloseHandle(hStdoutRead); CloseHandle(hStdoutWrite);
                CloseHandle(hStdinRead); CloseHandle(hStdinWrite);
                return {};
            }

            STARTUPINFOW si{};
            si.cb = sizeof(si);
            si.hStdOutput = si.hStdError = hStdoutWrite;
            si.hStdInput = hStdinRead;
            si.dwFlags = STARTF_USESTDHANDLES;

            PROCESS_INFORMATION pi{};

            // UTF-16 conversion
            auto wcmd = wolv::util::utf8ToWstring(command);

            if (!CreateProcessW(
                    nullptr, wcmd->data(),
                    nullptr, nullptr,
                    TRUE,
                    0,
                    nullptr, nullptr,
                    &si, &pi
                )) {
                log::error("CreateProcessW failed");
                CloseHandle(hStdoutRead); CloseHandle(hStdoutWrite);
                CloseHandle(hStdinRead); CloseHandle(hStdinWrite);
                return {};
            }

            CloseHandle(hStdoutWrite);
            CloseHandle(hStdinRead);

            // Write stdin
            if (!stdinData.empty()) {
                DWORD written = 0;
                WriteFile(hStdinWrite, stdinData.data(),
                          (DWORD)stdinData.size(), &written, nullptr);
            }
            CloseHandle(hStdinWrite);

            // Read stdout
            u8 buffer[4096];
            DWORD bytesRead = 0;

            while (ReadFile(hStdoutRead, buffer, sizeof(buffer), &bytesRead, nullptr) && bytesRead > 0) {
                output.insert(output.end(), buffer, buffer + bytesRead);
            }

            CloseHandle(hStdoutRead);

            WaitForSingleObject(pi.hProcess, INFINITE);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);

            return output;

        #else

            int stdinPipe[2];
            int stdoutPipe[2];

            if (pipe(stdinPipe) != 0 || pipe(stdoutPipe) != 0) {
                log::error("pipe() failed");
                return {};
            }

            pid_t pid = fork();
            if (pid < 0) {
                log::error("fork() failed");
                return {};
            }

            if (pid == 0) {
                // Child
                dup2(stdinPipe[0], STDIN_FILENO);
                dup2(stdoutPipe[1], STDOUT_FILENO);
                dup2(stdoutPipe[1], STDERR_FILENO);

                close(stdinPipe[1]);
                close(stdoutPipe[0]);

                execl("/bin/sh", "sh", "-c", command.c_str(), (char*)nullptr);

                _exit(127);
            }

            // Parent
            close(stdinPipe[0]);
            close(stdoutPipe[1]);

            if (!stdinData.empty()) {
                std::ignore = write(stdinPipe[1], stdinData.data(), stdinData.size());
            }
            close(stdinPipe[1]);

            u8 buffer[4096];
            while (true) {
                ssize_t n = read(stdoutPipe[0], buffer, sizeof(buffer));
                if (n <= 0) break;
                output.insert(output.end(), buffer, buffer + n);
            }

            close(stdoutPipe[0]);
            waitpid(pid, nullptr, 0);

            return output;

        #endif
    }

    static std::string executeCommandString(const std::string &command) {
        auto output = executeCommand(command);
        return std::string(output.begin(), output.end());
    }

    void CommandProvider::readFromSource(u64 offset, void *buffer, size_t size) {
        auto output = executeCommand(
            fmt::format(fmt::runtime(m_readCommand),
                fmt::arg("address", offset),
                fmt::arg("size", size)
            )
        );

        if (output.size() < size) {
            std::memcpy(buffer, output.data(), output.size());
            std::memset(static_cast<u8*>(buffer) + output.size(), 0, size - output.size());
        } else {
            std::memcpy(buffer, output.data(), size);
        }
    }

    void CommandProvider::writeToSource(u64 offset, const void *buffer, size_t size) {
        if (m_writeCommand.empty())
            return;

        std::ignore = executeCommand(
            fmt::format(fmt::runtime(m_writeCommand),
                fmt::arg("address", offset),
                fmt::arg("size", size)
            ),
            { static_cast<const u8*>(buffer), size }
        );
    }

    void CommandProvider::save() {
        Provider::save();
        std::ignore = executeCommand(m_saveCommand);
    }

    u64 CommandProvider::getSourceSize() const {
        if (m_sizeCommand.empty())
            return std::numeric_limits<u32>::max();

        const auto output = executeCommandString(m_sizeCommand);
        return wolv::util::from_chars<u64>(output).value_or(0);
    }

    std::string CommandProvider::getName() const {
        return fmt::format("hex.builtin.provider.command.name"_lang, m_name);
    }

    prv::Provider::OpenResult CommandProvider::open() {
        m_open = true;
        return {};
    }

    void CommandProvider::close() {

    }

    bool CommandProvider::drawLoadInterface() {
        ImGui::InputText("hex.builtin.provider.command.load.name"_lang, m_name);
        ImGui::Separator();
        ImGui::NewLine();

        ImGui::InputText("hex.builtin.provider.command.load.read_command"_lang,   m_readCommand);
        ImGui::InputTextWithHint("hex.builtin.provider.command.load.write_command"_lang,  "hex.builtin.provider.command.optional"_lang, m_writeCommand);
        ImGui::InputTextWithHint("hex.builtin.provider.command.load.size_command"_lang,   "hex.builtin.provider.command.optional"_lang, m_sizeCommand);
        ImGui::InputTextWithHint("hex.builtin.provider.command.load.resize_command"_lang, "hex.builtin.provider.command.optional"_lang, m_resizeCommand);
        ImGui::InputTextWithHint("hex.builtin.provider.command.load.save_command"_lang,   "hex.builtin.provider.command.optional"_lang, m_saveCommand);
        ImGuiExt::HelpHover("hex.builtin.provider.command.load.hint"_lang, ICON_VS_INFO);

        return !m_name.empty() && !m_readCommand.empty();
    }

    void CommandProvider::loadSettings(const nlohmann::json &settings) {
        Provider::loadSettings(settings);

        m_readCommand   = settings.value("read", "");
        m_writeCommand  = settings.value("write", "");
        m_resizeCommand = settings.value("resize", "");
        m_sizeCommand   = settings.value("size", "");
        m_saveCommand   = settings.value("save", "");
        m_name          = settings.value("name", "");
    }

    nlohmann::json CommandProvider::storeSettings(nlohmann::json settings) const {
        settings["read"]   = m_readCommand;
        settings["write"]  = m_writeCommand;
        settings["resize"] = m_resizeCommand;
        settings["size"]   = m_sizeCommand;
        settings["save"]   = m_saveCommand;
        settings["name"]   = m_name;

        return Provider::storeSettings(settings);
    }

}

#endif