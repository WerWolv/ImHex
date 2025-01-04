#include <hex/api/event_manager.hpp>
#include <hex/api/task_manager.hpp>
#include <hex/helpers/utils.hpp>

#include <init/splash_window.hpp>
#include <init/tasks.hpp>

namespace hex::init {

    /**
     * @brief Handles a file open request by opening the file specified by OS-specific means
     */
    void handleFileOpenRequest() {
        if (auto path = hex::getInitialFilePath(); path.has_value()) {
            RequestOpenFile::post(path.value());
        }
    }

    /**
     * @brief Displays ImHex's splash screen and runs all initialization tasks. The splash screen will be displayed until all tasks have finished.
     */
    [[maybe_unused]]
    std::unique_ptr<init::WindowSplash> initializeImHex() {
        auto splashWindow = std::make_unique<init::WindowSplash>();

        log::info("Using '{}' GPU", ImHexApi::System::getGPUVendor());

        // Add initialization tasks to run
        TaskManager::init();
        for (const auto &[name, task, async, running] : init::getInitTasks())
            splashWindow->addStartupTask(name, task, async);

        splashWindow->startStartupTasks();

        return splashWindow;
    }


    /**
     * @brief Deinitializes ImHex by running all exit tasks
     */
    void deinitializeImHex() {
        // Run exit tasks
        init::runExitTasks();

    }

}