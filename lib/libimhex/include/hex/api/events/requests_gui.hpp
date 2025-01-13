#pragma once

#include <hex.hpp>
#include <hex/api/event_manager.hpp>


namespace hex {

    EVENT_DEF(RequestOpenWindow, std::string);

    EVENT_DEF(RequestUpdateWindowTitle);

    EVENT_DEF(RequestChangeTheme, std::string);


    EVENT_DEF(RequestOpenPopup, std::string);

}
