#if !defined(OS_WEB)

    #include <hex/api/events/requests_lifecycle.hpp>
    #include <hex/api/imhex_api/system.hpp>
    #include <wolv/utils/guards.hpp>

    #include <init/run.hpp>
    #include <window.hpp>
    #include <window_backend.hpp>

    namespace hex::init {

        int runImHex() {
            if (!initializeWindowing()) {
                log::fatal("Failed to initialize the window backend!");
                std::abort();
            }
            ON_SCOPE_EXIT { shutdownWindowing(); };

            bool shouldRestart = false;
            do {
                // Register an event handler that will make ImHex restart when requested
                shouldRestart = false;
                RequestRestartImHex::subscribe([&] {
                    shouldRestart = true;
                });

                // Splash window
                {
                    auto splashWindow = initializeImHex();
                    // Draw the splash window while tasks are running

                    while (true) {
                        const auto result = splashWindow->loop();
                        if (result.has_value()) {
                            if (!result.value()) {
                                ImHexApi::System::impl::addInitArgument("tasks-failed");
                            }

                            break;
                        }
                    }

                    handleFileOpenRequest();
                }

                // Main window
                {
                    Window window;
                    initializationFinished();

                    window.loop();
                }

                deinitializeImHex();
            } while (shouldRestart);

            return EXIT_SUCCESS;
        }

    }

#endif
