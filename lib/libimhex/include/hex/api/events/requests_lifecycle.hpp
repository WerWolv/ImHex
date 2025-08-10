#pragma once

#include <hex.hpp>
#include <hex/api/event_manager.hpp>

/* Lifecycle requests definitions */
namespace hex {

    /**
     * @brief Emit a request to add an initialization task to the list
     *
     * These tasks will be executed at startup.
     *
     * @param name Name of the init task
     * @param isAsync Whether the task is asynchronous (true if yes)
     * @param callbackFunction The function to call to execute the task
     */
    EVENT_DEF(RequestAddInitTask, std::string, bool, std::function<bool()>);

    /**
     * @brief Emit a request to add an exit task to the list
     *
     * These tasks will be executed during the exit phase.
     *
     * FIXME: request is unused and should be scrapped.
     *
     * @param name Name of the exit task
     * @param callbackFunction The function to call to execute the task
     */
    EVENT_DEF(RequestAddExitTask, std::string, std::function<bool()>);

    /**
     * @brief Requests ImHex's graceful shutdown
     *
     * If there are no questions (bool set to true), ImHex closes immediately.
     * If set to false, there is a procedure run to prompt a confirmation to the user.
     *
     * @param noQuestions true if no questions
     */
    EVENT_DEF(RequestCloseImHex, bool);

    /**
     * @brief Requests ImHex's restart
     *
     * This event is necessary for ImHex to restart in the main loop for native and web platforms,
     * as ImHex cannot simply close and re-open.
     *
     * This event serves no purpose on Linux, Windows and macOS platforms.
     */
    EVENT_DEF(RequestRestartImHex);

    /**
     * @brief Requests the initialization of theme handlers
     *
     * This is called during ImGui bootstrapping, and should not be called at any other time.
     */
    EVENT_DEF(RequestInitThemeHandlers);

    /**
     * @brief Send a subcommand to the main Imhex instance
     *
     * This request is called to send a subcommand to the main ImHex instance.
     * This subcommand will then be executed by a handler when ImHex finishing initializing
     * (`EventImHexStartupFinished`).
     *
     * FIXME: change the name so that it is prefixed with "Request" like every other request.
     *
     * @param name the subcommand's name
     * @param data the subcommand's data
     */
    EVENT_DEF(SendMessageToMainInstance, const std::string, const std::vector<u8>&);

}
