#include <hex.hpp>

#include <clocale>

#include <hex/helpers/logger.hpp>

#include <window.hpp>
#include <messaging.hpp>

#include "crash_handlers.hpp"

#include <hex/api/task_manager.hpp>
#include <hex/api/plugin_manager.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/trace/exceptions.hpp>

namespace hex::init {

    int runImHex();
    void runCommandLine(int argc, char **argv);

}

/**
 * @brief Main entry point of ImHex
 * @param argc Argument count
 * @param argv Argument values
 * @return Exit code
 */
int main(int argc, char **argv) {
    using namespace hex;

    std::setlocale(LC_ALL, "en_US.utf8");

    // Tell the Task Manager that we are the main thread
    TaskManager::setMainThreadId(std::this_thread::get_id());

    // Set the main thread's name. This is the name that will be displayed
    // in tools like btop
    TaskManager::setCurrentThreadName("ImHex 🔍");

    // Setup crash handlers right away to catch crashes as early as possible
    crash::setupCrashHandlers();

    // Enable exception tracing on the main thread
    trace::enableExceptionCaptureForCurrentThread();

    // Run platform-specific initialization code
    Window::initNative();

    // Setup messaging system to allow sending commands to the main ImHex instance
    messaging::setupMessaging();

    // Handle command line arguments if any have been passed
    if (argc > 1) {
        init::runCommandLine(argc, argv);
    }


    // Log some system information to aid debugging when users share their logs
    log::info("Welcome to ImHex {}!", ImHexApi::System::getImHexVersion().get());
    log::info("Compiled using commit {}@{}", ImHexApi::System::getCommitBranch(), ImHexApi::System::getCommitHash());
    log::info("Running on {} {} ({})", ImHexApi::System::getOSName(), ImHexApi::System::getOSVersion(), ImHexApi::System::getArchitecture());

    #if defined(OS_LINUX)
        if (auto distro = ImHexApi::System::getLinuxDistro(); distro.has_value()) {
            log::info("Linux distribution: {}. Version: {}", distro->name, distro->version.empty() ? "None" : distro->version);
        }
    #endif
    
    // Run ImHex
    return init::runImHex();
}
