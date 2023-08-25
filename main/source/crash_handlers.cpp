#include <hex/api/project_file_manager.hpp>
#include <hex/api/task.hpp>

#include <init/tasks.hpp>

#include <hex/helpers/logger.hpp>
#include <hex/helpers/fs.hpp>
#include <hex/helpers/stacktrace.hpp>

#include <wolv/io/fs.hpp>
#include <wolv/utils/string.hpp>

#include <window.hpp>

#include <nlohmann/json.hpp>

#include <llvm/Demangle/Demangle.h>

#include <csignal>
#include <exception>
#include <typeinfo>

#if defined (OS_MACOS)
    #include <sys/utsname.h>
#endif

namespace hex::crash {

    constexpr static auto CrashBackupFileName = "crash_backup.hexproj";
    constexpr static auto Signals = { SIGSEGV, SIGILL, SIGABRT, SIGFPE };

    void resetCrashHandlers();
    
    static void sendNativeMessage(const std::string& message) {
        hex::nativeErrorMessage(hex::format("ImHex crashed during its loading.\nError: {}", message));
    }

    // Function that decides what should happen on a crash
    // (either sending a message or saving a crash file, depending on when the crash occurred)
    using CrashCallback = void (*) (const std::string&);
    static CrashCallback crashCallback = sendNativeMessage;

    static void saveCrashFile(const std::string& message) {
        log::fatal(message);

        nlohmann::json crashData{
            { "logFile", wolv::util::toUTF8String(hex::log::impl::getFile().getPath()) },
            { "project", wolv::util::toUTF8String(ProjectFile::getPath()) },
        };
        
        for (const auto &path : fs::getDefaultPaths(fs::ImHexPath::Config)) {
            wolv::io::File file(path / "crash.json", wolv::io::File::Mode::Create);
            if (file.isValid()) {
                file.writeString(crashData.dump(4));
                file.close();
                log::info("Wrote crash.json file to {}", wolv::util::toUTF8String(file.getPath()));

                return;
            }
        }

        log::warn("Could not write crash.json file!");
    }

    static void printStackTrace() {
        for (const auto &stackFrame : stacktrace::getStackTrace()) {
            if (stackFrame.line == 0)
                log::fatal("  {}", stackFrame.function);
            else
                log::fatal("  ({}:{}) | {}",  stackFrame.file, stackFrame.line, stackFrame.function);
        }
    }

    extern "C" void triggerSafeShutdown(int signalNumber = 0) {
        // Trigger an event so that plugins can handle crashes
        EventManager::post<EventAbnormalTermination>(signalNumber);

        // Run exit tasks
        for (const auto &[name, task, async] : init::getExitTasks())
            task();

        // Terminate all asynchronous tasks
        TaskManager::exit();

        // Trigger a breakpoint if we're in a debug build or raise the signal again for the default handler to handle it
        #if defined(DEBUG)

            if (signalNumber == 0) {
                #if defined(OS_WINDOWS)
                    __debugbreak();
                #else
                    raise(SIGTRAP);
                #endif
            } else {
                std::exit(signalNumber);
            }

        #else

            if (signalNumber == 0)
                std::abort();
            else
                std::exit(signalNumber);

        #endif
    }

    void handleCrash(const std::string &msg) {
        // Call the crash callback
        crashCallback(msg);

        // Print the stacktrace to the console or log file
        printStackTrace();

        // Flush all streams
        fflush(stdout);
        fflush(stderr);
    }

    // Custom signal handler to print various information and a stacktrace when the application crashes
    static void signalHandler(int signalNumber, const std::string &signalName) {
        // Reset crash handlers, so we can't have a recursion if this code crashes
        resetCrashHandlers();

        // Actually handle the crash
        handleCrash(hex::format("Received signal '{}' ({})", signalName, signalNumber));

        // Detect if the crash was due to an uncaught exception
        if (std::uncaught_exceptions() > 0) {
            log::fatal("Uncaught exception thrown!");
        }

        triggerSafeShutdown(signalNumber);
    }

    static void uncaughtExceptionHandler() {
        // Reset crash handlers, so we can't have a recursion if this code crashes
        resetCrashHandlers();

        handleCrash("Uncaught exception!");

        // Print the current exception info
        try {
            std::rethrow_exception(std::current_exception());
        } catch (std::exception &ex) {
            std::string exceptionStr = hex::format("{}()::what() -> {}", llvm::itaniumDemangle(typeid(ex).name(), nullptr, nullptr, nullptr), ex.what());
            log::fatal("Program terminated with uncaught exception: {}", exceptionStr);

        }

        triggerSafeShutdown();
    }

    // Setup functions to handle signals, uncaught exception, or similar stuff that will crash ImHex
    void setupCrashHandlers() {
         // Register signal handlers
        {
            #define HANDLE_SIGNAL(name)              \
            std::signal(name, [](int signalNumber) { \
                signalHandler(signalNumber, #name);  \
            })

            HANDLE_SIGNAL(SIGSEGV);
            HANDLE_SIGNAL(SIGILL);
            HANDLE_SIGNAL(SIGABRT);
            HANDLE_SIGNAL(SIGFPE);

            #undef HANDLE_SIGNAL
        }

        // Configure the uncaught exception handler
        std::set_terminate(uncaughtExceptionHandler);

        // Save a backup project when the application crashes
        // We need to save the project no mater if it is dirty,
        // because this save is responsible for telling us which files
        // were opened in case there wasn't a project
        // Only do it when ImHex has finished its loading
        EventManager::subscribe<EventImHexStartupFinished>([] {
            EventManager::subscribe<EventAbnormalTermination>([](int) {
                // Save ImGui settings
                auto imguiSettingsPath = hex::getImGuiSettingsPath();
                if (!imguiSettingsPath.empty())
                    ImGui::SaveIniSettingsToDisk(wolv::util::toUTF8String(imguiSettingsPath).c_str());

                // Create crash backup if any providers are open
                if (ImHexApi::Provider::isValid()) {
                    for (const auto &path : fs::getDefaultPaths(fs::ImHexPath::Config)) {
                        if (ProjectFile::store(path / CrashBackupFileName, false))
                            break;
                    }
                }
            });
        });

        // Change the crash callback when ImHex has finished startup
        EventManager::subscribe<EventImHexStartupFinished>([]{
            crashCallback = saveCrashFile;
        });
    }

    void resetCrashHandlers() {
        std::set_terminate(nullptr);

        for (auto signal : Signals)
            std::signal(signal, SIG_DFL);
    }
}