#pragma once

#include <hex/api/event_manager.hpp>

struct ImGuiTestEngine;

/* Lifecycle events definitions */
namespace hex {

    /**
     * @brief Called when Imhex finished startup, and will enter the main window rendering loop
     */
    EVENT_DEF(EventImHexStartupFinished);

    /**
     * @brief Called when ImHex is closing, to trigger the last shutdown hooks
     *
     * This is the last event to fire before complete graceful shutdown.
     */
    EVENT_DEF(EventImHexClosing);

    /**
     * @brief Signals that it's ImHex first launch ever
     *
     * This event allows for the launch of the ImHex tutorial (also called Out of Box experience).
     */
    EVENT_DEF(EventFirstLaunch);

    /**
     * FIXME: this event is unused and should be scrapped.
     */
    EVENT_DEF(EventAnySettingChanged);

    /**
     * @brief Ensures correct plugin cleanup on crash
     *
     * This event is fired when catching an unexpected error that cannot be recovered and
     * which forces Imhex to close immediately.
     *
     * Subscribing to this event ensures that the plugin can correctly clean up any mission-critical tasks
     * before forceful shutdown.
     *
     * @param signal the POSIX signal code
     */
    EVENT_DEF(EventAbnormalTermination, int);

    /**
     * @brief Informs of the ImHex versions (and difference, if any)
     *
     * Called on every startup to inform subscribers of the two versions picked up:
     * - the version of the previous launch, gathered from the settings file
     * - the current version, gathered directly from C++ code
     *
     * In most cases, and unless ImHex was updated, the two parameters will be the same.
     *
     * FIXME: Maybe rename the event to signal a startup information, instead of the misleading
     *  title that the event could be fired when ImHex detects that it was updated since last launch?
     *
     * @param previousLaunchVersion ImHex's version during the previous launch
     * @param currentVersion ImHex's current version for this startup
     */
    EVENT_DEF(EventImHexUpdated, SemanticVersion, SemanticVersion);

    /**
     * @brief Called when ImHex managed to catch an error in a general try/catch to prevent/recover from a crash
    */
    EVENT_DEF(EventCrashRecovered, const std::exception &);

    /**
     * @brief Called when a project has been loaded
     */
    EVENT_DEF(EventProjectOpened);

    /**
     * @brief Called when a native message was received from another ImHex instance
     * @param rawData Raw bytes received from other instance
     */
    EVENT_DEF(EventNativeMessageReceived, std::vector<u8>);

    /**
     * @brief Called when ImGui is initialized to register tests
     * @param testEngine Pointer to the ImGui Test Engine Context
     */
    EVENT_DEF(EventRegisterImGuiTests, ImGuiTestEngine*);

}