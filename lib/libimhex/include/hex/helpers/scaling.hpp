#pragma once

#include <imgui.h>

namespace hex {

    [[nodiscard]] float operator""_scaled(long double value);
    [[nodiscard]] float operator""_scaled(unsigned long long value);
    [[nodiscard]] ImVec2 scaled(const ImVec2 &vector);
    [[nodiscard]] ImVec2 scaled(float x, float y);

}