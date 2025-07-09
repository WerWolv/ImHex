#pragma once

#include <hex/api/imhex_api.hpp>

namespace hex::fonts {

    [[nodiscard]] const ImHexApi::Fonts::Font& Default();
    [[nodiscard]] const ImHexApi::Fonts::Font& HexEditor();
    [[nodiscard]] const ImHexApi::Fonts::Font& CodeEditor();

}