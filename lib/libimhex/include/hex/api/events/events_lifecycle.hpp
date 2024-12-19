#pragma once

#include <hex.hpp>
#include <hex/api/event_manager.hpp>

namespace hex {

    /**
     * @brief Called when Imhex finished startup, and will enter the main window rendering loop
     */
    EVENT_DEF(EventImHexStartupFinished);
    EVENT_DEF(EventImHexClosing);
    EVENT_DEF(EventFirstLaunch);
    EVENT_DEF(EventAnySettingChanged);
    EVENT_DEF(EventAbnormalTermination, int);

    EVENT_DEF(EventImHexUpdated, SemanticVersion, SemanticVersion);

    /**
     * @brief Called when ImHex managed to catch an error in a general try/catch to prevent/recover from a crash
    */
    EVENT_DEF(EventCrashRecovered, const std::exception &);

    /**
     * @brief Called when a project has been loaded
     */
    EVENT_DEF(EventProjectOpened);

}