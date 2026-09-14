#pragma once

#include <hex.hpp>

#include <algorithm>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace hex::ui {

    namespace impl {

        // Where a line's own text starts. Equal to the line's length for a line
        // that is only spaces and tabs.
        inline size_t indentOf(std::string_view line) {
            const auto firstCharacter = line.find_first_not_of(" \t");
            return firstCharacter == std::string_view::npos ? line.size() : firstCharacter;
        }

        inline bool isBlank(std::string_view line) {
            return line.find_first_not_of(" \t") == std::string_view::npos;
        }

        inline bool startsWithComment(std::string_view line, std::string_view commentToken) {
            return line.substr(indentOf(line)).starts_with(commentToken);
        }

    }

    // Comments or uncomments a run of whole lines, the way Ctrl+/ does.
    //
    // Uncomments only when every line with text on it is already commented.
    // A run that is partly commented becomes wholly commented, rather than
    // having each line inverted on its own. One more keypress then clears it.
    //
    // A blank line gets no comment token, and does not decide the direction
    // either: a blank line between two commented lines must not make the run
    // comment a second time.
    //
    // The token goes at the shallowest indent in the run, not at column 0, so
    // a commented block keeps the shape it had.
    //
    // Returns `lines` unchanged when there is no token to apply, or when every
    // line is blank.
    inline std::vector<std::string> toggleLineComments(std::span<const std::string> lines, std::string_view commentToken) {
        std::vector<std::string> result(lines.begin(), lines.end());
        if (commentToken.empty())
            return result;

        bool anyText      = false;
        bool allCommented = true;
        size_t column     = std::string::npos;

        for (const auto &line : lines) {
            if (impl::isBlank(line))
                continue;

            anyText      = true;
            allCommented = allCommented && impl::startsWithComment(line, commentToken);
            column       = std::min(column, impl::indentOf(line));
        }

        if (!anyText)
            return result;

        for (auto &line : result) {
            if (impl::isBlank(line))
                continue;

            if (allCommented) {
                const auto tokenStart = impl::indentOf(line);
                auto text = line.substr(tokenStart + commentToken.size());

                // Takes back the one space the comment step adds, and no more.
                // Anything deeper is the line's own indentation.
                if (text.starts_with(' '))
                    text.erase(0, 1);

                line = line.substr(0, tokenStart) + text;
            } else {
                line.insert(column, std::string(commentToken) + " ");
            }
        }

        return result;
    }

}
