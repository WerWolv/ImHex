#include <crash_handlers.hpp>

#include <hex/api/project_file_manager.hpp>
#include <hex/api/task_manager.hpp>
#include <hex/api/workspace_manager.hpp>

#include <hex/helpers/logger.hpp>
#include <hex/helpers/default_paths.hpp>

#include <wolv/utils/string.hpp>

#include <window.hpp>
#include <init/tasks.hpp>
#include <hex/trace/stacktrace.hpp>

#include <nlohmann/json.hpp>

#include <csignal>
#include <exception>
#include <typeinfo>
#include <hex/helpers/debugging.hpp>
#include <hex/helpers/utils.hpp>

#if defined(IMGUI_TEST_ENGINE)
    #include <imgui_te_engine.h>
#endif

#if defined (OS_WINDOWS)
    #include <windows.h>

    #if !defined(_MSC_VER)
        #include <pthread.h>
    #endif
#elif defined (OS_MACOS)
    #include <sys/utsname.h>
#endif

namespace hex::crash {

    constexpr static auto CrashBackupFileName = "crash_backup.hexproj";
    constexpr static auto Signals = { SIGSEGV, SIGILL, SIGABRT, SIGFPE };

    void resetCrashHandlers();
    
    static void sendNativeMessage(const std::string& message) {
        hex::showErrorMessageBox(fmt::format("ImHex crashed during initial setup!\nError: {}", message));
    }

    // Function that decides what should happen on a crash
    // (either sending a message or saving a crash file, depending on when the crash occurred)
    static CrashCallback s_crashCallback = sendNativeMessage;

    void setCrashCallback(CrashCallback callback) {
        s_crashCallback = std::move(callback);
    }

    static std::fs::path s_crashBackupPath;
    static void saveCrashFile(const std::string& message) {
        log::fatal("{}", message);

        const nlohmann::json crashData {
            { "logFile", wolv::io::fs::toNormalizedPathString(hex::log::impl::getFile().getPath()) },
            { "project", wolv::io::fs::toNormalizedPathString(s_crashBackupPath) },
        };
        
        for (const auto &path : paths::Config.write()) {
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

    static void callCrashHandlers(const std::string &msg) {
        // Call the crash callback
        s_crashCallback(msg);

        // Print the stacktrace to the console or log file
        dbg::printStackTrace(trace::getStackTrace());

        // Flush all streams
        std::fflush(stdout);
        std::fflush(stderr);

        #if defined(IMGUI_TEST_ENGINE)
            ImGuiTestEngine_CrashHandler();
        #endif
    }

     extern "C" [[noreturn]] void triggerSafeShutdown(std::string crashMessage, int signalNumber = 0) {
        if (!TaskManager::isMainThread()) {
            log::error("Terminating from non-main thread, scheduling termination on main thread");
            TaskManager::doLater([=] {
                triggerSafeShutdown(crashMessage, signalNumber);
            });

            // Terminate this thread
            #if defined(_MSC_VER)
                TerminateThread(GetCurrentThread(), EXIT_FAILURE);
            #else
                pthread_kill(pthread_self(), SIGABRT);
            #endif

            while (true) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }

        // Trigger an event so that plugins can handle crashes
        EventAbnormalTermination::post(signalNumber);
        callCrashHandlers(crashMessage);

        // Run exit tasks
        init::runExitTasks();

        // Terminate all asynchronous tasks
        TaskManager::exit();

        // Trigger a breakpoint if we're in a debug build or raise the signal again for the default handler to handle it
        #if defined(DEBUG)

            static bool breakpointTriggered = false;
            if (!breakpointTriggered) {
                breakpointTriggered = true;
                #if defined(OS_WINDOWS)
                    __debugbreak();
                #else
                    raise(SIGTRAP);
                #endif
            }

            std::exit(signalNumber);

        #else

            if (signalNumber == 0)
                std::abort();
            else
                std::exit(signalNumber);

        #endif
    }

    // Custom signal handler to print various information and a stacktrace when the application crashes
    static void signalHandler(int signalNumber, const std::string &signalName) {
        #if !defined (DEBUG)
            if (signalNumber == SIGINT) {
                ImHexApi::System::closeImHex();
                return;
            }
        #endif

        // Detect if the crash was due to an uncaught exception
        if (std::uncaught_exceptions() > 0) {
            log::fatal("Uncaught exception thrown!");
        }

        triggerSafeShutdown(fmt::format("Received signal '{}' ({})", signalName, signalNumber), signalNumber);
    }

    static void uncaughtExceptionHandler() {
        // Reset crash handlers, so we can't have a recursion if this code crashes
        resetCrashHandlers();

        // Print the current exception info
        try {
            if (auto exception = std::current_exception(); exception != nullptr)
                std::rethrow_exception(exception);
            else
                log::fatal("Program terminated due to unknown reason!");
        } catch (std::exception &ex) {
            const auto exceptionStr = fmt::format("Program terminated with uncaught exception: {}()::what() -> {}", trace::demangle(typeid(ex).name()), ex.what());
            triggerSafeShutdown(exceptionStr);
        }
    }

    // Setup functions to handle signals, uncaught exception, or similar stuff that will crash ImHex
    void setupCrashHandlers() {
        trace::initialize();
        trace::setAssertionHandler(dbg::assertionHandler);

        // Register signal handlers
        {
            #if defined(OS_WINDOWS)
                #define HANDLE_SIGNAL(name) case name: signalHandler(name, #name); break
                SetUnhandledExceptionFilter([](EXCEPTION_POINTERS* exceptionInfo) -> LONG {
                    switch (exceptionInfo->ExceptionRecord->ExceptionCode) {
                        HANDLE_SIGNAL(EXCEPTION_ACCESS_VIOLATION);
                        HANDLE_SIGNAL(EXCEPTION_ILLEGAL_INSTRUCTION);
                        HANDLE_SIGNAL(EXCEPTION_INT_DIVIDE_BY_ZERO);
                        HANDLE_SIGNAL(EXCEPTION_STACK_OVERFLOW);
                        HANDLE_SIGNAL(EXCEPTION_DATATYPE_MISALIGNMENT);
                        HANDLE_SIGNAL(EXCEPTION_ARRAY_BOUNDS_EXCEEDED);
                    }

                    return EXCEPTION_CONTINUE_SEARCH;
                });
                #undef HANDLE_SIGNAL
            #else
                #define HANDLE_SIGNAL(name)                         \
                    {                                               \
                        struct sigaction action = { };              \
                        action.sa_handler = [](int signalNumber) {  \
                            signalHandler(signalNumber, #name);     \
                        };                                          \
                        sigemptyset(&action.sa_mask);               \
                        action.sa_flags = 0;                        \
                        sigaction(name, &action, nullptr);          \
                    }

                HANDLE_SIGNAL(SIGSEGV);
                HANDLE_SIGNAL(SIGILL);
                HANDLE_SIGNAL(SIGABRT);
                HANDLE_SIGNAL(SIGFPE);

                #if defined (SIGBUS)
                    HANDLE_SIGNAL(SIGBUS);
                #endif

                #undef HANDLE_SIGNAL
            #endif
        }

        // Configure the uncaught exception handler
        std::set_terminate(uncaughtExceptionHandler);

        // Save a backup project when the application crashes
        // We need to save the project no mater if it is dirty,
        // because this save is responsible for telling us which files
        // were opened in case there wasn't a project
        // Only do it when ImHex has finished its loading
        EventImHexStartupFinished::subscribe([] {
            EventAbnormalTermination::subscribe([](int) {
                WorkspaceManager::exportToFile();

                // Create crash backup if any providers are open
                if (ImHexApi::Provider::isValid()) {
                    for (const auto &path : paths::Config.write()) {
                        if (ProjectFile::store(path / CrashBackupFileName, false)) {
                            s_crashBackupPath = path / CrashBackupFileName;
                            log::fatal("Saved crash backup to '{}'", wolv::util::toUTF8String(s_crashBackupPath));
                            break;
                        }
                    }
                }
            });
        });

        // Change the crash callback when ImHex has finished startup
        EventImHexStartupFinished::subscribe([]{
            setCrashCallback(saveCrashFile);
        });
    }

    void resetCrashHandlers() {
        log::resumeLogging();
        std::set_terminate(nullptr);

        for (auto signal : Signals)
            std::signal(signal, SIG_DFL);
    }
}
