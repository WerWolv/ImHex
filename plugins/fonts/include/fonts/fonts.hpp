#pragma once

#include <hex/api/imhex_api.hpp>

namespace hex::fonts {

    const static auto Default    = []{ return ImHexApi::Fonts::getFont("hex.fonts.font.default");     };
    const static auto HexEditor  = []{ return ImHexApi::Fonts::getFont("hex.fonts.font.hex_editor");  };
    const static auto CodeEditor = []{ return ImHexApi::Fonts::getFont("hex.fonts.font.code_editor"); };

}