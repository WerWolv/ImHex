#include <hex/helpers/scaling.hpp>

#include <hex/api/imhex_api/system.hpp>

namespace hex {

    float operator""_scaled(long double value) {
        return value * ImHexApi::System::getGlobalScale();
    }

    float operator""_scaled(unsigned long long value) {
        return value * ImHexApi::System::getGlobalScale();
    }

    ImVec2 scaled(const ImVec2 &vector) {
        return vector * ImHexApi::System::getGlobalScale();
    }

    ImVec2 scaled(float x, float y) {
        return ImVec2(x, y) * ImHexApi::System::getGlobalScale();
    }

}