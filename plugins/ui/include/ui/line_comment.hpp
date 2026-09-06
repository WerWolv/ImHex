#pragma once

#include <hex.hpp>

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace hex::ui {

    /**
     * @brief Comments or uncomments lines
     *
     * Uncomments only if all lines with text have a comment. Skips blank lines.
     * Puts the token at the smallest indent.
     *
     * @return `lines` unchanged if `commentToken` is empty or all lines are blank
     */
    std::vector<std::string> toggleLineComments(std::span<const std::string> lines, std::string_view commentToken);

}
