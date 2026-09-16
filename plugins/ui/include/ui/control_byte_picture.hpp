#pragma once

#include <string>
#include <hex/helpers/unicode.hpp>
#include <string_view>

namespace hex::ui {

    /**
     * @brief The picture character standing in for a NUL byte in an edit box
     *
     * ImGui's InputText is null-terminated, so a raw NUL would end the
     * buffer there. Shown only while editing; converted back before every
     * encode check and commit. Ctrl+C is the one exception - InputText
     * copies the buffer straight to the clipboard, picture and all.
     *
     * Not reversible for text that already holds a real U+2400: nothing in
     * a null-terminated buffer can tell the two apart, so editing such a
     * string changes it.
     */
    constexpr inline char NulPicture[] = { '\xE2', '\x90', '\x80' };

    /**
     * @brief Replaces each NUL byte with NulPicture, so an edit box can hold the whole value
     */
    inline std::string nulToPicture(std::string_view text) {
        std::string result;
        for (const char byte : text) {
            if (byte == '\0')
                result.append(NulPicture, sizeof(NulPicture));
            else
                result += byte;
        }
        return result;
    }

    /**
     * @brief Inverse of nulToPicture(), for text on its way back out of an edit box
     */
    inline std::string pictureToNul(std::string_view text) {
        std::string result;
        for (size_t i = 0; i < text.size(); i += 1) {
            if (i + sizeof(NulPicture) <= text.size() && text.substr(i, sizeof(NulPicture)) == std::string_view(NulPicture, sizeof(NulPicture))) {
                result += '\0';
                i += sizeof(NulPicture) - 1;
            } else {
                result += text[i];
            }
        }
        return result;
    }

}
