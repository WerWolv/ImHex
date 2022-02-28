#include <hex.hpp>

#include <hex/helpers/utils.hpp>
#include <hex/helpers/logger.hpp>

#include "window.hpp"

#include "init/splash_window.hpp"
#include "init/tasks.hpp"

int main(int argc, char **argv, char **envp) {
    using namespace hex;
    ImHexApi::System::impl::setProgramArguments(argc, argv, envp);


#if defined(OS_WINDOWS)
    ImHexApi::System::impl::setBorderlessWindowMode(true);
#endif

    // Initialization
    {
        Window::initNative();

        hex::log::info("Welcome to ImHex!");

        init::WindowSplash splashWindow;

        // Intel's OpenGL driver has weird bugs that cause the drawn window to be offset to the bottom right.
        // This can be fixed by either using Mesa3D's OpenGL Software renderer or by simply disabling it.
        // If you want to try if it works anyways on your GPU, set the hex.builtin.setting.interface.force_borderless_window_mode setting to 1
        if (ImHexApi::System::isBorderlessWindowModeEnabled()) {
            bool isIntelGPU = hex::containsIgnoreCase(splashWindow.getGPUVendor(), "Intel");
            ImHexApi::System::impl::setBorderlessWindowMode(!isIntelGPU);

            if (isIntelGPU)
                log::warn("Intel GPU detected! Intel's OpenGL driver has bugs that can cause issues when using ImHex. If you experience any rendering bugs, please try the Mesa3D Software Renderer");
        }

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
        else if (argc == 2)
            EventManager::post<RequestOpenFile>(argv[1]);
        else {
            hex::log::fatal("Usage: {} [<file_name>]", argv[0]);
            return EXIT_FAILURE;
        }

        window.loop();
    }

    return EXIT_SUCCESS;
}
