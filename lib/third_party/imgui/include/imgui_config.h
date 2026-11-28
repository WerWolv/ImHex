// ImGui config (check imconfig.h for more information about these definitions)
#pragma once

#include <assert.h>

// For alloc() on ARM64 Windows
#if __has_include(<malloc.h>)
    #include <malloc.h>
#endif

namespace hex::dbg {
    [[noreturn]] void assertionHandler(const char* file, int line, const char *function, const char* exprString);
}

#if defined(__PRETTY_FUNCTION__)
    #define IM_ASSERT(_EXPR) do { if (!(_EXPR)) [[unlikely]] { hex::dbg::assertionHandler(__FILE__, __LINE__, __PRETTY_FUNCTION__, #_EXPR); } } while(0)
#elif defined(__FUNCSIG__)
    #define IM_ASSERT(_EXPR) do { if (!(_EXPR)) [[unlikely]] { hex::dbg::assertionHandler(__FILE__, __LINE__, __FUNCSIG__, #_EXPR); } } while(0)
#else
    #define IM_ASSERT(_EXPR) do { if (!(_EXPR)) [[unlikely]] { hex::dbg::assertionHandler(__FILE__, __LINE__, __FUNCTION__, #_EXPR); } } while(0)
#endif

#if defined(_MSC_VER)
    #if EXPORT_SYMBOLS == 1
        #define IMGUI_API           __declspec(dllexport)
        #define IMGUI_IMPL_API      __declspec(dllexport)
        #define IMPLOT_API          __declspec(dllexport)
        #define IMPLOT_IMPL_API     __declspec(dllexport)
        #define IMPLOT3D_API        __declspec(dllexport)
        #define IMPLOT3D_IMPL_API   __declspec(dllexport)
    #else
        #define IMGUI_API           __declspec(dllimport)
        #define IMGUI_IMPL_API      __declspec(dllimport)
        #define IMPLOT_API          __declspec(dllimport)
        #define IMPLOT_IMPL_API     __declspec(dllimport)
        #define IMPLOT3D_API        __declspec(dllimport)
        #define IMPLOT3D_IMPL_API   __declspec(dllimport)
    #endif
#endif

#define IMGUI_ENABLE_TEST_ENGINE

// IMPLOT CONFIG
#define IMPLOT_CUSTOM_NUMERIC_TYPES (ImS8)(ImU8)(ImS16)(ImU16)(ImS32)(ImU32)(ImS64)(ImU64)(float)(double)(long double)

#define IM_OFFSETOF(_TYPE,_MEMBER)  offsetof(_TYPE, _MEMBER)

#define IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS
#define IMGUI_ENABLE_FREETYPE
#define ImDrawIdx unsigned int
#define IMGUI_USE_WCHAR32
#define IMGUI_USE_LEGACY_CRC32_ADLER 1

#if defined(IMGUI_TEST_ENGINE)
    #include "imgui_te_imconfig.h"
#endif
