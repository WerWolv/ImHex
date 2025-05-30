// dear imgui test engine
// (template for compile-time configuration)
// Replicate or #include this file in your imconfig.h to enable test engine.

// Compile Dear ImGui with test engine hooks
// (Important: This is a value-less define, to be consistent with other defines used in core dear imgui.)
#define IMGUI_ENABLE_TEST_ENGINE

// [Optional, default 0] Enable plotting of perflog data for comparing performance of different runs.
// This feature requires ImPlot to be linked in the application.
#ifndef IMGUI_TEST_ENGINE_ENABLE_IMPLOT
#define IMGUI_TEST_ENGINE_ENABLE_IMPLOT 1
#endif

// [Optional, default 1] Enable screen capture and PNG/GIF saving functionalities
// There's not much point to disable this but we provide it to reassure user that the dependencies on imstb_image_write.h and ffmpeg are technically optional.
#ifndef IMGUI_TEST_ENGINE_ENABLE_CAPTURE
#define IMGUI_TEST_ENGINE_ENABLE_CAPTURE 1
#endif

// [Optional, default 0] Using std::function and <functional> for function pointers such as ImGuiTest::TestFunc and ImGuiTest::GuiFunc
#ifndef IMGUI_TEST_ENGINE_ENABLE_STD_FUNCTION
#define IMGUI_TEST_ENGINE_ENABLE_STD_FUNCTION 1
#endif

// [Optional, default 0] Automatically fill ImGuiTestEngineIO::CoroutineFuncs with a default implementation using std::thread
#ifndef IMGUI_TEST_ENGINE_ENABLE_COROUTINE_STDTHREAD_IMPL
#define IMGUI_TEST_ENGINE_ENABLE_COROUTINE_STDTHREAD_IMPL 1
#endif

// [Optional, default 0] Disable calls that do not make sense on game consoles
// (Disable: system(), popen(), sigaction(), colored TTY output)
#ifndef IMGUI_TEST_ENGINE_IS_GAME_CONSOLE
#if defined(__ORBIS__) || defined(__PROSPERO__) || defined(_GAMING_XBOX) || defined(_DURANGO)
#define IMGUI_TEST_ENGINE_IS_GAME_CONSOLE 1
#else
#define IMGUI_TEST_ENGINE_IS_GAME_CONSOLE 0
#endif
#endif

// Define IM_DEBUG_BREAK macros so it is accessible in imgui.h
// (this is a conveniance for app using test engine may define an IM_ASSERT() that uses this instead of an actual assert)
// (this is a copy of the block in imgui_internal.h. if the one in imgui_internal.h were to be defined at the top of imgui.h we wouldn't need this)
#ifndef IM_DEBUG_BREAK
#if defined (_MSC_VER)
#define IM_DEBUG_BREAK()    __debugbreak()
#elif defined(__clang__)
#define IM_DEBUG_BREAK()    __builtin_debugtrap()
#elif defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
#define IM_DEBUG_BREAK()    __asm__ volatile("int $0x03")
#elif defined(__GNUC__) && defined(__thumb__)
#define IM_DEBUG_BREAK()    __asm__ volatile(".inst 0xde01")
#elif defined(__GNUC__) && defined(__arm__) && !defined(__thumb__)
#define IM_DEBUG_BREAK()    __asm__ volatile(".inst 0xe7f001f0");
#else
#define IM_DEBUG_BREAK()    IM_ASSERT(0)    // It is expected that you define IM_DEBUG_BREAK() into something that will break nicely in a debugger!
#endif
#endif // #ifndef IMGUI_DEBUG_BREAK

#ifndef IMGUI_API
#define IMGUI_API
#endif

// [Options] We provide custom assert macro used by our our test suite, which you may use:
// - Calling IM_DEBUG_BREAK() instead of an actual assert, so we can easily recover and step over (compared to many assert implementations).
// - If a test is running, test name will be included in the log.
// - Macro is calling IM_DEBUG_BREAK() inline to get debugger to break in the calling function (instead of a deeper callstack level).
// - Macro is using comma operator instead of an if() to avoid "conditional expression is constant" warnings.
IMGUI_API extern void ImGuiTestEngine_AssertLog(const char* expr, const char* file, const char* func, int line);
#define IM_TEST_ENGINE_ASSERT(_EXPR)    do { if ((void)0, !(_EXPR)) { ImGuiTestEngine_AssertLog(#_EXPR, __FILE__, __func__, __LINE__); IM_DEBUG_BREAK(); } } while (0)
// V_ASSERT_CONTRACT, assertMacro:IM_ASSERT
