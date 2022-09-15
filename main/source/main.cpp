#include <hex.hpp>

#include <hex/helpers/utils.hpp>
#include <hex/helpers/logger.hpp>

#include "window.hpp"

#include "init/splash_window.hpp"
#include "init/tasks.hpp"

#include <hex/api/project_file_manager.hpp>

int main(int argc, char **argv, char **envp) {
    using namespace hex;
    ImHexApi::System::impl::setProgramArguments(argc, argv, envp);

#if defined(OS_WINDOWS)
    ImHexApi::System::impl::setBorderlessWindowMode(true);
#endif

    bool shouldRestart = false;

    EventManager::subscribe<RequestRestartImHex>([&]{ shouldRestart = true; });

    do {
        shouldRestart = false;

        // Initialization
        {
            Window::initNative();

            hex::log::info("Welcome to ImHex!");

            init::WindowSplash splashWindow;

            for (const auto &[name, task] : init::getInitTasks())
                splashWindow.addStartupTask(name, task);

            if (!splashWindow.loop())
                ImHexApi::System::getInitArguments().insert({ "tasks-failed", {} });
        }

        // Clean up
        ON_SCOPE_EXIT {
            for (const auto &[name, task] : init::getExitTasks())
                task();
        };

        // Main window
        {
            Window window;

            if (argc == 1)
                ;    // No arguments provided
            else if (argc >= 2) {
                for (auto i = 1; i < argc; i++)
                    EventManager::post<RequestOpenFile>(argv[i]);
            }

            window.loop();
        }

    } while (shouldRestart);

    return EXIT_SUCCESS;
}
