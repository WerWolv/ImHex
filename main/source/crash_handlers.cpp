#include <hex/api/project_file_manager.hpp>
#include <hex/helpers/logger.hpp>
#include <hex/helpers/fs.hpp>
#include <hex/helpers/stacktrace.hpp>

#include <wolv/io/fs.hpp>
#include <wolv/utils/string.hpp>

#include "window.hpp"

#include <nlohmann/json.hpp>

#include <llvm/Demangle/Demangle.h>

#include <exception>
#include <csignal>

namespace hex::crash {

    constexpr static auto CrashBackupFileName = "crash_backup.hexproj";

    static std::terminate_handler originalHandler;
    static bool s_shouldSaveCrashFile = false;

    
    static void saveCrashFile(){
        if(!s_shouldSaveCrashFile)return;

        nlohmann::json crashData{
            {"logFile", wolv::util::toUTF8String(hex::log::getFile().getPath())},
            {"project", wolv::util::toUTF8String(ProjectFile::getPath())},
        };
        
        for (const auto &path : fs::getDefaultPaths(fs::ImHexPath::Config)) {
            wolv::io::File file(path / "crash.json", wolv::io::File::Mode::Write);
            if (file.isValid()) {
                file.writeString(crashData.dump(4));
                file.close();
                log::info("Wrote crash.json file to {}", wolv::util::toUTF8String(file.getPath()));
                return;
            }
        }
        log::warn("Could not write crash.json file !");
    }

    // Custom signal handler to print various information and a stacktrace when the application crashes
    static void signalHandler(int signalNumber, const std::string &signalName) {
        log::fatal("Terminating with signal '{}' ({})", signalName, signalNumber);

        // save crash.json file
        saveCrashFile();

        // Trigger an event so that plugins can handle crashes
        // It may affect things (like the project path),
        // so we do this after saving the crash file    
        EventManager::post<EventAbnormalTermination>(signalNumber);

        // Detect if the crash was due to an uncaught exception
        if (std::uncaught_exceptions() > 0) {
            log::fatal("Uncaught exception thrown!");
        }

        // Reset the signal handler to the default handler
        std::signal(SIGSEGV, SIG_DFL);
        std::signal(SIGILL,  SIG_DFL);
        std::signal(SIGABRT, SIG_DFL);
        std::signal(SIGFPE,  SIG_DFL);

        // Print stack trace
        for (const auto &stackFrame : stacktrace::getStackTrace()) {
            if (stackFrame.line == 0)
                log::fatal("  {}", stackFrame.function);
            else
                log::fatal("  ({}:{}) | {}",  stackFrame.file, stackFrame.line, stackFrame.function);
        }
    
        // Trigger a breakpoint if we're in a debug build or raise the signal again for the default handler to handle it
        #if defined(DEBUG)
            assert(!"Debug build, triggering breakpoint");
        #else
            std::raise(signalNumber);
        #endif
    }

    // setup functions to handle signals, uncaught exception, or similar stuff that will crash ImHex
    void setupCrashHandlers(){
         // Register signal handlers
        {
            #define HANDLE_SIGNAL(name)             \
            std::signal(name, [](int signalNumber){ \
                signalHandler(signalNumber, #name); \
            })

            HANDLE_SIGNAL(SIGSEGV);
            HANDLE_SIGNAL(SIGILL);
            HANDLE_SIGNAL(SIGABRT);
            HANDLE_SIGNAL(SIGFPE);

            #undef HANDLE_SIGNAL
        }

        // register uncaught exception handlers
        originalHandler = std::set_terminate([]{
            try {
                std::rethrow_exception(std::current_exception());
            } catch (std::exception &ex) {
                log::fatal(
                        "Program terminated with uncaught exception: {}()::what() -> {}",
                        llvm::itaniumDemangle(typeid(ex).name(), nullptr, nullptr, nullptr),
                        ex.what()
                );
            }

            // the handler should eventually release a signal, which will be caught and used to handle the crash
            originalHandler();

            log::error("Should not happen: original std::set_terminate handler returned. Terminating manually");
            exit(EXIT_FAILURE);
        });

        // Save a backup project when the application crashes
        // We need to save the project no mater if it is dirty,
        // because this save is responsible for telling us which files
        // were opened in case there wasn't a project
        EventManager::subscribe<EventAbnormalTermination>([](int) {
            auto imguiSettingsPath = hex::getImGuiSettingsPath();
            if(!imguiSettingsPath.empty())
                ImGui::SaveIniSettingsToDisk(wolv::util::toUTF8String(imguiSettingsPath).c_str());

            for (const auto &path : fs::getDefaultPaths(fs::ImHexPath::Config)) {
                if (ProjectFile::store(path / CrashBackupFileName))
                    break;
            }
        });

        EventManager::subscribe<EventImHexStartupFinished>([]{
            s_shouldSaveCrashFile = true;
        });
    }
}