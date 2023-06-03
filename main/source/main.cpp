#include <hex.hpp>

#include <hex/helpers/logger.hpp>

#include "window.hpp"
#include "crash_handlers.hpp"

#include "init/splash_window.hpp"
#include "init/tasks.hpp"
#include "init/tasks.hpp"

#include <hex/api/task.hpp>
#include <hex/api/project_file_manager.hpp>

#include <wolv/io/fs.hpp>
#include <wolv/utils/guards.hpp>

int main(int argc, char **argv, char **envp) {
    using namespace hex;
    hex::crash::setupCrashHandlers();
    ImHexApi::System::impl::setProgramArguments(argc, argv, envp);

    // Check if ImHex is installed in portable mode
    {
        if (const auto executablePath = wolv::io::fs::getExecutablePath(); executablePath.has_value()) {
            const auto flagFile = executablePath->parent_path() / "PORTABLE";

            if (wolv::io::fs::exists(flagFile) && wolv::io::fs::isRegularFile(flagFile))
                ImHexApi::System::impl::setPortableVersion(true);
        }
    }

    bool shouldRestart = false;
    do {
        // Register an event to handle restarting of ImHex
        EventManager::subscribe<RequestRestartImHex>([&]{ shouldRestart = true; });
        shouldRestart = false;

        // Initialization
        {
            Window::initNative();

            hex::log::info("Welcome to ImHex {}!", IMHEX_VERSION);
            #if defined(GIT_BRANCH) && defined(GIT_COMMIT_HASH_SHORT)
                hex::log::info("Compiled using commit {}@{}", GIT_BRANCH, GIT_COMMIT_HASH_SHORT);
            #endif

            init::WindowSplash splashWindow;

            // Add initialization tasks to run
            TaskManager::init();
            for (const auto &[name, task, async] : init::getInitTasks())
                splashWindow.addStartupTask(name, task, async);

            // Draw the splash window while tasks are running
            if (!splashWindow.loop())
                ImHexApi::System::getInitArguments().insert({ "tasks-failed", {} });
        }

        // Clean up everything after the main window is closed
        ON_SCOPE_EXIT {
            for (const auto &[name, task, async] : init::getExitTasks())
                task();
            TaskManager::exit();
        };

        // Main window
        {
            Window window;
            if (argc == 1)
                ;    // No arguments provided
            else if (argc >= 2) {
                for (auto i = 1; i < argc; i++) {
                    if (auto argument = ImHexApi::System::getProgramArgument(i); argument.has_value())
                        EventManager::post<RequestOpenFile>(argument.value());
                }
            }

            // Open file that has been requested to be opened through other, OS-specific means
            if (auto path = hex::getInitialFilePath(); path.has_value()) {
                EventManager::post<RequestOpenFile>(path.value());
            }

            // Render the main window

            EventManager::post<EventImHexStartupFinished>();
            window.loop();
        }

    } while (shouldRestart);

    return EXIT_SUCCESS;
}
