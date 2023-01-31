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

    bool shouldRestart = false;

    // Check if ImHex is installed in portable mode
    {
        if (const auto executablePath = fs::getExecutablePath(); executablePath.has_value()) {
            const auto flagFile = executablePath->parent_path() / "PORTABLE";

            if (fs::exists(flagFile) && fs::isRegularFile(flagFile))
                ImHexApi::System::impl::setPortableVersion(true);
        }
    }

    do {
        EventManager::subscribe<RequestRestartImHex>([&]{ shouldRestart = true; });
        shouldRestart = false;

        // Initialization
        {
            Window::initNative();

            hex::log::info("Welcome to ImHex {}!", IMHEX_VERSION);

            init::WindowSplash splashWindow;

            TaskManager::init();
            for (const auto &[name, task, async] : init::getInitTasks())
                splashWindow.addStartupTask(name, task, async);

            if (!splashWindow.loop())
                ImHexApi::System::getInitArguments().insert({ "tasks-failed", {} });
        }

        // Clean up
        ON_SCOPE_EXIT {
            for (const auto &[name, task, async] : init::getExitTasks())
                task();
            TaskManager::exit();
        };

        // Main window
        {
            if (argc == 1)
                ;    // No arguments provided
            else if (argc >= 2) {
                for (auto i = 1; i < argc; i++) {
                    if (auto argument = ImHexApi::System::getProgramArgument(i); argument.has_value())
                        EventManager::post<RequestOpenFile>(argument.value());
                }
            }

            Window window;
            window.loop();
        }

    } while (shouldRestart);

    return EXIT_SUCCESS;
}
