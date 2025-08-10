#pragma once

#include <hex/api/event_manager.hpp>

/* Forward declarations */
struct GLFWwindow;
using ImGuiID = unsigned int;
namespace hex { class View; }

/* GUI events definitions */
namespace hex {
    /**
     * @brief Signals a newly opened view
     *
     * This event is sent when the view has just been opened by the Window manager.
     *
     * FIXME: This is currently only used for the introduction tutorial.
     *  If the event's only purpose is this, maybe rename it?
     *
     * @param view the new view reference
     */
    EVENT_DEF(EventViewOpened, View*);

    /**
     * @brief Signals a newly closed view
     *
     * This event is sent when the view has just been closed.
     *
     * @param view the closed view reference
     */
    EVENT_DEF(EventViewClosed, View*);

    /**
     * @brief Signals a change in the DPI scale.
     *
     * This event is called once at startup to signal native scale definition (by passing the same value twice).
     * On Windows OS, this event can also be posted if the window DPI has been changed.
     *
     * @param oldScale the old scale
     * @param newScale the current scale that's now in use
     */
    EVENT_DEF(EventDPIChanged, float, float);

    /**
     * @brief Signals the focus of the ImHex main window.
     *
     * This is directly tied as a GLFW window focus callback, and will be called accordingly when GLFW detects
     * a change in focus.
     *
     * @param isFocused true if the window is focused
     */
    EVENT_DEF(EventWindowFocused, bool);

    /**
     * @brief Signals a window being closed.
     *
     * Allows reactive clean up of running tasks, and prevents ImHex from closing
     * by displaying an exit confirmation popup.
     *
     * @param window The window reference
     */
    EVENT_DEF(EventWindowClosing, GLFWwindow*);

    /**
     * @brief Informs that the main window is deinitializing
     *
     * Allows for lifecycle cleanup before ImHex shutdown.
     *
     * @param window The window reference
     */
    EVENT_DEF(EventWindowDeinitializing, GLFWwindow*);

    /**
     * @brief Signals a theme change in the host OS
     *
     * Allows ImHex to react to OS theme changes dynamically during execution.
     */
    EVENT_DEF(EventOSThemeChanged);

}

/* silent (no-logging) GUI events definitions */
namespace hex {

    /**
     * @brief Signals the start of a new ImGui frame
     */
    EVENT_DEF_NO_LOG(EventFrameBegin);

    /**
     * @brief Signals the end of an ImGui frame
     */
    EVENT_DEF_NO_LOG(EventFrameEnd);

    /**
     * @brief Windows OS: Sets the taskbar icon state
     *
     * This event is used on Windows OS to display progress through the taskbar icon (the famous "green loading bar"
     * in the taskbar).
     *
     * @param progressState the progress state (converted from the TaskProgressState enum)
     * @param progressType the type of progress (converted from the TaskProgressType enum)
     * @param percentage actual progress percentage (expected from 0 to 100)
     *
     * @see hex::ImHexApi::System::TaskProgressState
     * @see hex::ImHexApi::System::TaskProgressType
     */
    EVENT_DEF_NO_LOG(EventSetTaskBarIconState, u32, u32, u32);

    /**
     * @brief Informs of an ImGui element being rendered
     *
     * @param elementId the element's ID
     * @param boundingBox the bounding box (composed of 4 floats)
     */
    EVENT_DEF_NO_LOG(EventImGuiElementRendered, ImGuiID, const std::array<float, 4>&);

}
