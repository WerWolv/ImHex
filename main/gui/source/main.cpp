#include <hex.hpp>

#include <hex/helpers/logger.hpp>

#include "window.hpp"
#include "crash_handlers.hpp"
#include "messaging.hpp"

#include "init/splash_window.hpp"
#include "init/tasks.hpp"

#include <hex/api/task.hpp>
#include <hex/api/project_file_manager.hpp>
#include <hex/api/plugin_manager.hpp>

#include <hex/helpers/fs.hpp>
#include "hex/subcommands/subcommands.hpp"

#include <wolv/io/fs.hpp>
#include <wolv/utils/guards.hpp>

#if defined(OS_EMSCRIPTEN)
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
        // Skip the first argument (the executable path) and convert the rest to a vector of strings
        std::vector<std::string> args(argv + 1, argv + argc);

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
            ImHexApi::System::getInitArguments().insert({ "tasks-failed", {} });
    }


    /**
     * @brief Deinitializes ImHex by running all exit tasks and terminating all asynchronous tasks
     */
    void deinitializeImHex() {
        // Run exit tasks
        for (const auto &[name, task, async] : init::getExitTasks())
            task();

        // Terminate all asynchronous tasks
        TaskManager::exit();
    }

    /**
     * @brief Handles a file open request by opening the file specified by OS-specific means
     */
    void handleFileOpenRequest() {
        if (auto path = hex::getInitialFilePath(); path.has_value()) {
            EventManager::post<RequestOpenFile>(path.value());
        }
    }


    #if defined(OS_EMSCRIPTEN)
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

            // Draw the splash window while tasks are running
            emscripten_set_main_loop_arg([](void *arg) {
                auto splashWindow = reinterpret_cast<WindowSplash*>(arg);

                FrameResult res = splashWindow->fullFrame();
                if (res == FrameResult::success) {
                    handleFileOpenRequest();

                    // Clean up everything after the main window is closed
                    emscripten_set_beforeunload_callback(nullptr, [](int eventType, const void *reserved, void *userData){
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
                EventManager::subscribe<RequestRestartImHex>([&] {
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
    Window::initNative();
    hex::crash::setupCrashHandlers();

    if (argc > 1) {
        handleCommandLineInterface(argc, argv);
    }

    log::info("Welcome to ImHex {}!", ImHexApi::System::getImHexVersion());
    log::info("Compiled using commit {}@{}", ImHexApi::System::getCommitBranch(), ImHexApi::System::getCommitHash());
    log::info("Running on {} {} ({})", ImHexApi::System::getOSName(), ImHexApi::System::getOSVersion(), ImHexApi::System::getArchitecture());

    ImHexApi::System::impl::setPortableVersion(isPortableVersion());

    return runImHex();
};
