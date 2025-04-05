#if !defined(OS_WEB)

    #include <hex/api/events/requests_lifecycle.hpp>
    #include <wolv/utils/guards.hpp>

    #include <init/run.hpp>
    #include <window.hpp>

    #include <GLFW/glfw3.h>

    namespace hex::init {

        int runImHex() {

            bool shouldRestart = false;
            do {
                // Register an event handler that will make ImHex restart when requested
                shouldRestart = false;
                RequestRestartImHex::subscribe([&] {
                    shouldRestart = true;
                });

                {
                    auto splashWindow = initializeImHex();
                    // Draw the splash window while tasks are running
                    if (!splashWindow->loop())
                        ImHexApi::System::impl::addInitArgument("tasks-failed");

                    handleFileOpenRequest();
                }

                {
                    // Initialize GLFW
                    if (!glfwInit()) {
                        log::fatal("Failed to initialize GLFW!");
                        std::abort();
                    }
                    ON_SCOPE_EXIT { glfwTerminate(); };

                    // Main window
                    {
                        Window window;
                        window.loop();
                    }

                    deinitializeImHex();
                }
            } while (shouldRestart);

            return EXIT_SUCCESS;
        }

    }

#endif