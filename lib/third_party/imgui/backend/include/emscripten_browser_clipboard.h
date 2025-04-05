#ifndef EMSCRIPTEN_BROWSER_CLIPBOARD_H_INCLUDED
#define EMSCRIPTEN_BROWSER_CLIPBOARD_H_INCLUDED

#include <string>
#include <emscripten.h>

#define _EM_JS_INLINE(ret, c_name, js_name, params, code)                          \
  extern "C" {                                                                     \
  ret c_name params EM_IMPORT(js_name);                                            \
  EMSCRIPTEN_KEEPALIVE                                                             \
  __attribute__((section("em_js"), aligned(1))) inline char __em_js__##js_name[] = \
    #params "<::>" code;                                                           \
  }

#define EM_JS_INLINE(ret, name, params, ...) _EM_JS_INLINE(ret, name, name, params, #__VA_ARGS__)

namespace emscripten_browser_clipboard {

/////////////////////////////////// Interface //////////////////////////////////

using paste_handler = void(*)(std::string const&, void*);
using copy_handler = char const*(*)(void*);

inline void paste(paste_handler callback, void *callback_data = nullptr);
inline void copy(copy_handler callback, void *callback_data = nullptr);
inline void copy(std::string const &content);

///////////////////////////////// Implementation ///////////////////////////////

namespace {

EM_JS_INLINE(void, paste_js, (paste_handler callback, void *callback_data), {
  /// Register the given callback to handle paste events. Callback data pointer is passed through to the callback.
  /// Paste handler callback signature is:
  ///   void my_handler(std::string const &paste_data, void *callback_data = nullptr);
  document.addEventListener('paste', (event) => {
    Module["ccall"]('paste_return', 'number', ['string', 'number', 'number'], [event.clipboardData.getData('text/plain'), callback, callback_data]);
  });
});

EM_JS_INLINE(void, copy_js, (copy_handler callback, void *callback_data), {
  /// Register the given callback to handle copy events. Callback data pointer is passed through to the callback.
  /// Copy handler callback signature is:
  ///   char const *my_handler(void *callback_data = nullptr);
  document.addEventListener('copy', (event) => {
    const content_ptr = Module["ccall"]('copy_return', 'number', ['number', 'number'], [callback, callback_data]);
    event.clipboardData.setData('text/plain', UTF8ToString(content_ptr));
    event.preventDefault();
  });
});

EM_JS_INLINE(void, copy_async_js, (char const *content_ptr), {
  /// Attempt to copy the provided text asynchronously
  navigator.clipboard.writeText(UTF8ToString(content_ptr));
});

}

inline void paste(paste_handler callback, void *callback_data) {
  /// C++ wrapper for javascript paste call
  paste_js(callback, callback_data);
}

inline void copy(copy_handler callback, void *callback_data) {
  /// C++ wrapper for javascript copy call
  copy_js(callback, callback_data);
}

inline void copy(std::string const &content) {
  /// C++ wrapper for javascript copy call
  copy_async_js(content.c_str());
}

namespace {

extern "C" {

EMSCRIPTEN_KEEPALIVE inline int paste_return(char const *paste_data, paste_handler callback, void *callback_data);

EMSCRIPTEN_KEEPALIVE inline int paste_return(char const *paste_data, paste_handler callback, void *callback_data) {
  /// Call paste callback - this function is called from javascript when the paste event occurs
  callback(paste_data, callback_data);
  return 1;
}

EMSCRIPTEN_KEEPALIVE inline char const *copy_return(copy_handler callback, void *callback_data);

EMSCRIPTEN_KEEPALIVE inline char const *copy_return(copy_handler callback, void *callback_data) {
  /// Call paste callback - this function is called from javascript when the paste event occurs
  return callback(callback_data);
}

}

}

}

#endif // EMSCRIPTEN_BROWSER_CLIPBOARD_H_INCLUDED