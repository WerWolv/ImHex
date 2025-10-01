#pragma once

#if defined(OS_WEB)

#include <string>
#include <functional>

namespace hex::web {

    /**
     * @brief Web-specific clipboard operations for Emscripten builds
     */
    class WebClipboard {
    public:
        /**
         * @brief Set clipboard text using the Web Clipboard API
         * @param text Text to copy to clipboard
         * @return true if successful, false otherwise
         */
        static bool setClipboardText(const std::string& text);

        /**
         * @brief Get clipboard text using the Web Clipboard API
         * @param callback Callback function to handle the clipboard text
         */
        static void getClipboardText(std::function<void(const std::string&)> callback);

        /**
         * @brief Check if clipboard API is available
         * @return true if clipboard API is supported
         */
        static bool isClipboardAvailable();

        /**
         * @brief Initialize web clipboard functionality
         */
        static void initialize();
    };

}

#endif