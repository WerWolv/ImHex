#include <hex.hpp>

#include <hex/helpers/utils.hpp>
#include <hex/helpers/logger.hpp>

#include "window.hpp"

#include "init/splash_window.hpp"
#include "init/tasks.hpp"

#include <hex/helpers/file.hpp>

int main(int argc, char **argv, char **envp) {
    using namespace hex;

    // Initialization
    {
        Window::initNative();

        init::WindowSplash splashWindow(argc, argv, envp);

        for (const auto &[name, task] : init::getInitTasks())
            splashWindow.addStartupTask(name, task);

        if (!splashWindow.loop())
            init::getInitArguments().push_back({ "tasks-failed", { } });
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
            ; // No arguments provided
        else if (argc == 2)
            EventManager::post<RequestOpenFile>(argv[1]);
        else {
            hex::log::fatal("Usage: imhex [file_name]");
            return EXIT_FAILURE;
        }

        hex::log::info("Welcome to ImHex!");

        window.loop();
    }

    return EXIT_SUCCESS;
}
