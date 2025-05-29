// dear imgui test engine
// (core)
// This is the interface that your initial setup (app init, main loop) will mostly be using.
// Actual tests will mostly use the interface of imgui_te_context.h

// This file is governed by the "Dear ImGui Test Engine License".
// Details of the license are provided in the LICENSE.txt file in the same directory.

#pragma once

#include "imgui.h"
#include "imgui_internal.h"         // ImPool<>, ImRect, ImGuiItemStatusFlags, ImFormatString

#if defined(__clang__)
#pragma clang diagnostic push
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wclass-memaccess"                  // [__GNUC__ >= 8] warning: 'memset/memcpy' clearing/writing an object of type 'xxxx' with no trivial copy-assignment; use assignment or value-initialization instead
#endif
#ifdef Status // X11 headers
#undef Status
#endif

//-----------------------------------------------------------------------------
// Function Pointers
//-----------------------------------------------------------------------------

#if IMGUI_TEST_ENGINE_ENABLE_STD_FUNCTION
#include <functional>
#define ImFuncPtr(FUNC_TYPE)        std::function<FUNC_TYPE>
#else
#define ImFuncPtr(FUNC_TYPE)        FUNC_TYPE*
#endif

#include "imgui_capture_tool.h"     // ImGuiScreenCaptureFunc

//-------------------------------------------------------------------------
// Forward Declarations
//-------------------------------------------------------------------------

struct ImGuiTest;                   // Data for a test registered with IM_REGISTER_TEST()
struct ImGuiTestContext;            // Context while a test is running
struct ImGuiTestCoroutineInterface; // Interface to expose coroutine functions (imgui_te_coroutine provides a default implementation for C++11 using std::thread, but you may use your own)
struct ImGuiTestEngine;             // Test engine instance
struct ImGuiTestEngineIO;           // Test engine public I/O
struct ImGuiTestEngineResultSummary;// Output of ImGuiTestEngine_GetResultSummary()
struct ImGuiTestItemInfo;           // Info queried from item (id, geometry, status flags, debug label)
struct ImGuiTestItemList;           // A list of items
struct ImGuiTestInputs;             // Simulated user inputs (will be fed into ImGuiIO by the test engine)
struct ImGuiTestRunTask;            // A queued test (test + runflags)

typedef int ImGuiTestFlags;         // Flags: See ImGuiTestFlags_
typedef int ImGuiTestCheckFlags;    // Flags: See ImGuiTestCheckFlags_
typedef int ImGuiTestLogFlags;      // Flags: See ImGuiTestLogFlags_
typedef int ImGuiTestRunFlags;      // Flags: See ImGuiTestRunFlags_

enum ImGuiTestActiveFunc : int;
enum ImGuiTestGroup : int;
enum ImGuiTestRunSpeed : int;
enum ImGuiTestStatus : int;
enum ImGuiTestVerboseLevel : int;
enum ImGuiTestEngineExportFormat : int;

//-------------------------------------------------------------------------
// Types
//-------------------------------------------------------------------------

// Stored in ImGuiTestContext: where we are currently running GuiFunc or TestFunc
enum ImGuiTestActiveFunc : int
{
    ImGuiTestActiveFunc_None,
    ImGuiTestActiveFunc_GuiFunc,
    ImGuiTestActiveFunc_TestFunc
};

enum ImGuiTestRunSpeed : int
{
    ImGuiTestRunSpeed_Fast          = 0,    // Run tests as fast as possible (teleport mouse, skip delays, etc.)
    ImGuiTestRunSpeed_Normal        = 1,    // Run tests at human watchable speed (for debugging)
    ImGuiTestRunSpeed_Cinematic     = 2,    // Run tests with pauses between actions (for e.g. tutorials)
    ImGuiTestRunSpeed_COUNT
};

enum ImGuiTestVerboseLevel : int
{
    ImGuiTestVerboseLevel_Silent    = 0,    // -v0
    ImGuiTestVerboseLevel_Error     = 1,    // -v1
    ImGuiTestVerboseLevel_Warning   = 2,    // -v2
    ImGuiTestVerboseLevel_Info      = 3,    // -v3
    ImGuiTestVerboseLevel_Debug     = 4,    // -v4
    ImGuiTestVerboseLevel_Trace     = 5,
    ImGuiTestVerboseLevel_COUNT
};

// Test status (stored in ImGuiTest)
enum ImGuiTestStatus : int
{
    ImGuiTestStatus_Unknown     = 0,
    ImGuiTestStatus_Success     = 1,
    ImGuiTestStatus_Queued      = 2,
    ImGuiTestStatus_Running     = 3,
    ImGuiTestStatus_Error       = 4,
    ImGuiTestStatus_Suspended   = 5,
    ImGuiTestStatus_COUNT
};

// Test group: this is mostly used to categorize tests in our testing UI. (Stored in ImGuiTest)
enum ImGuiTestGroup : int
{
    ImGuiTestGroup_Unknown      = -1,
    ImGuiTestGroup_Tests        = 0,
    ImGuiTestGroup_Perfs        = 1,
    ImGuiTestGroup_COUNT
};

// Flags (stored in ImGuiTest)
enum ImGuiTestFlags_
{
    ImGuiTestFlags_None                 = 0,
    ImGuiTestFlags_NoGuiWarmUp          = 1 << 0,   // Disable running the GUI func for 2 frames before starting test code. For tests which absolutely need to start before GuiFunc.
    ImGuiTestFlags_NoAutoFinish         = 1 << 1,   // By default, tests with no TestFunc (only a GuiFunc) will end after warmup. Setting this require test to call ctx->Finish().
    ImGuiTestFlags_NoRecoveryWarnings   = 1 << 2    // Error/recovery warnings (missing End/Pop calls etc.) will be displayed as normal debug entries, for tests which may rely on those.
    //ImGuiTestFlags_RequireViewports   = 1 << 10
};

// Flags for IM_CHECK* macros.
enum ImGuiTestCheckFlags_
{
    ImGuiTestCheckFlags_None            = 0,
    ImGuiTestCheckFlags_SilentSuccess   = 1 << 0
};

// Flags for ImGuiTestContext::Log* functions.
enum ImGuiTestLogFlags_
{
    ImGuiTestLogFlags_None              = 0,
    ImGuiTestLogFlags_NoHeader          = 1 << 0    // Do not display frame count and depth padding
};

enum ImGuiTestRunFlags_
{
    ImGuiTestRunFlags_None              = 0,
    ImGuiTestRunFlags_GuiFuncDisable    = 1 << 0,   // Used internally to temporarily disable the GUI func (at the end of a test, etc)
    ImGuiTestRunFlags_GuiFuncOnly       = 1 << 1,   // Set when user selects "Run GUI func"
    ImGuiTestRunFlags_NoSuccessMsg      = 1 << 2,
    ImGuiTestRunFlags_EnableRawInputs   = 1 << 3,   // Disable input submission to let test submission raw input event (in order to test e.g. IO queue)
    ImGuiTestRunFlags_RunFromGui        = 1 << 4,   // Test ran manually from GUI, will disable watchdog.
    ImGuiTestRunFlags_RunFromCommandLine= 1 << 5,   // Test queued from command-line.

    // Flags for ImGuiTestContext::RunChildTest()
    ImGuiTestRunFlags_NoError           = 1 << 10,
    ImGuiTestRunFlags_ShareVars         = 1 << 11,  // Share generic vars and custom vars between child and parent tests (custom vars need to be same type)
    ImGuiTestRunFlags_ShareTestContext  = 1 << 12,  // Share ImGuiTestContext instead of creating a new one (unsure what purpose this may be useful for yet)
    // TODO: Add GuiFunc options
};

struct ImGuiTestEngineResultSummary
{
    int     CountTested = 0;    // Number of tests executed
    int     CountSuccess = 0;   // Number of tests succeeded
    int     CountInQueue = 0;   // Number of tests remaining in queue (e.g. aborted, crashed)
};

//-------------------------------------------------------------------------
// Functions
//-------------------------------------------------------------------------

// Hooks for core imgui/ library (generally called via macros)
extern void         ImGuiTestEngineHook_ItemAdd(ImGuiContext* ui_ctx, ImGuiID id, const ImRect& bb, const ImGuiLastItemData* item_data);
#if IMGUI_VERSION_NUM < 18934
extern void         ImGuiTestEngineHook_ItemAdd(ImGuiContext* ui_ctx, const ImRect& bb, ImGuiID id);
#endif
#ifdef IMGUI_HAS_IMSTR
extern void         ImGuiTestEngineHook_ItemInfo(ImGuiContext* ui_ctx, ImGuiID id, ImStrv label, ImGuiItemStatusFlags flags);
#else
extern void         ImGuiTestEngineHook_ItemInfo(ImGuiContext* ui_ctx, ImGuiID id, const char* label, ImGuiItemStatusFlags flags);
#endif
extern void         ImGuiTestEngineHook_Log(ImGuiContext* ui_ctx, const char* fmt, ...);
extern const char*  ImGuiTestEngine_FindItemDebugLabel(ImGuiContext* ui_ctx, ImGuiID id);

// Functions (generally called via IM_CHECK() macros)
IMGUI_API bool      ImGuiTestEngine_Check(const char* file, const char* func, int line, ImGuiTestCheckFlags flags, bool result, const char* expr);
IMGUI_API bool      ImGuiTestEngine_CheckStrOp(const char* file, const char* func, int line, ImGuiTestCheckFlags flags, const char* op, const char* lhs_var, const char* lhs_value, const char* rhs_var, const char* rhs_value, bool* out_result);
IMGUI_API bool      ImGuiTestEngine_Error(const char* file, const char* func, int line, ImGuiTestCheckFlags flags, const char* fmt, ...);
IMGUI_API void      ImGuiTestEngine_AssertLog(const char* expr, const char* file, const char* function, int line);
IMGUI_API ImGuiTextBuffer* ImGuiTestEngine_GetTempStringBuilder();

//-------------------------------------------------------------------------
// ImGuiTestEngine API
//-------------------------------------------------------------------------

// Functions: Initialization
IMGUI_API ImGuiTestEngine*    ImGuiTestEngine_CreateContext();                                      // Create test engine
IMGUI_API void                ImGuiTestEngine_DestroyContext(ImGuiTestEngine* engine);              // Destroy test engine. Call after ImGui::DestroyContext() so test engine specific ini data gets saved.
IMGUI_API void                ImGuiTestEngine_Start(ImGuiTestEngine* engine, ImGuiContext* ui_ctx); // Bind to a dear imgui context. Start coroutine.
IMGUI_API void                ImGuiTestEngine_Stop(ImGuiTestEngine* engine);                        // Stop coroutine and export if any. (Unbind will lazily happen on context shutdown)
IMGUI_API void                ImGuiTestEngine_PostSwap(ImGuiTestEngine* engine);                    // Call every frame after framebuffer swap, will process screen capture and call test_io.ScreenCaptureFunc()
IMGUI_API ImGuiTestEngineIO&  ImGuiTestEngine_GetIO(ImGuiTestEngine* engine);

// Macros: Register Test
#define IM_REGISTER_TEST(_ENGINE, _CATEGORY, _NAME)  ImGuiTestEngine_RegisterTest(_ENGINE, _CATEGORY, _NAME, __FILE__, __LINE__)
IMGUI_API ImGuiTest*          ImGuiTestEngine_RegisterTest(ImGuiTestEngine* engine, const char* category, const char* name, const char* src_file = nullptr, int src_line = 0); // Prefer calling IM_REGISTER_TEST()
IMGUI_API void                ImGuiTestEngine_UnregisterTest(ImGuiTestEngine* engine, ImGuiTest* test);
IMGUI_API void                ImGuiTestEngine_UnregisterAllTests(ImGuiTestEngine* engine);

// Functions: Main
IMGUI_API void                ImGuiTestEngine_QueueTest(ImGuiTestEngine* engine, ImGuiTest* test, ImGuiTestRunFlags run_flags = 0);
IMGUI_API void                ImGuiTestEngine_QueueTests(ImGuiTestEngine* engine, ImGuiTestGroup group, const char* filter = nullptr, ImGuiTestRunFlags run_flags = 0);
IMGUI_API bool                ImGuiTestEngine_TryAbortEngine(ImGuiTestEngine* engine);
IMGUI_API void                ImGuiTestEngine_AbortCurrentTest(ImGuiTestEngine* engine);
IMGUI_API ImGuiTest*          ImGuiTestEngine_FindTestByName(ImGuiTestEngine* engine, const char* category, const char* name);

// Functions: Status Queries
// FIXME: Clarify API to avoid function calls vs raw bools in ImGuiTestEngineIO
IMGUI_API bool                ImGuiTestEngine_IsTestQueueEmpty(ImGuiTestEngine* engine);
IMGUI_API bool                ImGuiTestEngine_IsUsingSimulatedInputs(ImGuiTestEngine* engine);
IMGUI_API void                ImGuiTestEngine_GetResultSummary(ImGuiTestEngine* engine, ImGuiTestEngineResultSummary* out_results);
IMGUI_API void                ImGuiTestEngine_GetTestList(ImGuiTestEngine* engine, ImVector<ImGuiTest*>* out_tests);
IMGUI_API void                ImGuiTestEngine_GetTestQueue(ImGuiTestEngine* engine, ImVector<ImGuiTestRunTask>* out_tests);

#ifndef IMGUI_DISABLE_OBSOLETE_FUNCTIONS
// Obsoleted 2025/03/17
static inline void            ImGuiTestEngine_GetResult(ImGuiTestEngine* engine, int& out_count_tested, int& out_count_success) { ImGuiTestEngineResultSummary summary; ImGuiTestEngine_GetResultSummary(engine, &summary); out_count_tested = summary.CountTested; out_count_success = summary.CountSuccess; }
#endif

// Functions: Crash Handling
// Ensure past test results are properly exported even if application crash during a test.
IMGUI_API void                ImGuiTestEngine_InstallDefaultCrashHandler();     // Install default crash handler (if you don't have one)
IMGUI_API void                ImGuiTestEngine_CrashHandler();                   // Default crash handler, should be called from a custom crash handler if such exists

//-----------------------------------------------------------------------------
// IO structure to configure the test engine
//-----------------------------------------------------------------------------

// Function bound to right-clicking on a test and selecting "Open source" in the UI
// - Easy: you can make this function call OS shell to "open" the file (e.g. ImOsOpenInShell() helper).
// - Better: bind this function to a custom setup which can pass line number to a text editor (e.g. see 'imgui_test_suite/tools/win32_open_with_sublime.cmd' example)
typedef void (ImGuiTestEngineSrcFileOpenFunc)(const char* filename, int line_no, void* user_data);

struct IMGUI_API ImGuiTestEngineIO
{
    //-------------------------------------------------------------------------
    // Functions
    //-------------------------------------------------------------------------

    // Options: Functions
    ImGuiTestCoroutineInterface*                CoroutineFuncs = nullptr;          // (Required) Coroutine functions (see imgui_te_coroutines.h)
    ImFuncPtr(ImGuiTestEngineSrcFileOpenFunc)   SrcFileOpenFunc = nullptr;         // (Optional) To open source files from test engine UI (otherwise default to open file in shell)
    ImFuncPtr(ImGuiScreenCaptureFunc)           ScreenCaptureFunc = nullptr;       // (Optional) To capture graphics output (application _MUST_ call ImGuiTestEngine_PostSwap() function after swapping is framebuffer)
    void*                                       SrcFileOpenUserData = nullptr;     // (Optional) User data for SrcFileOpenFunc
    void*                                       ScreenCaptureUserData = nullptr;   // (Optional) User data for ScreenCaptureFunc

    // Options: Main
    bool                        ConfigSavedSettings = true;                     // Load/Save settings in main context .ini file.
    ImGuiTestRunSpeed           ConfigRunSpeed = ImGuiTestRunSpeed_Fast;        // Run tests in fast/normal/cinematic mode
    bool                        ConfigStopOnError = false;                      // Stop queued tests on test error
    bool                        ConfigBreakOnError = false;                     // Break debugger on test error by calling IM_DEBUG_BREAK()
    bool                        ConfigKeepGuiFunc = false;                      // Keep test GUI running at the end of the test
    ImGuiTestVerboseLevel       ConfigVerboseLevel = ImGuiTestVerboseLevel_Warning;
    ImGuiTestVerboseLevel       ConfigVerboseLevelOnError = ImGuiTestVerboseLevel_Info;
    bool                        ConfigLogToTTY = false;
    bool                        ConfigLogToDebugger = false;
    bool                        ConfigRestoreFocusAfterTests = true;// Restore focus back after running tests
    bool                        ConfigCaptureEnabled = true;        // Master enable flags for capturing and saving captures. Disable to avoid e.g. lengthy saving of large PNG files.
    bool                        ConfigCaptureOnError = false;
    bool                        ConfigNoThrottle = false;           // Disable vsync for performance measurement or fast test running
    bool                        ConfigMouseDrawCursor = true;       // Enable drawing of Dear ImGui software mouse cursor when running tests
    float                       ConfigFixedDeltaTime = 0.0f;        // Use fixed delta time instead of calculating it from wall clock
    int                         PerfStressAmount = 1;               // Integer to scale the amount of items submitted in test
    char                        GitBranchName[64] = "";             // e.g. fill in branch name (e.g. recorded in perf samples .csv)

    // Options: Speed of user simulation
    float                       MouseSpeed = 600.0f;                // Mouse speed (pixel/second) when not running in fast mode
    float                       MouseWobble = 0.25f;                // (0.0f..1.0f) How much wobble to apply to the mouse (pixels per pixel of move distance) when not running in fast mode
    float                       ScrollSpeed = 1400.0f;              // Scroll speed (pixel/second) when not running in fast mode
    float                       TypingSpeed = 20.0f;                // Char input speed (characters/second) when not running in fast mode
    float                       ActionDelayShort = 0.15f;           // Time between short actions
    float                       ActionDelayStandard = 0.40f;        // Time between most actions

    // Options: Screen/video capture
    char                        VideoCaptureEncoderPath[256] = "";  // Video encoder executable path, e.g. "path/to/ffmpeg.exe".
    char                        VideoCaptureEncoderParams[256] = "";// Video encoder parameters for .MP4 captures, e.g. see IMGUI_CAPTURE_DEFAULT_VIDEO_PARAMS_FOR_FFMPEG
    char                        GifCaptureEncoderParams[512] = "";  // Video encoder parameters for .GIF captures, e.g. see IMGUI_CAPTURE_DEFAULT_GIF_PARAMS_FOR_FFMPEG
    char                        VideoCaptureExtension[8] = ".mp4";  // Video file extension (default, may be overridden by test).

    // Options: Watchdog. Set values to FLT_MAX to disable.
    // Interactive GUI applications that may be slower tend to use higher values.
    float                       ConfigWatchdogWarning = 30.0f;      // Warn when a test exceed this time (in second)
    float                       ConfigWatchdogKillTest = 60.0f;     // Attempt to stop running a test when exceeding this time (in second)
    float                       ConfigWatchdogKillApp = FLT_MAX;    // Stop application when exceeding this time (in second)

    // Options: Export
    // While you can manually call ImGuiTestEngine_Export(), registering filename/format here ensure the crash handler will always export if application crash.
    const char*                 ExportResultsFilename = nullptr;
    ImGuiTestEngineExportFormat ExportResultsFormat = (ImGuiTestEngineExportFormat)0;

    // Options: Sanity Checks
    bool                        CheckDrawDataIntegrity = false;     // Check ImDrawData integrity (buffer count, etc.). Currently cheap but may become a slow operation.

    //-------------------------------------------------------------------------
    // Output
    //-------------------------------------------------------------------------

    // Output: State of test engine
    bool                        IsRunningTests = false;
    bool                        IsRequestingMaxAppSpeed = false;    // When running in fast mode: request app to skip vsync or even skip rendering if it wants
    bool                        IsCapturing = false;                // Capture is in progress
};

//-------------------------------------------------------------------------
// ImGuiTestItemInfo
//-------------------------------------------------------------------------

// Information about a given item or window, result of an ItemInfo() or WindowInfo() query
struct ImGuiTestItemInfo
{
    ImGuiID                     ID = 0;                     // Item ID
    char                        DebugLabel[32] = {};        // Shortened/truncated label for debugging and convenience purpose
    ImGuiWindow*                Window = nullptr;           // Item Window
    unsigned int                NavLayer : 1;               // Nav layer of the item (ImGuiNavLayer)
    int                         Depth : 16;                 // Depth from requested parent id. 0 == ID is immediate child of requested parent id.
    int                         TimestampMain;              // Timestamp of main result (all fields)
    int                         TimestampStatus;            // Timestamp of StatusFlags
    ImGuiID                     ParentID = 0;               // Item Parent ID (value at top of the ID stack)
    ImRect                      RectFull = ImRect();        // Item Rectangle
    ImRect                      RectClipped = ImRect();     // Item Rectangle (clipped with window->ClipRect at time of item submission)
    ImGuiItemFlags              ItemFlags = 0;              // Item flags
    //ImGuiItemFlags            InFlags = 0;                // Item flags (OBSOLETE: before 2024/10/17 ItemFlags was called InFlags)
    ImGuiItemStatusFlags        StatusFlags = 0;            // Item Status flags (fully updated for some items only, compare TimestampStatus to FrameCount)

    ImGuiTestItemInfo()         { memset(this, 0, sizeof(*this)); }
};

// Result of an GatherItems() query
struct IMGUI_API ImGuiTestItemList
{
    ImPool<ImGuiTestItemInfo>   Pool;

    void                        Clear()                 { Pool.Clear(); }
    void                        Reserve(int capacity)   { Pool.Reserve(capacity); }
    int                         GetSize() const         { return Pool.GetMapSize(); }
    const ImGuiTestItemInfo*    GetByIndex(int n)       { return Pool.GetByIndex(n); }
    const ImGuiTestItemInfo*    GetByID(ImGuiID id)     { return Pool.GetByKey(id); }

    // For range-for
    size_t                      size() const            { return (size_t)Pool.GetMapSize(); }
    const ImGuiTestItemInfo*    begin() const           { return Pool.Buf.begin(); }
    const ImGuiTestItemInfo*    end() const             { return Pool.Buf.end(); }
    const ImGuiTestItemInfo*    operator[] (size_t n)   { return &Pool.Buf[(int)n]; }
};

//-------------------------------------------------------------------------
// ImGuiTestLog: store textual output of one given Test.
//-------------------------------------------------------------------------

struct IMGUI_API ImGuiTestLogLineInfo
{
    ImGuiTestVerboseLevel           Level;
    int                             LineOffset;
};

struct IMGUI_API ImGuiTestLog
{
    ImGuiTextBuffer                 Buffer;
    ImVector<ImGuiTestLogLineInfo>  LineInfo;
    int                             CountPerLevel[ImGuiTestVerboseLevel_COUNT] = {};

    // Functions
    ImGuiTestLog() {}
    bool    IsEmpty() const         { return Buffer.empty(); }
    void    Clear();

    // Extract log contents filtered per log-level.
    // Output:
    // - If 'buffer != nullptr': all extracted lines are appended to 'buffer'. Use 'buffer->c_str()' on your side to obtain the text.
    // - Return value: number of lines extracted (should be equivalent to number of '\n' inside buffer->c_str()).
    // - You may call the function with buffer == nullptr to only obtain a count without getting the data.
    // Verbose levels are inclusive:
    // - To get ONLY Error:                     Use level_min == ImGuiTestVerboseLevel_Error, level_max = ImGuiTestVerboseLevel_Error
    // - To get ONLY Error and Warnings:        Use level_min == ImGuiTestVerboseLevel_Error, level_max = ImGuiTestVerboseLevel_Warning
    // - To get All Errors, Warnings, Debug...  Use level_min == ImGuiTestVerboseLevel_Error, level_max = ImGuiTestVerboseLevel_Trace
    int     ExtractLinesForVerboseLevels(ImGuiTestVerboseLevel level_min, ImGuiTestVerboseLevel level_max, ImGuiTextBuffer* out_buffer);

    // [Internal]
    void    UpdateLineOffsets(ImGuiTestEngineIO* engine_io, ImGuiTestVerboseLevel level, const char* start);
};

//-------------------------------------------------------------------------
// ImGuiTest
//-------------------------------------------------------------------------

typedef void    (ImGuiTestGuiFunc)(ImGuiTestContext* ctx);
typedef void    (ImGuiTestTestFunc)(ImGuiTestContext* ctx);

// Wraps a placement new of a given type (where 'buffer' is the allocated memory)
typedef void    (ImGuiTestVarsConstructor)(void* buffer);
typedef void    (ImGuiTestVarsPostConstructor)(ImGuiTestContext* ctx, void* ptr, void* fn);
typedef void    (ImGuiTestVarsDestructor)(void* ptr);

// Storage for the output of a test run
struct IMGUI_API ImGuiTestOutput
{
    ImGuiTestStatus                 Status = ImGuiTestStatus_Unknown;
    ImGuiTestLog                    Log;
    ImU64                           StartTime = 0;
    ImU64                           EndTime = 0;
};

// Storage for one test
struct IMGUI_API ImGuiTest
{
    // Test Definition
    const char*                     Category = nullptr;             // Literal, not owned
    const char*                     Name = nullptr;                 // Literal, generally not owned unless NameOwned=true
    ImGuiTestGroup                  Group = ImGuiTestGroup_Unknown; // Coarse groups: 'Tests' or 'Perf'
    bool                            NameOwned = false;              //
    int                             ArgVariant = 0;                 // User parameter. Generally we use it to run variations of a same test by sharing GuiFunc/TestFunc
    ImGuiTestFlags                  Flags = ImGuiTestFlags_None;    // See ImGuiTestFlags_
    ImFuncPtr(ImGuiTestGuiFunc)     GuiFunc = nullptr;              // GUI function (optional if your test are running over an existing GUI application)
    ImFuncPtr(ImGuiTestTestFunc)    TestFunc = nullptr;             // Test function
    void*                           UserData = nullptr;             // General purpose user data (if assigning capturing lambdas on GuiFunc/TestFunc you may not need to use this)
    //ImVector<ImGuiTestRunTask>    Dependencies;                   // Registered via AddDependencyTest(), ran automatically before our test. This is a simpler wrapper to calling ctx->RunChildTest()

    // Sources information (exposed in UI)
    const char*                     SourceFile = nullptr;           // __FILE__
    int                             SourceLine = 0;                 // __LINE__
    int                             SourceLineEnd = 0;              // end of line (when calculated by ImGuiTestEngine_StartCalcSourceLineEnds())

    // Last Test Output/Status
    // (this is the only part that may change after registration)
    ImGuiTestOutput                 Output;

    // User variables (which are instantiated when running the test)
    // Setup after test registration with SetVarsDataType<>(), access instance during test with GetVars<>().
    // This is mostly useful to communicate between GuiFunc and TestFunc. If you don't use both you may not want to use it!
    size_t                          VarsSize = 0;
    ImGuiTestVarsConstructor*       VarsConstructor = nullptr;
    ImGuiTestVarsPostConstructor*   VarsPostConstructor = nullptr;  // To override constructor default (in case the default are problematic on the first GuiFunc frame)
    void*                           VarsPostConstructorUserFn = nullptr;
    ImGuiTestVarsDestructor*        VarsDestructor = nullptr;

    // Functions
    ImGuiTest() {}
    ~ImGuiTest();

    void SetOwnedName(const char* name);

    template <typename T>
    void SetVarsDataType(void(*post_initialize)(ImGuiTestContext* ctx, T& vars) = nullptr)
    {
        VarsSize = sizeof(T);
        VarsConstructor = [](void* ptr) { IM_PLACEMENT_NEW(ptr) T; };
        VarsDestructor = [](void* ptr) { IM_UNUSED(ptr); reinterpret_cast<T*>(ptr)->~T(); };
        if (post_initialize != nullptr)
        {
            VarsPostConstructorUserFn = (void*)post_initialize;
            VarsPostConstructor = [](ImGuiTestContext* ctx, void* ptr, void* fn) { ((void (*)(ImGuiTestContext*, T&))(fn))(ctx, *(T*)ptr); };
        }
    }
};

// Stored in test queue
struct IMGUI_API ImGuiTestRunTask
{
    ImGuiTest*          Test = nullptr;
    ImGuiTestRunFlags   RunFlags = ImGuiTestRunFlags_None;
};

//-------------------------------------------------------------------------

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
