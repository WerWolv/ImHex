#pragma once

#include <hex/api/event_manager.hpp>

/* Forward declarations */
struct GLFWwindow;
namespace hex {
    class View;
}

namespace hex {

    EVENT_DEF(EventViewOpened, View*);
    EVENT_DEF(EventDPIChanged, float, float);

    EVENT_DEF(EventWindowFocused, bool);
    EVENT_DEF(EventWindowClosing, GLFWwindow *);

    EVENT_DEF(EventWindowInitialized);
    EVENT_DEF(EventWindowDeinitializing, GLFWwindow *);

    EVENT_DEF(EventOSThemeChanged);

    EVENT_DEF_NO_LOG(EventFrameBegin);
    EVENT_DEF_NO_LOG(EventFrameEnd);
    EVENT_DEF_NO_LOG(EventSetTaskBarIconState, u32, u32, u32);
    EVENT_DEF_NO_LOG(EventImGuiElementRendered, ImGuiID, const std::array<float, 4>&);

}