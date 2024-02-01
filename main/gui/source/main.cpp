#include <hex.hpp>

#include <hex/helpers/logger.hpp>

#include "window.hpp"
#include "crash_handlers.hpp"
#include "messaging.hpp"

#include "init/splash_window.hpp"
#include "init/tasks.hpp"

#include <hex/api/task_manager.hpp>
#include <hex/api/plugin_manager.hpp>

#include <hex/helpers/fs.hpp>
#include "hex/subcommands/subcommands.hpp"

#include <wolv/io/fs.hpp>
#include <wolv/utils/guards.hpp>
#include <fcntl.h>

#if defined(OS_WINDOWS)
    #include <windows.h>
    #include <shellapi.h>
    #include <codecvt>
#elif defined(OS_WEB)
    #include <emscripten.h>
    #include <emscripten/html5.h>
#endif

using namespace hex;

namespace {

    /**
     * @brief Handles commands passed to ImHex via the command line
     * @param argc Argument count
     * @param argv Argument values
     */
    void handleCommandLineInterface(int argc, char **argv) {
        std::vector<std::string> args;

        #if defined (OS_WINDOWS)
            hex::unused(argv);

            // On Windows, argv contains UTF-16 encoded strings, so we need to convert them to UTF-8
            auto convertedCommandLine = ::CommandLineToArgvW(::GetCommandLineW(), &argc);
            if (convertedCommandLine == nullptr) {
                log::error("Failed to convert command line arguments to UTF-8");
                std::exit(EXIT_FAILURE);
            }

            for (int i = 1; i < argc; i += 1) {
                std::wstring wcharArg = convertedCommandLine[i];
                std::string  utf8Arg  = std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().to_bytes(wcharArg);

                args.push_back(utf8Arg);
            }

            ::LocalFree(convertedCommandLine);
        #else
            // Skip the first argument (the executable path) and convert the rest to a vector of strings
            args = { argv + 1, argv + argc };
        #endif


        // Load all plugins but don't initialize them
        for (const auto &dir : fs::getDefaultPaths(fs::ImHexPath::Plugins)) {
            PluginManager::load(dir);
        }

        // Setup messaging system to allow sending commands to the main ImHex instance
        hex::messaging::setupMessaging();

        // Process the arguments
        hex::subcommands::processArguments(args);

        // Unload plugins again
        PluginManager::unload();
    }

    /**
     * @brief Checks if the portable version of ImHex is installed
     * @note The portable version is detected by the presence of a file named "PORTABLE" in the same directory as the executable
     * @return True if ImHex is running in portable mode, false otherwise
     */
    bool isPortableVersion() {
        if (const auto executablePath = wolv::io::fs::getExecutablePath(); executablePath.has_value()) {
            const auto flagFile = executablePath->parent_path() / "PORTABLE";

            if (wolv::io::fs::exists(flagFile) && wolv::io::fs::isRegularFile(flagFile))
                return true;
        }

        return false;
    }

    /**
     * @brief Displays ImHex's splash screen and runs all initialization tasks. The splash screen will be displayed until all tasks have finished.
     */
    [[maybe_unused]]
    void initializeImHex() {
        init::WindowSplash splashWindow;

        log::info("Using '{}' GPU", ImHexApi::System::getGPUVendor());

        // Add initialization tasks to run
        TaskManager::init();
        for (const auto &[name, task, async] : init::getInitTasks())
            splashWindow.addStartupTask(name, task, async);

        splashWindow.startStartupTasks();

        // Draw the splash window while tasks are running
        if (!splashWindow.loop())
            ImHexApi::System::impl::addInitArgument("tasks-failed");
    }


    /**
     * @brief Deinitializes ImHex by running all exit tasks
     */
    void deinitializeImHex() {
        // Run exit tasks
        init::runExitTasks();

    }

    /**
     * @brief Handles a file open request by opening the file specified by OS-specific means
     */
    void handleFileOpenRequest() {
        if (auto path = hex::getInitialFilePath(); path.has_value()) {
            RequestOpenFile::post(path.value());
        }
    }


    #if defined(OS_WEB)

        using namespace hex::init;

        void saveFsData() {
            EM_ASM({
                FS.syncfs(function (err) {
                    if (!err)
                        return;
                    alert("Failed to save permanent file system: "+err);
                });
            });
        }

        int runImHex() {
            auto splashWindow = new WindowSplash();

            log::info("Using '{}' GPU", ImHexApi::System::getGPUVendor());

            // Add initialization tasks to run
            TaskManager::init();
            for (const auto &[name, task, async] : init::getInitTasks())
                splashWindow->addStartupTask(name, task, async);

            splashWindow->startStartupTasks();

            RequestRestartImHex::subscribe([&] {
                MAIN_THREAD_EM_ASM({
                    location.reload();
                });
            });

            // Draw the splash window while tasks are running
            emscripten_set_main_loop_arg([](void *arg) {
                auto splashWindow = reinterpret_cast<WindowSplash*>(arg);

                FrameResult frameResult = splashWindow->fullFrame();
                if (frameResult == FrameResult::Success) {
                    handleFileOpenRequest();

                    // Clean up everything after the main window is closed
                    emscripten_set_beforeunload_callback(nullptr, [](int eventType, const void *reserved, void *userData) {
                        hex::unused(eventType, reserved, userData);

                        try {
                            saveFsData();
                            deinitializeImHex();
                            return "";
                        } catch (const std::exception &ex) {
                            std::string *msg = new std::string("Failed to deinitialize ImHex. This is just a message warning you of this, the application has already closed, you probably can't do anything about it. Message: ");
                            msg->append(std::string(ex.what()));
                            log::fatal("{}", *msg);
                            return msg->c_str();
                        }
                    });

                    // Delete splash window (do it before creating the main window so glfw destroys the window)
                    delete splashWindow;

                    emscripten_cancel_main_loop();

                    // Main window
                    static Window window;
                    emscripten_set_main_loop([]() {
                        window.fullFrame();
                    }, 60, 0);
                }
            }, splashWindow, 60, 0);

            return -1;
        }

    #else

        int runImHex() {

            bool shouldRestart = false;
            do {
                // Register an event handler that will make ImHex restart when requested
                shouldRestart = false;
                RequestRestartImHex::subscribe([&] {
                    shouldRestart = true;
                });

                initializeImHex();
                handleFileOpenRequest();

                // Clean up everything after the main window is closed
                ON_SCOPE_EXIT {
                    deinitializeImHex();
                };

                // Main window
                Window window;
                window.loop();

            } while (shouldRestart);

            return EXIT_SUCCESS;
        }

    #endif

}

/**
 * @brief Main entry point of ImHex
 * @param argc Argument count
 * @param argv Argument values
 * @return Exit code
 */
int main(int argc, char **argv) {
    TaskManager::setCurrentThreadName("Main");
    Window::initNative();
    crash::setupCrashHandlers();

    if (argc > 1) {
        handleCommandLineInterface(argc, argv);
    }

    log::info("Welcome to ImHex {}!", ImHexApi::System::getImHexVersion());
    log::info("Compiled using commit {}@{}", ImHexApi::System::getCommitBranch(), ImHexApi::System::getCommitHash());
    log::info("Running on {} {} ({})", ImHexApi::System::getOSName(), ImHexApi::System::getOSVersion(), ImHexApi::System::getArchitecture());

    ImHexApi::System::impl::setPortableVersion(isPortableVersion());

    return runImHex();
}
