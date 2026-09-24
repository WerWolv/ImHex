#include <ui/line_comment.hpp>

#include <algorithm>

namespace hex::ui {

    namespace {

        /**
         * @brief Gets the index of the first character that is not a space or tab
         * @return The length of `line` if there is no such character
         */
        size_t indentOf(std::string_view line) {
            const auto firstCharacter = line.find_first_not_of(" \t");
            return firstCharacter == std::string_view::npos ? line.size() : firstCharacter;
        }

        bool isBlank(std::string_view line) {
            return indentOf(line) == line.size();
        }

        bool startsWithComment(std::string_view line, std::string_view commentToken) {
            return line.substr(indentOf(line)).starts_with(commentToken);
        }

    }

    std::vector<std::string> toggleLineComments(std::span<const std::string> lines, std::string_view commentToken) {
        std::vector<std::string> result(lines.begin(), lines.end());
        if (commentToken.empty())
            return result;

        bool anyText      = false;
        bool allCommented = true;
        size_t column     = std::string::npos;

        for (const auto &line : lines) {
            if (isBlank(line))
                continue;

            anyText      = true;
            allCommented = allCommented && startsWithComment(line, commentToken);
            column       = std::min(column, indentOf(line));
        }

        if (!anyText)
            return result;

        for (auto &line : result) {
            if (isBlank(line))
                continue;

            if (allCommented) {
                const auto tokenStart = indentOf(line);
                auto text = line.substr(tokenStart + commentToken.size());

                // Remove only the space that commenting adds.
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
