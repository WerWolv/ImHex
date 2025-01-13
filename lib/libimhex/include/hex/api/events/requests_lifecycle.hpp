#pragma once

#include <hex.hpp>
#include <hex/api/event_manager.hpp>


namespace hex {

    /**
     * @brief Emit a request to add an initialization task to the list.
     *
     * These tasks will be executed at startup.
     *
     * @param std::string Name of the init task
     * @param bool Whether the task is asynchronous (true if yes)
     * @param std::function The function to call to execute the task
     */
    EVENT_DEF(RequestAddInitTask, std::string, bool, std::function<bool()>);

    EVENT_DEF(RequestAddExitTask, std::string, std::function<bool()>);

    EVENT_DEF(RequestCloseImHex, bool);
    EVENT_DEF(RequestRestartImHex);

    EVENT_DEF(RequestInitThemeHandlers);

    EVENT_DEF(RequestStartMigration);

    /**
     * @brief Send an event to the main Imhex instance
     */
    EVENT_DEF(SendMessageToMainInstance, const std::string, const std::vector<u8>&);

}
