#include <hex.hpp>

#include "window.hpp"

#include "init/splash_window.hpp"
#include "init/tasks.hpp"


int main(int argc, char **argv) {
    using namespace hex;

    // Initialization
    {
        init::WindowSplash splashWindow(argc, argv);

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
            EventManager::post<EventFileDropped>(argv[1]);
        else {
            hex::log::fatal("Usage: imhex [file_name]");
            return EXIT_SUCCESS;
        }

        hex::log::info("Welcome to ImHex!");

        window.loop();
    }

    return EXIT_SUCCESS;
}
