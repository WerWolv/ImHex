#if !defined(OS_WEB)

    #include <hex/api/event_manager.hpp>
    #include <wolv/utils/guards.hpp>

    #include <init/run.hpp>
    #include <window.hpp>

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

                // Main window
                {
                    Window window;
                    window.loop();
                }

                deinitializeImHex();

            } while (shouldRestart);

            return EXIT_SUCCESS;
        }

    }

#endif