#include <hex/api/achievement_manager.hpp>
#include <hex/api/events/requests_interaction.hpp>
#include <hex/api/task_manager.hpp>
#include <hex/api/imhex_api/system.hpp>
#include <hex/api/content_registry/settings.hpp>
#include <hex/api/tutorial_manager.hpp>
#include <hex/api/shortcut_manager.hpp>

#include <hex/api/events/events_lifecycle.hpp>

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

        splashWindow->startStartupTaskExecution();

        return splashWindow;
    }

    void initializationFinished() {
        ContentRegistry::Settings::impl::load();
        ContentRegistry::Settings::impl::store();

        AchievementManager::loadProgress();

        EventImHexStartupFinished::post();

        TutorialManager::init();

        #if defined(OS_MACOS)
            ShortcutManager::enableMacOSMode();
        #endif
    }


    /**
     * @brief Deinitializes ImHex by running all exit tasks
     */
    void deinitializeImHex() {
        ContentRegistry::Settings::impl::store();

        // Run exit tasks
        init::runExitTasks();

    }

}