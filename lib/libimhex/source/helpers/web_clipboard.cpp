#include <hex/helpers/web_clipboard.hpp>

#if defined(OS_WEB)

#include <emscripten.h>
#include <emscripten/bind.h>
#include <emscripten/val.h>

namespace hex::web {

    static std::function<void(const std::string&)> s_clipboardCallback;

    // JavaScript function to set clipboard text
    EM_JS(int, js_set_clipboard_text, (const char* text), {
        if (!navigator.clipboard) {
            console.warn('Clipboard API not available');
            return 0;
        }
        
        try {
            navigator.clipboard.writeText(UTF8ToString(text)).then(function() {
                console.log('Text copied to clipboard');
            }).catch(function(err) {
                console.error('Failed to copy text: ', err);
            });
            return 1;
        } catch (err) {
            console.error('Clipboard write failed: ', err);
            return 0;
        }
    });

    // JavaScript function to get clipboard text
    EM_JS(void, js_get_clipboard_text, (), {
        if (!navigator.clipboard) {
            console.warn('Clipboard API not available');
            Module._clipboard_read_callback('');
            return;
        }
        
        navigator.clipboard.readText().then(function(text) {
            Module._clipboard_read_callback(text);
        }).catch(function(err) {
            console.error('Failed to read clipboard: ', err);
            Module._clipboard_read_callback('');
        });
    });

    // JavaScript function to check clipboard availability
    EM_JS(int, js_is_clipboard_available, (), {
        return navigator.clipboard ? 1 : 0;
    });

    // C++ callback function called from JavaScript
    extern "C" {
        EMSCRIPTEN_KEEPALIVE
        void clipboard_read_callback(const char* text) {
            if (s_clipboardCallback) {
                s_clipboardCallback(std::string(text));
                s_clipboardCallback = nullptr;
            }
        }
    }

    bool WebClipboard::setClipboardText(const std::string& text) {
        return js_set_clipboard_text(text.c_str()) == 1;
    }

    void WebClipboard::getClipboardText(std::function<void(const std::string&)> callback) {
        s_clipboardCallback = std::move(callback);
        js_get_clipboard_text();
    }

    bool WebClipboard::isClipboardAvailable() {
        return js_is_clipboard_available() == 1;
    }

    void WebClipboard::initialize() {
        // Set up the callback function for JavaScript to call
        emscripten::function("_clipboard_read_callback", &clipboard_read_callback);
    }

}

#endif