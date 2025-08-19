#pragma once

#include <hex/api/event_manager.hpp>

/* GUI requests definitions */
namespace hex {

    /**
     * @brief Requests the opening of a new window.
     *
     * @param name the window's name
     */
    EVENT_DEF(RequestOpenWindow, std::string);

    /**
     * @brief Centralized request to update ImHex's main window title
     *
     * This request can be called to make ImHex refresh its main window title, taking into account a new project
     * or file opened/closed.
     */
    EVENT_DEF(RequestUpdateWindowTitle);

    /**
     * @brief Requests a theme type (light or dark) change
     *
     * @param themeType either `Light` or `Dark`
     */
    EVENT_DEF(RequestChangeTheme, std::string);

    /**
     * @brief Requests the opening of a popup
     *
     * @param name the popup's name
     */
    EVENT_DEF(RequestOpenPopup, std::string);

    /**
     * @brief Requests updating of the active post-processing shader
     *
     * @param vertexShader the vertex shader source code
     * @param fragmentShader the fragment shader source code
     */
    EVENT_DEF(RequestSetPostProcessingShader, std::string, std::string);
}
