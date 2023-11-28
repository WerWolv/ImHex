#include <hex/api/imhex_api.hpp>

#include <romfs/romfs.hpp>

#include <hex/helpers/utils.hpp>

#include <fonts/fontawesome_font.h>
#include <fonts/codicons_font.h>

namespace hex::plugin::builtin {

    void loadFonts() {
        ImHexApi::Fonts::loadFont("Unifont", romfs::get("fonts/unifont.otf").span<u8>());
        ImHexApi::Fonts::loadFont("VS Codicons", romfs::get("fonts/codicons.ttf").span<u8>(), { { ICON_MIN_VS, ICON_MAX_VS } }, { 0, 3_scaled });
        ImHexApi::Fonts::loadFont("Font Awesome 5", romfs::get("fonts/fontawesome.otf").span<u8>(), { { ICON_MIN_FA, ICON_MAX_FA } }, { 0, 3_scaled });
    }

}