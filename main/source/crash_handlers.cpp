#include <hex/api/project_file_manager.hpp>
#include <hex/api/task.hpp>

#include <hex/helpers/logger.hpp>
#include <hex/helpers/fs.hpp>
#include <hex/helpers/stacktrace.hpp>

#include <wolv/io/fs.hpp>
#include <wolv/utils/string.hpp>

#include <window.hpp>

#include <nlohmann/json.hpp>

#include <llvm/Demangle/Demangle.h>

#include <exception>
#include <csignal>

#if defined (OS_MACOS)
    #include <sys/utsname.h>
#endif

namespace hex::crash {

    constexpr static auto CrashBackupFileName = "crash_backup.hexproj";
    constexpr static auto Signals = {SIGSEGV, SIGILL, SIGABRT,SIGFPE};

    static std::terminate_handler originalHandler;
    
    static void sendNativeMessage(const std::string& message) {
        hex::nativeErrorMessage(hex::format("ImHex crashed during its loading.\nError: {}", message));
    }

    // Function that decides what should happen on a crash
    // (either sending a message or saving a crash file, depending on when the crash occurred)
    static std::function<void(const std::string&)> crashCallback = sendNativeMessage;

    static void saveCrashFile(const std::string& message) {
        log::fatal(message);

        nlohmann::json crashData{
            {"logFile", wolv::util::toUTF8String(hex::log::impl::getFile().getPath())},
            {"project", wolv::util::toUTF8String(ProjectFile::getPath())},
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
        log::warn("Could not write crash.json file !");
    }

    static void printStackTrace() {
        for (const auto &stackFrame : stacktrace::getStackTrace()) {
            if (stackFrame.line == 0)
                log::fatal("  {}", stackFrame.function);
            else
                log::fatal("  ({}:{}) | {}",  stackFrame.file, stackFrame.line, stackFrame.function);
        }
    }

    void handleCrash(const std::string &msg, int signalNumber) {
        crashCallback(msg);

        printStackTrace();
        
        // Trigger an event so that plugins can handle crashes
        EventManager::post<EventAbnormalTermination>(signalNumber);
    }

    // Custom signal handler to print various information and a stacktrace when the application crashes
    static void signalHandler(int signalNumber, const std::string &signalName) {
        // Reset the signal handler to the default handler
        for(auto signal : Signals) std::signal(signal, SIG_DFL);

        // Actually handle the crash
        handleCrash(hex::format("Received signal '{}' ({})", signalName, signalNumber), signalNumber);

        // Detect if the crash was due to an uncaught exception
        if (std::uncaught_exceptions() > 0) {
            log::fatal("Uncaught exception thrown!");
        }

        // Trigger a breakpoint if we're in a debug build or raise the signal again for the default handler to handle it
        #if defined(DEBUG)
            assert(!"Debug build, triggering breakpoint");
        #else
            std::exit(signalNumber);
        #endif
    }

    // Setup functions to handle signals, uncaught exception, or similar stuff that will crash ImHex
    void setupCrashHandlers() {
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

        originalHandler = std::set_terminate([]{
            // Restore the original handler of C++ std
            std::set_terminate(originalHandler);

            try {
                std::rethrow_exception(std::current_exception());
            } catch (std::exception &ex) {
                std::string exceptionStr = hex::format("{}()::what() -> {}", llvm::itaniumDemangle(typeid(ex).name(), nullptr, nullptr, nullptr), ex.what());
                log::fatal("Program terminated with uncaught exception: {}", exceptionStr);

                // Actually handle the crash
                handleCrash(hex::format("Uncaught exception: {}", exceptionStr), 0);

                // Reset signal handlers prior to calling the original handler, because it may raise a signal
                for(auto signal : Signals) std::signal(signal, SIG_DFL);

                #if defined(DEBUG)
                    assert(!"Debug build, triggering breakpoint");
                #else
                    std::exit(100);
                #endif
            }
        });

        // Save a backup project when the application crashes
        // We need to save the project no mater if it is dirty,
        // because this save is responsible for telling us which files
        // were opened in case there wasn't a project
        // Only do it when ImHex has finished its loading
        EventManager::subscribe<EventImHexStartupFinished>([] {
            EventManager::subscribe<EventAbnormalTermination>([](int) {
                auto imguiSettingsPath = hex::getImGuiSettingsPath();
                if (!imguiSettingsPath.empty())
                    ImGui::SaveIniSettingsToDisk(wolv::util::toUTF8String(imguiSettingsPath).c_str());

                for (const auto &path : fs::getDefaultPaths(fs::ImHexPath::Config)) {
                    if (ProjectFile::store(path / CrashBackupFileName, false))
                        break;
                }
            });
        });

        EventManager::subscribe<EventImHexStartupFinished>([]{
            crashCallback = saveCrashFile;
        });
    }
}