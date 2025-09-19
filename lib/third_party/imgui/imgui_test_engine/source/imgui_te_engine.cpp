// dear imgui test engine
// (core)
// This is the interface that your initial setup (app init, main loop) will mostly be using.
// Actual tests will mostly use the interface of imgui_te_context.h

// This file is governed by the "Dear ImGui Test Engine License".
// Details of the license are provided in the LICENSE.txt file in the same directory.

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_te_engine.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_te_utils.h"
#include "imgui_te_context.h"
#include "imgui_te_internal.h"
#include "imgui_te_perftool.h"
#include "imgui_te_exporters.h"
#include "thirdparty/Str/Str.h"
#if _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>    // SetUnhandledExceptionFilter()
#undef Yield            // Undo some of the damage done by <windows.h>
#else
#if !IMGUI_TEST_ENGINE_IS_GAME_CONSOLE
#include <signal.h>     // signal()
#endif
#include <unistd.h>     // sleep()
#endif

// Warnings
#ifdef _MSC_VER
#pragma warning (disable: 4127) // conditional expression is constant
#endif

/*

Index of this file:

// [SECTION] TODO
// [SECTION] FORWARD DECLARATIONS
// [SECTION] DATA STRUCTURES
// [SECTION] TEST ENGINE FUNCTIONS
// [SECTION] CRASH HANDLING
// [SECTION] HOOKS FOR CORE LIBRARY
// [SECTION] CHECK/ERROR FUNCTIONS FOR TESTS
// [SECTION] SETTINGS
// [SECTION] ImGuiTestLog
// [SECTION] ImGuiTest

*/

//-------------------------------------------------------------------------
// [SECTION] DATA
//-------------------------------------------------------------------------

static ImGuiTestEngine* GImGuiTestEngine = nullptr;

//-------------------------------------------------------------------------
// [SECTION] FORWARD DECLARATIONS
//-------------------------------------------------------------------------

// Private functions
static void ImGuiTestEngine_CoroutineStopAndJoin(ImGuiTestEngine* engine);
static void ImGuiTestEngine_ClearInput(ImGuiTestEngine* engine);
static void ImGuiTestEngine_ApplyInputToImGuiContext(ImGuiTestEngine* engine);
static void ImGuiTestEngine_ProcessTestQueue(ImGuiTestEngine* engine);
static void ImGuiTestEngine_ClearTests(ImGuiTestEngine* engine);
static void ImGuiTestEngine_PreNewFrame(ImGuiTestEngine* engine, ImGuiContext* ui_ctx);
static void ImGuiTestEngine_PostNewFrame(ImGuiTestEngine* engine, ImGuiContext* ui_ctx);
static void ImGuiTestEngine_PreEndFrame(ImGuiTestEngine* engine, ImGuiContext* ui_ctx);
static void ImGuiTestEngine_PreRender(ImGuiTestEngine* engine, ImGuiContext* ui_ctx);
static void ImGuiTestEngine_PostRender(ImGuiTestEngine* engine, ImGuiContext* ui_ctx);
static void ImGuiTestEngine_UpdateHooks(ImGuiTestEngine* engine);
static void ImGuiTestEngine_RunGuiFunc(ImGuiTestEngine* engine);
static void ImGuiTestEngine_RunTestFunc(ImGuiTestEngine* engine);
static void ImGuiTestEngine_ErrorRecoverySetup(ImGuiTestEngine* engine);
static void ImGuiTestEngine_ErrorRecoveryRun(ImGuiTestEngine* engine);
static void ImGuiTestEngine_TestQueueCoroutineMain(void* engine_opaque);

// Settings
static void* ImGuiTestEngine_SettingsReadOpen(ImGuiContext*, ImGuiSettingsHandler*, const char* name);
static void  ImGuiTestEngine_SettingsReadLine(ImGuiContext*, ImGuiSettingsHandler*, void* entry, const char* line);
static void  ImGuiTestEngine_SettingsWriteAll(ImGuiContext* imgui_ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buf);

//-------------------------------------------------------------------------
// [SECTION] TEST ENGINE FUNCTIONS
//-------------------------------------------------------------------------
// Public
// - ImGuiTestEngine_CreateContext()
// - ImGuiTestEngine_DestroyContext()
// - ImGuiTestEngine_BindImGuiContext()
// - ImGuiTestEngine_UnbindImGuiContext()
// - ImGuiTestEngine_GetIO()
// - ImGuiTestEngine_Abort()
// - ImGuiTestEngine_QueueAllTests()
//-------------------------------------------------------------------------
// - ImGuiTestEngine_FindItemInfo()
// - ImGuiTestEngine_ClearTests()
// - ImGuiTestEngine_ApplyInputToImGuiContext()
// - ImGuiTestEngine_PreNewFrame()
// - ImGuiTestEngine_PostNewFrame()
// - ImGuiTestEngine_Yield()
// - ImGuiTestEngine_ProcessTestQueue()
// - ImGuiTestEngine_QueueTest()
// - ImGuiTestEngine_RunTest()
//-------------------------------------------------------------------------

ImGuiTestEngine::ImGuiTestEngine()
{
    PerfRefDeltaTime = 0.0f;
    PerfDeltaTime100.Init(100);
    PerfDeltaTime500.Init(500);
    PerfTool = IM_NEW(ImGuiPerfTool);
    UiFilterTests = IM_NEW(Str256); // We bite the bullet of adding an extra alloc/indirection in order to avoid including Str.h in our header
    UiFilterPerfs = IM_NEW(Str256);

    // Initialize std::thread based coroutine implementation if requested
#if IMGUI_TEST_ENGINE_ENABLE_COROUTINE_STDTHREAD_IMPL
    IM_ASSERT(IO.CoroutineFuncs == nullptr && "IO.CoroutineFuncs already setup elsewhere!");
    IO.CoroutineFuncs = Coroutine_ImplStdThread_GetInterface();
#endif
}

ImGuiTestEngine::~ImGuiTestEngine()
{
    IM_ASSERT(TestQueueCoroutine == nullptr);
    IM_DELETE(PerfTool);
    IM_DELETE(UiFilterTests);
    IM_DELETE(UiFilterPerfs);
}

// Using named functions here instead of lambda gives nicer call-stacks (mostly because we frequently step in PostNewFrame)
static void ImGuiTestEngine_ShutdownHook(ImGuiContext* ui_ctx, ImGuiContextHook* hook)      { ImGuiTestEngine_UnbindImGuiContext((ImGuiTestEngine*)hook->UserData, ui_ctx); }
static void ImGuiTestEngine_PreNewFrameHook(ImGuiContext* ui_ctx, ImGuiContextHook* hook)   { ImGuiTestEngine_PreNewFrame((ImGuiTestEngine*)hook->UserData, ui_ctx); }
static void ImGuiTestEngine_PostNewFrameHook(ImGuiContext* ui_ctx, ImGuiContextHook* hook)  { ImGuiTestEngine_PostNewFrame((ImGuiTestEngine*)hook->UserData, ui_ctx); }
static void ImGuiTestEngine_PreEndFrameHook(ImGuiContext* ui_ctx, ImGuiContextHook* hook)   { ImGuiTestEngine_PreEndFrame((ImGuiTestEngine*)hook->UserData, ui_ctx); }
static void ImGuiTestEngine_PreRenderHook(ImGuiContext* ui_ctx, ImGuiContextHook* hook)     { ImGuiTestEngine_PreRender((ImGuiTestEngine*)hook->UserData, ui_ctx); }
static void ImGuiTestEngine_PostRenderHook(ImGuiContext* ui_ctx, ImGuiContextHook* hook)    { ImGuiTestEngine_PostRender((ImGuiTestEngine*)hook->UserData, ui_ctx); }

void ImGuiTestEngine_BindImGuiContext(ImGuiTestEngine* engine, ImGuiContext* ui_ctx)
{
    IM_ASSERT(engine->UiContextTarget == ui_ctx);

    // Add .ini handle for ImGuiWindow type
    if (engine->IO.ConfigSavedSettings)
    {
        ImGuiSettingsHandler ini_handler;
        ini_handler.TypeName = "TestEngine";
        ini_handler.TypeHash = ImHashStr("TestEngine");
        ini_handler.ReadOpenFn = ImGuiTestEngine_SettingsReadOpen;
        ini_handler.ReadLineFn = ImGuiTestEngine_SettingsReadLine;
        ini_handler.WriteAllFn = ImGuiTestEngine_SettingsWriteAll;
        ui_ctx->SettingsHandlers.push_back(ini_handler);
        engine->PerfTool->_AddSettingsHandler();
    }

    // Install generic context hooks facility
    ImGuiContextHook hook;
    hook.Type = ImGuiContextHookType_Shutdown;
    hook.Callback = ImGuiTestEngine_ShutdownHook;
    hook.UserData = (void*)engine;
    ImGui::AddContextHook(ui_ctx, &hook);

    hook.Type = ImGuiContextHookType_NewFramePre;
    hook.Callback = ImGuiTestEngine_PreNewFrameHook;
    hook.UserData = (void*)engine;
    ImGui::AddContextHook(ui_ctx, &hook);

    hook.Type = ImGuiContextHookType_NewFramePost;
    hook.Callback = ImGuiTestEngine_PostNewFrameHook;
    hook.UserData = (void*)engine;
    ImGui::AddContextHook(ui_ctx, &hook);

    hook.Type = ImGuiContextHookType_EndFramePre;
    hook.Callback = ImGuiTestEngine_PreEndFrameHook;
    hook.UserData = (void*)engine;
    ImGui::AddContextHook(ui_ctx, &hook);

    hook.Type = ImGuiContextHookType_RenderPre;
    hook.Callback = ImGuiTestEngine_PreRenderHook;
    hook.UserData = (void*)engine;
    ImGui::AddContextHook(ui_ctx, &hook);

    hook.Type = ImGuiContextHookType_RenderPost;
    hook.Callback = ImGuiTestEngine_PostRenderHook;
    hook.UserData = (void*)engine;
    ImGui::AddContextHook(ui_ctx, &hook);

    // Install custom test engine hook data
    if (GImGuiTestEngine == nullptr)
        GImGuiTestEngine = engine;
    IM_ASSERT(ui_ctx->TestEngine == nullptr);
    ui_ctx->TestEngine = engine;
    engine->UiContextHasHooks = false;
}

void    ImGuiTestEngine_UnbindImGuiContext(ImGuiTestEngine* engine, ImGuiContext* ui_ctx)
{
    IM_ASSERT(engine->UiContextTarget == ui_ctx);

    // FIXME: Could use ImGui::RemoveContextHook() if we stored our hook ids
    for (int hook_n = 0; hook_n < ui_ctx->Hooks.Size; hook_n++)
        if (ui_ctx->Hooks[hook_n].UserData == engine)
            ImGui::RemoveContextHook(ui_ctx, ui_ctx->Hooks[hook_n].HookId);

    ImGuiTestEngine_CoroutineStopAndJoin(engine);

    IM_ASSERT(ui_ctx->TestEngine == engine);
    ui_ctx->TestEngine = nullptr;

    // Remove .ini handler
    IM_ASSERT(GImGui == ui_ctx);
    if (engine->IO.ConfigSavedSettings)
    {
        ImGui::RemoveSettingsHandler("TestEngine");
        ImGui::RemoveSettingsHandler("TestEnginePerfTool");
    }

    // Remove hook
    if (GImGuiTestEngine == engine)
        GImGuiTestEngine = nullptr;
    engine->UiContextTarget = engine->UiContextActive = nullptr;
}

// Create test context (not bound to any dear imgui context yet)
ImGuiTestEngine*    ImGuiTestEngine_CreateContext()
{
    IMGUI_CHECKVERSION(); // <--- If you get a crash here: mismatching config, check that both imgui and imgui_test_engine are using same defines (e.g. using the same imconfig file)
    ImGuiTestEngine* engine = IM_NEW(ImGuiTestEngine)();
    return engine;
}

void    ImGuiTestEngine_DestroyContext(ImGuiTestEngine* engine)
{
    // We require user to call DestroyContext() before ImGuiTestEngine_DestroyContext() in order to preserve ini data...
    // In case of e.g. dynamically creating a TestEngine as runtime and not caring about its settings, you may set io.ConfigSavedSettings to false
    // in order to allow earlier destruction of the context.
    if (engine->IO.ConfigSavedSettings)
        IM_ASSERT(engine->UiContextTarget == nullptr && "You need to call ImGui::DestroyContext() BEFORE ImGuiTestEngine_DestroyContext()");

    // Shutdown coroutine
    ImGuiTestEngine_CoroutineStopAndJoin(engine);
    if (engine->UiContextTarget != nullptr)
        ImGuiTestEngine_UnbindImGuiContext(engine, engine->UiContextTarget);

    ImGuiTestEngine_ClearTests(engine);

    for (int n = 0; n < engine->InfoTasks.Size; n++)
        IM_DELETE(engine->InfoTasks[n]);
    engine->InfoTasks.clear();

    IM_DELETE(engine);

    // Release hook
    if (GImGuiTestEngine == engine)
        GImGuiTestEngine = nullptr;
}

void    ImGuiTestEngine_Start(ImGuiTestEngine* engine, ImGuiContext* ui_ctx)
{
    IM_ASSERT(engine->Started == false);
    IM_ASSERT(engine->UiContextTarget == nullptr);

    engine->UiContextTarget = ui_ctx;
    ImGuiTestEngine_BindImGuiContext(engine, engine->UiContextTarget);

    // Create our coroutine
    // (we include the word "Main" in the name to facilitate filtering for both this thread and the "Main Thread" in debuggers)
    if (!engine->TestQueueCoroutine)
    {
        IM_ASSERT(engine->IO.CoroutineFuncs && "Missing CoroutineFuncs! Use '#define IMGUI_TEST_ENGINE_ENABLE_COROUTINE_STDTHREAD_IMPL 1' or define your own implementation!");
        engine->TestQueueCoroutine = engine->IO.CoroutineFuncs->CreateFunc(ImGuiTestEngine_TestQueueCoroutineMain, "Main Dear ImGui Test Thread", engine);
    }
    engine->TestQueueCoroutineShouldExit = false;
    engine->Started = true;
}

void    ImGuiTestEngine_Stop(ImGuiTestEngine* engine)
{
    IM_ASSERT(engine->Started);
    IM_ASSERT(engine->UiContextTarget != NULL);

    engine->Abort = true;
    ImGuiTestEngine_CoroutineStopAndJoin(engine);
    //ImGuiTestEngine_UnbindImGuiContext(engine, engine->UiContextTarget);
    ImGuiTestEngine_Export(engine);
    engine->Started = false;
}

static void    ImGuiTestEngine_CoroutineStopRequest(ImGuiTestEngine* engine)
{
    if (engine->TestQueueCoroutine != nullptr)
        engine->TestQueueCoroutineShouldExit = true;
}

static void    ImGuiTestEngine_CoroutineStopAndJoin(ImGuiTestEngine* engine)
{
    if (engine->TestQueueCoroutine != nullptr)
    {
        // Run until the coroutine exits
        engine->TestQueueCoroutineShouldExit = true;
        while (true)
        {
            if (!engine->IO.CoroutineFuncs->RunFunc(engine->TestQueueCoroutine))
                break;
        }
        engine->IO.CoroutineFuncs->DestroyFunc(engine->TestQueueCoroutine);
        engine->TestQueueCoroutine = nullptr;
    }
}

// [EXPERIMENTAL] Destroy and recreate ImGui context
// This potentially allow us to test issues related to handling new windows, restoring settings etc.
// This also gets us once inch closer to more dynamic management of context (e.g. jail tests in their own context)
// FIXME: This is currently called by ImGuiTestEngine_PreNewFrame() in hook but may end up needing to be called
// by main application loop in order to facilitate letting app know of the new pointers. For now none of our backends
// preserve the pointer so may be fine.
void    ImGuiTestEngine_RebootUiContext(ImGuiTestEngine* engine)
{
    IM_ASSERT(engine->Started);
    ImGuiContext* ctx = engine->UiContextTarget;
    ImGuiTestEngine_Stop(engine);
    ImGuiTestEngine_UnbindImGuiContext(engine, ctx);

    // Backup
#ifdef IMGUI_HAS_TEXTURES
    ImGuiContext* backup_atlas_owner = ctx->IO.Fonts->OwnerContext;
#else
    bool backup_atlas_owned_by_context = ctx->FontAtlasOwnedByContext;
#endif
    ImFontAtlas* backup_atlas = ctx->IO.Fonts;
    ImGuiIO backup_io = ctx->IO;
#ifdef IMGUI_HAS_VIEWPORT
    // FIXME: Break with multi-viewports as we don't preserve user windowing data properly.
    // Backend tend to store e.g. HWND data in viewport 0.
    if (ctx->IO.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        IM_ASSERT(0);
    //ImGuiViewport backup_viewport0 = *(ImGuiViewport*)ctx->Viewports[0];
    //ImGuiPlatformIO backup_platform_io = ctx->PlatformIO;
    //ImGui::DestroyPlatformWindows();
#endif

    // Recreate
#ifdef IMGUI_HAS_TEXTURES
    ctx->IO.Fonts->OwnerContext = backup_atlas_owner;
#else
    ctx->FontAtlasOwnedByContext = false;
#endif
#if 1
    ImGui::DestroyContext();
    ImGui::CreateContext(backup_atlas);
#else
    // Preserve same context pointer, which is probably misleading and not even necessary.
    ImGui::Shutdown(ctx);
    ctx->~ImGuiContext();
    IM_PLACEMENT_NEW(ctx) ImGuiContext(backup_atlas);
    ImGui::Initialize(ctx);
#endif

    // Restore
#ifdef IMGUI_HAS_TEXTURES
    ctx->IO.Fonts->OwnerContext = ctx;
#else
    ctx->FontAtlasOwnedByContext = backup_atlas_owned_by_context;
#endif
    ctx->IO = backup_io;
#ifdef IMGUI_HAS_VIEWPORT
    //backup_platform_io.Viewports.swap(ctx->PlatformIO.Viewports);
    //ctx->PlatformIO = backup_platform_io;
    //ctx->Viewports[0]->RendererUserData = backup_viewport0.RendererUserData;
    //ctx->Viewports[0]->PlatformUserData = backup_viewport0.PlatformUserData;
    //ctx->Viewports[0]->PlatformHandle = backup_viewport0.PlatformHandle;
    //ctx->Viewports[0]->PlatformHandleRaw = backup_viewport0.PlatformHandleRaw;
    //memset(&backup_viewport0, 0, sizeof(backup_viewport0));
#endif

    ImGuiTestEngine_Start(engine, ctx);
}

void    ImGuiTestEngine_PostSwap(ImGuiTestEngine* engine)
{
    engine->PostSwapCalled = true;

    if (engine->IO.ConfigFixedDeltaTime != 0.0f)
        ImGuiTestEngine_SetDeltaTime(engine, engine->IO.ConfigFixedDeltaTime);

    // Sync capture tool configurations from engine IO.
    engine->CaptureContext.ScreenCaptureFunc = engine->IO.ScreenCaptureFunc;
    engine->CaptureContext.ScreenCaptureUserData = engine->IO.ScreenCaptureUserData;
    engine->CaptureContext.VideoCaptureEncoderPath = engine->IO.VideoCaptureEncoderPath;
    engine->CaptureContext.VideoCaptureEncoderPathSize = IM_ARRAYSIZE(engine->IO.VideoCaptureEncoderPath);
    engine->CaptureContext.VideoCaptureEncoderParams = engine->IO.VideoCaptureEncoderParams;
    engine->CaptureContext.VideoCaptureEncoderParamsSize = IM_ARRAYSIZE(engine->IO.VideoCaptureEncoderParams);
    engine->CaptureContext.GifCaptureEncoderParams = engine->IO.GifCaptureEncoderParams;
    engine->CaptureContext.GifCaptureEncoderParamsSize = IM_ARRAYSIZE(engine->IO.GifCaptureEncoderParams);
    engine->CaptureTool.VideoCaptureExtension = engine->IO.VideoCaptureExtension;
    engine->CaptureTool.VideoCaptureExtensionSize = IM_ARRAYSIZE(engine->IO.VideoCaptureExtension);

    // Capture a screenshot from main thread while coroutine waits
    if (engine->CaptureCurrentArgs != nullptr)
    {
        ImGuiCaptureStatus status = engine->CaptureContext.CaptureUpdate(engine->CaptureCurrentArgs);
        if (status != ImGuiCaptureStatus_InProgress)
        {
            if (status == ImGuiCaptureStatus_Done)
                ImStrncpy(engine->CaptureTool.OutputLastFilename, engine->CaptureCurrentArgs->InOutputFile, IM_ARRAYSIZE(engine->CaptureTool.OutputLastFilename));
            engine->CaptureCurrentArgs = nullptr;
        }
    }
}

ImGuiTestEngineIO&  ImGuiTestEngine_GetIO(ImGuiTestEngine* engine)
{
    return engine->IO;
}

void    ImGuiTestEngine_AbortCurrentTest(ImGuiTestEngine* engine)
{
    engine->Abort = true;
    if (ImGuiTestContext* test_context = engine->TestContext)
        test_context->Abort = true;
}

bool    ImGuiTestEngine_TryAbortEngine(ImGuiTestEngine* engine)
{
    ImGuiTestEngine_AbortCurrentTest(engine);
    ImGuiTestEngine_CoroutineStopRequest(engine);
    if (ImGuiTestEngine_IsTestQueueEmpty(engine))
        return true;
    return false; // Still running coroutine
}

// FIXME-OPT
ImGuiTest* ImGuiTestEngine_FindTestByName(ImGuiTestEngine* engine, const char* category, const char* name)
{
    IM_ASSERT(category != nullptr || name != nullptr);
    for (int n = 0; n < engine->TestsAll.Size; n++)
    {
        ImGuiTest* test = engine->TestsAll[n];
        if (name != nullptr && strcmp(test->Name, name) != 0)
            continue;
        if (category != nullptr && strcmp(test->Category, category) != 0)
            continue;
        return test;
    }
    return nullptr;
}

// FIXME-OPT
static ImGuiTestInfoTask* ImGuiTestEngine_FindInfoTask(ImGuiTestEngine* engine, ImGuiID id)
{
    for (int task_n = 0; task_n < engine->InfoTasks.Size; task_n++)
    {
        ImGuiTestInfoTask* task = engine->InfoTasks[task_n];
        if (task->ID == id)
            return task;
    }
    return nullptr;
}

// Request information about one item.
// Will push a request for the test engine to process.
// Will return nullptr when results are not ready (or not available).
ImGuiTestItemInfo* ImGuiTestEngine_FindItemInfo(ImGuiTestEngine* engine, ImGuiID id, const char* debug_id)
{
    IM_ASSERT(id != 0);

    if (ImGuiTestInfoTask* task = ImGuiTestEngine_FindInfoTask(engine, id))
    {
        if (task->Result.TimestampMain + 2 >= engine->FrameCount)
        {
            task->FrameCount = engine->FrameCount; // Renew task
            return &task->Result;
        }
        return nullptr;
    }

    // Create task
    ImGuiTestInfoTask* task = IM_NEW(ImGuiTestInfoTask)();
    task->ID = id;
    task->FrameCount = engine->FrameCount;
    if (debug_id)
    {
        size_t debug_id_sz = strlen(debug_id);
        if (debug_id_sz < IM_ARRAYSIZE(task->DebugName) - 1)
        {
            memcpy(task->DebugName, debug_id, debug_id_sz + 1);
        }
        else
        {
            size_t header_sz = (size_t)(IM_ARRAYSIZE(task->DebugName) * 0.30f);
            size_t footer_sz = IM_ARRAYSIZE(task->DebugName) - 2 - header_sz;
            IM_ASSERT(header_sz > 0 && footer_sz > 0);
            ImFormatString(task->DebugName, IM_ARRAYSIZE(task->DebugName), "%.*s..%.*s", (int)header_sz, debug_id, (int)footer_sz, debug_id + debug_id_sz - footer_sz);
        }
    }
    engine->InfoTasks.push_back(task);

    return nullptr;
}

static void ImGuiTestEngine_ClearTests(ImGuiTestEngine* engine)
{
    for (int n = 0; n < engine->TestsAll.Size; n++)
        IM_DELETE(engine->TestsAll[n]);
    engine->TestsAll.clear();
    engine->TestsQueue.clear();
}

// Called at the beginning of a test to ensure no previous inputs leak into the new test
// FIXME-TESTS: Would make sense to reset mouse position as well?
void ImGuiTestEngine_ClearInput(ImGuiTestEngine* engine)
{
    IM_ASSERT(engine->UiContextTarget != nullptr);
    ImGuiContext& g = *engine->UiContextTarget;

    engine->Inputs.MouseButtonsValue = 0;
    engine->Inputs.Queue.clear();
    engine->Inputs.MouseWheel = ImVec2(0, 0);

    // FIXME: Necessary?
#if IMGUI_VERSION_NUM >= 18972
    g.IO.ClearEventsQueue();
#else
    g.InputEventsQueue.resize(0);
    g.IO.ClearInputCharacters();
#endif
    g.IO.ClearInputKeys();

    ImGuiTestEngine_ApplyInputToImGuiContext(engine);
}

bool ImGuiTestEngine_IsUsingSimulatedInputs(ImGuiTestEngine* engine)
{
    if (engine->UiContextActive)
        if (!ImGuiTestEngine_IsTestQueueEmpty(engine))
            if (!(engine->TestContext->RunFlags & ImGuiTestRunFlags_GuiFuncOnly))
                return true;
    return false;
}

// Setup inputs in the tested Dear ImGui context. Essentially we override the work of the backend here.
void ImGuiTestEngine_ApplyInputToImGuiContext(ImGuiTestEngine* engine)
{
    IM_ASSERT(engine->UiContextTarget != nullptr);
    ImGuiContext& g = *engine->UiContextTarget;
    ImGuiIO& io = g.IO;

    const bool use_simulated_inputs = ImGuiTestEngine_IsUsingSimulatedInputs(engine);
    if (!use_simulated_inputs)
        return;

    // Erase events submitted by backend
    for (int n = 0; n < g.InputEventsQueue.Size; n++)
        if (g.InputEventsQueue[n].AddedByTestEngine == false)
            g.InputEventsQueue.erase(&g.InputEventsQueue[n--]);

    // To support using ImGuiKey_NavXXXX shortcuts pointing to gamepad actions
    // FIXME-TEST-ENGINE: Should restore
    g.IO.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    g.IO.BackendFlags |= ImGuiBackendFlags_HasGamepad;

    // Special flags to stop submitting events
    if (engine->TestContext->RunFlags & ImGuiTestRunFlags_EnableRawInputs)
        return;

    const int input_event_count_prev = g.InputEventsQueue.Size;

    // Apply mouse viewport
#ifdef IMGUI_HAS_VIEWPORT
    ImGuiPlatformIO& platform_io = g.PlatformIO;
    ImGuiViewport* mouse_hovered_viewport;
    if (engine->Inputs.MouseHoveredViewport != 0)
        mouse_hovered_viewport = ImGui::FindViewportByID(engine->Inputs.MouseHoveredViewport); // Common case
    else
        mouse_hovered_viewport = ImGui::FindHoveredViewportFromPlatformWindowStack(engine->Inputs.MousePosValue); // Rarely used, some tests rely on this (e.g. "docking_dockspace_passthru_hover") may make it a opt-in feature instead?
    if (mouse_hovered_viewport && (mouse_hovered_viewport->Flags & ImGuiViewportFlags_NoInputs))
        mouse_hovered_viewport = nullptr;
    //if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        if (io.BackendFlags & ImGuiBackendFlags_HasMouseHoveredViewport)
            io.AddMouseViewportEvent(mouse_hovered_viewport ? mouse_hovered_viewport->ID : 0);
    bool mouse_hovered_viewport_focused = mouse_hovered_viewport && (mouse_hovered_viewport->Flags & ImGuiViewportFlags_IsFocused) != 0;
#endif

    // Apply mouse
    io.AddMousePosEvent(engine->Inputs.MousePosValue.x, engine->Inputs.MousePosValue.y);
    for (int n = 0; n < ImGuiMouseButton_COUNT; n++)
    {
        bool down = (engine->Inputs.MouseButtonsValue & (1 << n)) != 0;
        io.AddMouseButtonEvent(n, down);

        // A click simulate platform focus on the viewport.
#ifdef IMGUI_HAS_VIEWPORT
        if (down && mouse_hovered_viewport && !mouse_hovered_viewport_focused)
            if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
            {
                mouse_hovered_viewport_focused = true;
                engine->Inputs.Queue.push_back(ImGuiTestInput::ForViewportFocus(mouse_hovered_viewport->ID));
            }
#endif
    }

    // Apply mouse wheel
    // [OSX] Simulate OSX behavior of automatically swapping mouse wheel axis when SHIFT is held.
    // This is working in conjonction with the fact that ImGuiTestContext::MouseWheel() assume Windows-style behavior.
    ImVec2 wheel = engine->Inputs.MouseWheel;
    if (io.ConfigMacOSXBehaviors && (io.KeyMods & ImGuiMod_Shift)) // FIXME!!
        ImSwap(wheel.x, wheel.y);
    if (wheel.x != 0.0f || wheel.y != 0.0f)
        io.AddMouseWheelEvent(wheel.x, wheel.y);
    engine->Inputs.MouseWheel = ImVec2(0, 0);

    // Process input requests/queues
    if (engine->Inputs.Queue.Size > 0)
    {
        for (int n = 0; n < engine->Inputs.Queue.Size; n++)
        {
            const ImGuiTestInput& input = engine->Inputs.Queue[n];
            switch (input.Type)
            {
            case ImGuiTestInputType_Key:
            {
                ImGuiKeyChord key_chord = input.KeyChord;
#if IMGUI_VERSION_NUM >= 19016 && IMGUI_VERSION_NUM < 19063
                key_chord = ImGui::FixupKeyChord(&g, key_chord); // This will add ImGuiMod_Alt when pressing ImGuiKey_LeftAlt or ImGuiKey_LeftRight
#endif
#if IMGUI_VERSION_NUM >= 19063
                key_chord = ImGui::FixupKeyChord(key_chord);     // This will add ImGuiMod_Alt when pressing ImGuiKey_LeftAlt or ImGuiKey_LeftRight
#endif
                ImGuiKey key = (ImGuiKey)(key_chord & ~ImGuiMod_Mask_);
                ImGuiKeyChord mods = (key_chord & ImGuiMod_Mask_);
                if (mods != 0x00)
                {
                    // OSX conversion
#if IMGUI_VERSION_NUM >= 18912 && IMGUI_VERSION_NUM < 19063
                    if (mods & ImGuiMod_Shortcut)
                        mods = (mods & ~ImGuiMod_Shortcut) | (g.IO.ConfigMacOSXBehaviors ? ImGuiMod_Super : ImGuiMod_Ctrl);
#endif
#if IMGUI_VERSION_NUM >= 19063
                    // MacOS: swap Cmd(Super) and Ctrl WILL BE SWAPPED BACK BY io.AddKeyEvent()
                    if (g.IO.ConfigMacOSXBehaviors)
                    {
                        if ((mods & (ImGuiMod_Ctrl | ImGuiMod_Super)) == ImGuiMod_Super)
                            mods = (mods & ~ImGuiMod_Super) | ImGuiMod_Ctrl;
                        else if ((mods & (ImGuiMod_Ctrl | ImGuiMod_Super)) == ImGuiMod_Ctrl)
                            mods = (mods & ~ImGuiMod_Ctrl) | ImGuiMod_Super;
                        if (key == ImGuiKey_LeftSuper)      { key = ImGuiKey_LeftCtrl; }
                        else if (key == ImGuiKey_LeftSuper) { key = ImGuiKey_RightCtrl; }
                        else if (key == ImGuiKey_LeftCtrl)  { key = ImGuiKey_LeftSuper; }
                        else if (key == ImGuiKey_LeftCtrl)  { key = ImGuiKey_RightSuper; }
                    }
#endif
                    // Submitting a ImGuiMod_XXX without associated key needs to add at least one of the key.
                    if (mods & ImGuiMod_Ctrl)
                    {
                        io.AddKeyEvent(ImGuiMod_Ctrl, input.Down);
                        if (key != ImGuiKey_LeftCtrl && key != ImGuiKey_RightCtrl)
                            io.AddKeyEvent(ImGuiKey_LeftCtrl, input.Down);
                    }
                    if (mods & ImGuiMod_Shift)
                    {
                        io.AddKeyEvent(ImGuiMod_Shift, input.Down);
                        if (key != ImGuiKey_LeftShift && key != ImGuiKey_RightShift)
                            io.AddKeyEvent(ImGuiKey_LeftShift, input.Down);
                    }
                    if (mods & ImGuiMod_Alt)
                    {
                        io.AddKeyEvent(ImGuiMod_Alt, input.Down);
                        if (key != ImGuiKey_LeftAlt && key != ImGuiKey_RightAlt)
                            io.AddKeyEvent(ImGuiKey_LeftAlt, input.Down);
                    }
                    if (mods & ImGuiMod_Super)
                    {
                        io.AddKeyEvent(ImGuiMod_Super, input.Down);
                        if (key != ImGuiKey_LeftSuper && key != ImGuiKey_RightSuper)
                            io.AddKeyEvent(ImGuiKey_LeftSuper, input.Down);
                    }
                }

                if (key != ImGuiKey_None)
                    io.AddKeyEvent(key, input.Down);
                break;
            }
            case ImGuiTestInputType_Char:
            {
                IM_ASSERT(input.Char != 0);
                io.AddInputCharacter(input.Char);
                break;
            }
#ifdef IMGUI_HAS_VIEWPORT
            case ImGuiTestInputType_ViewportFocus:
            {
                if ((io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) == 0)
                    break;
                IM_ASSERT(engine->TestContext != nullptr);
                ImGuiViewport* viewport = ImGui::FindViewportByID(input.ViewportId);
                if (viewport == nullptr)
                    engine->TestContext->LogError("ViewportPlatform_SetWindowFocus(%08X): cannot find viewport anymore!", input.ViewportId);
                else if (platform_io.Platform_SetWindowFocus == nullptr)
                    engine->TestContext->LogError("ViewportPlatform_SetWindowFocus(%08X): backend's Platform_SetWindowFocus() is not set", input.ViewportId);
                else
                    platform_io.Platform_SetWindowFocus(viewport);
                break;
            }
            case ImGuiTestInputType_ViewportSetPos:
            {
                if ((io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) == 0)
                    break;
                IM_ASSERT(engine->TestContext != nullptr);
                ImGuiViewport* viewport = ImGui::FindViewportByID(input.ViewportId);
                if (viewport == nullptr)
                    engine->TestContext->LogError("ViewportPlatform_SetWindowPos(%08X): cannot find viewport anymore!", input.ViewportId);
                else if (platform_io.Platform_SetWindowPos == nullptr)
                    engine->TestContext->LogError("ViewportPlatform_SetWindowPos(%08X): backend's Platform_SetWindowPos() is not set", input.ViewportId);
                else
                    platform_io.Platform_SetWindowPos(viewport, input.ViewportPosSize);
                break;
            }
            case ImGuiTestInputType_ViewportSetSize:
            {
                if ((io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) == 0)
                    break;
                IM_ASSERT(engine->TestContext != nullptr);
                ImGuiViewport* viewport = ImGui::FindViewportByID(input.ViewportId);
                if (viewport == nullptr)
                    engine->TestContext->LogError("ViewportPlatform_SetWindowSize(%08X): cannot find viewport anymore!", input.ViewportId);
                else if (platform_io.Platform_SetWindowPos == nullptr)
                    engine->TestContext->LogError("ViewportPlatform_SetWindowSize(%08X): backend's Platform_SetWindowSize() is not set", input.ViewportId);
                else
                    platform_io.Platform_SetWindowSize(viewport, input.ViewportPosSize);
                break;
            }
            case ImGuiTestInputType_ViewportClose:
            {
                if ((io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) == 0)
                    break;
                IM_ASSERT(engine->TestContext != nullptr);
                ImGuiViewport* viewport = ImGui::FindViewportByID(input.ViewportId);
                if (viewport == nullptr)
                    engine->TestContext->LogError("ViewportPlatform_CloseWindow(%08X): cannot find viewport anymore!", input.ViewportId);
                else
                    viewport->PlatformRequestClose = true;
                // FIXME: doesn't apply to actual backend
                break;
            }
#else
            case ImGuiTestInputType_ViewportFocus:
            case ImGuiTestInputType_ViewportSetPos:
            case ImGuiTestInputType_ViewportSetSize:
            case ImGuiTestInputType_ViewportClose:
                break;
#endif
            case ImGuiTestInputType_None:
            default:
                break;
            }
        }

        engine->Inputs.Queue.resize(0);
    }

    const int input_event_count_curr = g.InputEventsQueue.Size;
    for (int n = input_event_count_prev; n < input_event_count_curr; n++)
        g.InputEventsQueue[n].AddedByTestEngine = true;
}

// FIXME: Trying to abort a running GUI test won't kill the app immediately.
static void ImGuiTestEngine_UpdateWatchdog(ImGuiTestEngine* engine, ImGuiContext* ui_ctx, double t0, double t1)
{
    IM_UNUSED(ui_ctx);
    ImGuiTestContext* test_ctx = engine->TestContext;

    if (engine->IO.ConfigRunSpeed != ImGuiTestRunSpeed_Fast || ImOsIsDebuggerPresent())
        return;

    if (test_ctx->RunFlags & ImGuiTestRunFlags_RunFromGui)
        return;

    const float timer_warn = engine->IO.ConfigWatchdogWarning;
    const float timer_kill_test = engine->IO.ConfigWatchdogKillTest;
    const float timer_kill_app = engine->IO.ConfigWatchdogKillApp;

    // Emit a warning and then fail the test after a given time.
    if (t0 < timer_warn && t1 >= timer_warn)
    {
        test_ctx->LogWarning("[Watchdog] Running time for '%s' is >%.f seconds, may be excessive.", test_ctx->Test->Name, timer_warn);
    }
    if (t0 < timer_kill_test && t1 >= timer_kill_test)
    {
        test_ctx->LogError("[Watchdog] Running time for '%s' is >%.f seconds, aborting.", test_ctx->Test->Name, timer_kill_test);
        IM_CHECK(false);
    }

    // Final safety watchdog in case the TestFunc is calling Yield() but never returning.
    // Note that we are not catching infinite loop cases where the TestFunc may be running but not yielding..
    if (t0 < timer_kill_app + 5.0f && t1 >= timer_kill_app + 5.0f)
    {
        test_ctx->LogError("[Watchdog] Emergency process exit as the test didn't return.");
        exit(1);
    }
}

static void ImGuiTestEngine_PreNewFrame(ImGuiTestEngine* engine, ImGuiContext* ui_ctx)
{
    if (engine->UiContextTarget != ui_ctx)
        return;
    IM_ASSERT(ui_ctx == GImGui);
    ImGuiContext& g = *ui_ctx;

    engine->CaptureContext.PreNewFrame();

    if (engine->ToolDebugRebootUiContext)
    {
        ImGuiTestEngine_RebootUiContext(engine);
        ui_ctx = engine->UiContextTarget;
        engine->ToolDebugRebootUiContext = false;
    }

    // Inject extra time into the Dear ImGui context
    if (engine->OverrideDeltaTime >= 0.0f)
    {
        ui_ctx->IO.DeltaTime = engine->OverrideDeltaTime;
        engine->OverrideDeltaTime = -1.0f;
    }

    // NewFrame() will increase this so we are +1 ahead at the time of calling this
    engine->FrameCount = g.FrameCount + 1;
    if (ImGuiTestContext* test_ctx = engine->TestContext)
    {
        double t0 = test_ctx->RunningTime;
        double t1 = t0 + ui_ctx->IO.DeltaTime;
        test_ctx->FrameCount++;
        test_ctx->RunningTime = t1;
        ImGuiTestEngine_UpdateWatchdog(engine, ui_ctx, t0, t1);
    }

    engine->PerfDeltaTime100.AddSample(g.IO.DeltaTime);
    engine->PerfDeltaTime500.AddSample(g.IO.DeltaTime);

    if (!ImGuiTestEngine_IsTestQueueEmpty(engine) && !engine->Abort)
    {
        // Abort testing by holding ESC
        // When running GuiFunc only main_io == simulated_io we test for a long hold.
        ImGuiIO& main_io = g.IO;
        for (auto& e : g.InputEventsQueue)
            if (e.Type == ImGuiInputEventType_Key && e.Key.Key == ImGuiKey_Escape)
                engine->Inputs.HostEscDown = e.Key.Down;
        engine->Inputs.HostEscDownDuration = engine->Inputs.HostEscDown ? (ImMax(engine->Inputs.HostEscDownDuration, 0.0f) + main_io.DeltaTime) : -1.0f;
        const bool abort = engine->Inputs.HostEscDownDuration >= 0.20f;
        if (abort)
        {
            if (engine->TestContext)
                engine->TestContext->LogWarning("User aborted (pressed ESC)");
            ImGuiTestEngine_AbortCurrentTest(engine);
        }
    }
    else
    {
        engine->Inputs.HostEscDown = false;
        engine->Inputs.HostEscDownDuration = -1.0f;
    }

    ImGuiTestEngine_ApplyInputToImGuiContext(engine);
    ImGuiTestEngine_UpdateHooks(engine);
}

static void ImGuiTestEngine_PostNewFrame(ImGuiTestEngine* engine, ImGuiContext* ui_ctx)
{
    if (engine->UiContextTarget != ui_ctx)
        return;
    IM_ASSERT(ui_ctx == GImGui);

    // Set initial mouse position to a decent value on startup
    if (engine->FrameCount == 1)
        engine->Inputs.MousePosValue = ImGui::GetMainViewport()->Pos;

    engine->IO.IsCapturing = engine->CaptureContext.IsCapturing();

    // Garbage collect unused tasks
    const int LOCATION_TASK_ELAPSE_FRAMES = 20;
    for (int task_n = 0; task_n < engine->InfoTasks.Size; task_n++)
    {
        ImGuiTestInfoTask* task = engine->InfoTasks[task_n];
        if (task->FrameCount < engine->FrameCount - LOCATION_TASK_ELAPSE_FRAMES)
        {
            IM_DELETE(task);
            engine->InfoTasks.erase(engine->InfoTasks.Data + task_n);
            task_n--;
        }
    }

    // Slow down whole app
    if (engine->ToolSlowDown)
        ImThreadSleepInMilliseconds(engine->ToolSlowDownMs);

    // Call user GUI function
    ImGuiTestEngine_RunGuiFunc(engine);
}

static void ImGuiTestEngine_PreEndFrame(ImGuiTestEngine* engine, ImGuiContext* ui_ctx)
{
    IM_UNUSED(ui_ctx);

    // Call user Test Function
    // (process on-going queues in a coroutine)
    ImGuiTestEngine_RunTestFunc(engine);

    // Update hooks and output flags
    ImGuiTestEngine_UpdateHooks(engine);

    // Disable vsync
    engine->IO.IsRequestingMaxAppSpeed = engine->IO.ConfigNoThrottle;
    if (engine->IO.ConfigRunSpeed == ImGuiTestRunSpeed_Fast && engine->IO.IsRunningTests)
        if (engine->TestContext && (engine->TestContext->RunFlags & ImGuiTestRunFlags_GuiFuncOnly) == 0)
            engine->IO.IsRequestingMaxAppSpeed = true;
}

static void ImGuiTestEngine_PreRender(ImGuiTestEngine* engine, ImGuiContext* ui_ctx)
{
    if (engine->UiContextTarget != ui_ctx)
        return;
    IM_ASSERT(ui_ctx == GImGui);

    engine->CaptureContext.PreRender();
}

static void ImGuiTestEngine_PostRender(ImGuiTestEngine* engine, ImGuiContext* ui_ctx)
{
    if (engine->UiContextTarget != ui_ctx)
        return;
    IM_ASSERT(ui_ctx == GImGui);

    // When test are running make sure real backend doesn't pick mouse cursor shape from tests.
    // (If were to instead set io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange in ImGuiTestEngine_RunTest() that would get us 99% of the way,
    // but unfortunately backend wouldn't restore normal shape after modified by OS decoration such as resize, so not enough..)
    ImGuiContext& g = *ui_ctx;
    if (!engine->IO.ConfigMouseDrawCursor && !g.IO.MouseDrawCursor && ImGuiTestEngine_IsUsingSimulatedInputs(engine))
        g.MouseCursor = ImGuiMouseCursor_Arrow;


    // Check ImDrawData integrity
    // This is currently a very cheap operation but may later become slower we if e.g. check idx boundaries.
#ifdef IMGUI_HAS_DOCK
    if (engine->IO.CheckDrawDataIntegrity)
        for (ImGuiViewport* viewport : ImGui::GetPlatformIO().Viewports)
            DrawDataVerifyMatchingBufferCount(viewport->DrawData);
#else
    if (engine->IO.CheckDrawDataIntegrity)
        DrawDataVerifyMatchingBufferCount(ImGui::GetDrawData());
#endif

    engine->CaptureContext.PostRender();
}

static void ImGuiTestEngine_RunGuiFunc(ImGuiTestEngine* engine)
{
    ImGuiTestContext* ctx = engine->TestContext;
    if (ctx && ctx->Test->GuiFunc)
    {
        if (!(ctx->RunFlags & ImGuiTestRunFlags_GuiFuncDisable))
        {
            ImGuiTestActiveFunc backup_active_func = ctx->ActiveFunc;
            ctx->ActiveFunc = ImGuiTestActiveFunc_GuiFunc;
            engine->TestContext->Test->GuiFunc(engine->TestContext);
            ctx->ActiveFunc = backup_active_func;
        }

        ImGuiTestEngine_ErrorRecoveryRun(engine);
    }
    if (ctx)
        ctx->FirstGuiFrame = false;
}

static void ImGuiTestEngine_RunTestFunc(ImGuiTestEngine* engine)
{
    ImGuiContext* ui_ctx = engine->UiContextTarget;

    // Process on-going queues in a coroutine
    // Run the test coroutine. This will resume the test queue from either the last point the test called YieldFromCoroutine(),
    // or the loop in ImGuiTestEngine_TestQueueCoroutineMain that does so if no test is running.
    // If you want to breakpoint the point execution continues in the test code, breakpoint the exit condition in YieldFromCoroutine()
    const int input_queue_size_before = ui_ctx->InputEventsQueue.Size;
    engine->IO.CoroutineFuncs->RunFunc(engine->TestQueueCoroutine);

    // Events added by TestFunc() marked automaticaly to not be deleted
    if (engine->TestContext && (engine->TestContext->RunFlags & ImGuiTestRunFlags_EnableRawInputs))
        for (int n = input_queue_size_before; n < ui_ctx->InputEventsQueue.Size; n++)
            ui_ctx->InputEventsQueue[n].AddedByTestEngine = true;
}

// Main function for the test coroutine
static void ImGuiTestEngine_TestQueueCoroutineMain(void* engine_opaque)
{
    ImGuiTestEngine* engine = (ImGuiTestEngine*)engine_opaque;
    while (!engine->TestQueueCoroutineShouldExit)
    {
        ImGuiTestEngine_ProcessTestQueue(engine);
        engine->IO.CoroutineFuncs->YieldFunc();
    }
}

static void ImGuiTestEngine_DisableWindowInputs(ImGuiWindow* window)
{
    window->DisableInputsFrames = 1;
    for (ImGuiWindow* child_window : window->DC.ChildWindows)
        ImGuiTestEngine_DisableWindowInputs(child_window);
}

// Yield control back from the TestFunc to the main update + GuiFunc, for one frame.
void ImGuiTestEngine_Yield(ImGuiTestEngine* engine)
{
    ImGuiTestContext* ctx = engine->TestContext;

    // Can only yield in the test func!
    if (ctx)
    {
        IM_ASSERT(ctx->ActiveFunc == ImGuiTestActiveFunc_TestFunc && "Can only yield inside TestFunc()!");
        for (ImGuiWindow* window : ctx->ForeignWindowsToHide)
        {
            window->HiddenFramesForRenderOnly = 2;          // Hide root window
            ImGuiTestEngine_DisableWindowInputs(window);    // Disable inputs for root window and all it's children recursively
        }
    }

    engine->IO.CoroutineFuncs->YieldFunc();
}

void ImGuiTestEngine_SetDeltaTime(ImGuiTestEngine* engine, float delta_time)
{
    IM_ASSERT(delta_time >= 0.0f);
    engine->OverrideDeltaTime = delta_time;
}

int ImGuiTestEngine_GetFrameCount(ImGuiTestEngine* engine)
{
    return engine->FrameCount;
}

const char* ImGuiTestEngine_GetStatusName(ImGuiTestStatus v)
{
    static const char* names[ImGuiTestStatus_COUNT] = { "Unknown", "Success", "Queued", "Running", "Error", "Suspended" };
    IM_STATIC_ASSERT(IM_ARRAYSIZE(names) == ImGuiTestStatus_COUNT);
    if (v >= 0 && v < IM_ARRAYSIZE(names))
        return names[v];
    return "N/A";
}

const char* ImGuiTestEngine_GetRunSpeedName(ImGuiTestRunSpeed v)
{
    static const char* names[ImGuiTestRunSpeed_COUNT] = { "Fast", "Normal", "Cinematic" };
    IM_STATIC_ASSERT(IM_ARRAYSIZE(names) == ImGuiTestRunSpeed_COUNT);
    if (v >= 0 && v < IM_ARRAYSIZE(names))
        return names[v];
    return "N/A";
}

const char* ImGuiTestEngine_GetVerboseLevelName(ImGuiTestVerboseLevel v)
{
    static const char* names[ImGuiTestVerboseLevel_COUNT] = { "Silent", "Error", "Warning", "Info", "Debug", "Trace" };
    IM_STATIC_ASSERT(IM_ARRAYSIZE(names) == ImGuiTestVerboseLevel_COUNT);
    if (v >= 0 && v < IM_ARRAYSIZE(names))
        return names[v];
    return "N/A";
}

bool ImGuiTestEngine_CaptureScreenshot(ImGuiTestEngine* engine, ImGuiCaptureArgs* args)
{
    if (engine->IO.ScreenCaptureFunc == nullptr)
    {
        IM_ASSERT(0);
        return false;
    }

    IM_ASSERT(engine->CaptureCurrentArgs == nullptr && "Nested captures are not supported.");

    // Graphics API must render a window so it can be captured
    // FIXME: This should work without this, as long as Present vs Vsync are separated (we need a Present, we don't need Vsync)
    const ImGuiTestRunSpeed backup_run_speed = engine->IO.ConfigRunSpeed;
    engine->IO.ConfigRunSpeed = ImGuiTestRunSpeed_Fast;

    const int frame_count = engine->FrameCount;

    // Because we rely on window->ContentSize for stitching, let 1 extra frame elapse to make sure any
    // windows which contents have changed in the last frame get a correct window->ContentSize value.
    // FIXME: Can remove this yield if not stitching
    if ((args->InFlags & ImGuiCaptureFlags_Instant) == 0)
        ImGuiTestEngine_Yield(engine);

    // This will yield until ImGuiTestEngine_PostSwap() -> ImGuiCaptureContext::CaptureUpdate() return false.
    // - CaptureUpdate() will call user provided test_io.ScreenCaptureFunc() function
    // - Capturing is likely to take multiple frames depending on settings.
    int frames_yielded = 0;
    engine->CaptureCurrentArgs = args;
    engine->PostSwapCalled = false;
    while (engine->CaptureCurrentArgs != nullptr)
    {
        ImGuiTestEngine_Yield(engine);
        frames_yielded++;
        if (frames_yielded > 4)
            IM_ASSERT(engine->PostSwapCalled && "ImGuiTestEngine_PostSwap() is not being called by application! Must be called in order.");
    }

    // Verify that the ImGuiCaptureFlags_Instant flag got honored
    if (args->InFlags & ImGuiCaptureFlags_Instant)
        IM_ASSERT(frame_count + 1 == engine->FrameCount);

    engine->IO.ConfigRunSpeed = backup_run_speed;
    return true;
}

bool ImGuiTestEngine_CaptureBeginVideo(ImGuiTestEngine* engine, ImGuiCaptureArgs* args)
{
    if (engine->IO.ScreenCaptureFunc == nullptr)
    {
        IM_ASSERT(0);
        return false;
    }

    IM_ASSERT(engine->CaptureCurrentArgs == nullptr && "Nested captures are not supported.");

    // RunSpeed set to Fast      -> Switch to Cinematic, no throttle
    // RunSpeed set to Normal    -> No change
    // RunSpeed set to Cinematic -> No change
    engine->BackupConfigRunSpeed = engine->IO.ConfigRunSpeed;
    engine->BackupConfigNoThrottle = engine->IO.ConfigNoThrottle;
    if (engine->IO.ConfigRunSpeed == ImGuiTestRunSpeed_Fast)
    {
        engine->IO.ConfigRunSpeed = ImGuiTestRunSpeed_Cinematic;
        engine->IO.ConfigNoThrottle = true;
        engine->IO.ConfigFixedDeltaTime = 1.0f / 60.0f;
    }
    engine->CaptureCurrentArgs = args;
    engine->CaptureContext.BeginVideoCapture(args);
    return true;
}

bool ImGuiTestEngine_CaptureEndVideo(ImGuiTestEngine* engine, ImGuiCaptureArgs* args)
{
    IM_UNUSED(args);
    IM_ASSERT(engine->CaptureContext.IsCapturingVideo() && "No video capture is in progress.");

    engine->CaptureContext.EndVideoCapture();
    while (engine->CaptureCurrentArgs != nullptr)   // Wait until last frame is captured and gif is saved.
        ImGuiTestEngine_Yield(engine);
    engine->IO.ConfigRunSpeed = engine->BackupConfigRunSpeed;
    engine->IO.ConfigNoThrottle = engine->BackupConfigNoThrottle;
    engine->IO.ConfigFixedDeltaTime = 0;
    engine->CaptureCurrentArgs = nullptr;
    return true;
}

static void ImGuiTestEngine_ProcessTestQueue(ImGuiTestEngine* engine)
{
    // Avoid tracking scrolling in UI when running a single test
    const bool track_scrolling = (engine->TestsQueue.Size > 1) || (engine->TestsQueue.Size == 1 && (engine->TestsQueue[0].RunFlags & ImGuiTestRunFlags_RunFromCommandLine));

    // Backup some state
    ImGuiIO& io = ImGui::GetIO();
    const char* backup_ini_filename = io.IniFilename;
    ImGuiWindow* backup_nav_window = engine->UiContextTarget->NavWindow;
    io.IniFilename = nullptr;

    int ran_tests = 0;
    engine->BatchStartTime = ImTimeGetInMicroseconds();
    engine->IO.IsRunningTests = true;
    for (int n = 0; n < engine->TestsQueue.Size; n++)
    {
        ImGuiTestRunTask* run_task = &engine->TestsQueue[n];
        IM_ASSERT(run_task->Test->Output.Status == ImGuiTestStatus_Queued);

        // FIXME-TESTS: Blind mode not supported
        IM_ASSERT(engine->UiContextTarget != nullptr);
        IM_ASSERT(engine->UiContextActive == nullptr);
        engine->UiContextActive = engine->UiContextTarget;
        engine->UiSelectedTest = run_task->Test;
        if (track_scrolling)
            engine->UiSelectAndScrollToTest = run_task->Test;

        // Run test
        ImGuiTestEngine_RunTest(engine, nullptr, run_task->Test, run_task->RunFlags);

        // Cleanup
        IM_ASSERT(engine->TestContext == nullptr);
        IM_ASSERT(engine->UiContextActive == engine->UiContextTarget);
        engine->UiContextActive = nullptr;

        // Auto select the first error test
        //if (test->Status == ImGuiTestStatus_Error)
        //    if (engine->UiSelectedTest == nullptr || engine->UiSelectedTest->Status != ImGuiTestStatus_Error)
        //        engine->UiSelectedTest = test;

        ran_tests++;
    }
    engine->IO.IsRunningTests = false;
    engine->BatchEndTime = ImTimeGetInMicroseconds();

    engine->Abort = false;
    engine->TestsQueue.clear();

    // Restore UI state (done after all ImGuiTestEngine_RunTest() are done)
    if (ran_tests)
    {
        if (engine->IO.ConfigRestoreFocusAfterTests)
            ImGui::FocusWindow(backup_nav_window);
    }
    io.IniFilename = backup_ini_filename;
}

bool ImGuiTestEngine_IsTestQueueEmpty(ImGuiTestEngine* engine)
{
    return engine->TestsQueue.Size == 0;
}

static bool ImGuiTestEngine_IsRunningTest(ImGuiTestEngine* engine, ImGuiTest* test)
{
    for (ImGuiTestRunTask& t : engine->TestsQueue)
        if (t.Test == test)
            return true;
    return false;
}

void ImGuiTestEngine_QueueTest(ImGuiTestEngine* engine, ImGuiTest* test, ImGuiTestRunFlags run_flags)
{
    if (ImGuiTestEngine_IsRunningTest(engine, test))
        return;

    // Detect lack of signal from imgui context, most likely not compiled with IMGUI_ENABLE_TEST_ENGINE=1
    // FIXME: Why is in this function?
    if (engine->UiContextTarget && engine->FrameCount < engine->UiContextTarget->FrameCount - 2)
    {
        ImGuiTestEngine_AbortCurrentTest(engine);
        IM_ASSERT(0 && "Not receiving signal from core library. Did you call ImGuiTestEngine_CreateContext() with the correct context? Did you compile imgui/ with IMGUI_ENABLE_TEST_ENGINE=1?");
        test->Output.Status = ImGuiTestStatus_Error;
        return;
    }

    test->Output.Status = ImGuiTestStatus_Queued;

    ImGuiTestRunTask run_task;
    run_task.Test = test;
    run_task.RunFlags = run_flags;
    engine->TestsQueue.push_back(run_task);
}

// Called by IM_REGISTER_TEST(). Prefer calling IM_REGISTER_TEST() in your code so src_file/src_line are automatically passed.
ImGuiTest* ImGuiTestEngine_RegisterTest(ImGuiTestEngine* engine, const char* category, const char* name, const char* src_file, int src_line)
{
    ImGuiTestGroup group = ImGuiTestGroup_Tests;
    if (strcmp(category, "perf") == 0)
        group = ImGuiTestGroup_Perfs;

    ImGuiTest* t = IM_NEW(ImGuiTest)();
    t->Group = group;
    t->Category = category;
    t->Name = name;
    t->SourceFile = src_file;
    t->SourceLine = t->SourceLineEnd = src_line;
    engine->TestsAll.push_back(t);
    engine->TestsSourceLinesDirty = true;

    return t;
}

void ImGuiTestEngine_UnregisterTest(ImGuiTestEngine* engine, ImGuiTest* test)
{
    // Cannot unregister a running test. Please contact us if you need this.
    if (engine->TestContext != nullptr)
        IM_ASSERT(engine->TestContext->Test != test);

    // Remove from lists
    bool found = engine->TestsAll.find_erase(test);
    IM_ASSERT(found); // Calling ImGuiTestEngine_UnregisterTest() on an unknown test.
    for (int n = 0; n < engine->TestsQueue.Size; n++)
    {
        ImGuiTestRunTask& task = engine->TestsQueue[n];
        if (task.Test == test)
        {
            engine->TestsQueue.erase(&task);
            n--;
        }
    }
    if (engine->UiSelectAndScrollToTest == test)
        engine->UiSelectAndScrollToTest = nullptr;
    if (engine->UiSelectedTest == test)
        engine->UiSelectedTest = nullptr;
    engine->TestsSourceLinesDirty = true;

    IM_DELETE(test);
}

void ImGuiTestEngine_UnregisterAllTests(ImGuiTestEngine* engine)
{
    // Cannot unregister a running test. Please contact us if you need this.
    IM_ASSERT(engine->TestContext == nullptr);

    engine->TestsAll.clear_delete();
    engine->TestsQueue.clear();
    engine->UiSelectAndScrollToTest = nullptr;
    engine->UiSelectedTest = nullptr;
    engine->TestsSourceLinesDirty = true;
}

ImGuiPerfTool* ImGuiTestEngine_GetPerfTool(ImGuiTestEngine* engine)
{
    return engine->PerfTool;
}

// Filter tests by a specified query. Query is composed of one or more comma-separated filter terms optionally prefixed/suffixed with modifiers.
// Available modifiers:
// - '-' prefix excludes tests matched by the term.
// - '^' prefix anchors term matching to the start of the string.
// - '$' suffix anchors term matching to the end of the string.
// Special keywords:
// - "all"   : all tests, no matter what group they are in.
// - "tests" : tests in ImGuiTestGroup_Tests group.
// - "perfs" : tests in ImGuiTestGroup_Perfs group.
// Example queries:
// - ""      : empty query matches no tests.
// - "^nav_" : all tests with name starting with "nav_".
// - "_nav$" : all tests with name ending with "_nav".
// - "-xxx"  : all tests and perfs that do not contain "xxx".
// - "tests,-scroll,-^nav_" : all tests (but no perfs) that do not contain "scroll" in their name and does not start with "nav_".
// Note: while we borrowed ^ and $ from regex conventions, we do not support actual regex syntax except for behavior of these two modifiers.
bool ImGuiTestEngine_PassFilter(ImGuiTest* test, const char* filter_specs)
{
    IM_ASSERT(filter_specs != nullptr);
    auto str_iequal = [](const char* s1, const char* s2, const char* s2_end)
    {
        size_t s2_len = (size_t)(s2_end - s2);
        if (strlen(s1) != s2_len) return false;
        return ImStrnicmp(s1, s2, s2_len) == 0;
    };

    auto str_iendswith = [&str_iequal](const char* s1, const char* s2, const char* s2_end)
    {
        size_t s1_len = strlen(s1);
        size_t s2_len = (size_t)(s2_end - s2);
        if (s1_len < s2_len) return false;
        s1 = s1 + s1_len - s2_len;
        return str_iequal(s1, s2, s2_end);
    };

    bool include = false;
    const char* prefixes = "^-";

    // When filter starts with exclude condition, we assume we have included all tests from the start. This enables
    // writing "-window" instead of "all,-window".
    for (int i = 0; filter_specs[i]; i++)
        if (filter_specs[i] == '-')
            include = true; // First filter is exclusion
        else if (strchr(prefixes, filter_specs[i]) == nullptr)
            break;          // End of prefixes

    for (const char* filter_start = filter_specs; filter_start[0];)
    {
        // Filter modifiers
        bool is_exclude = false;
        bool is_anchor_to_start = false;
        bool is_anchor_to_end = false;
        for (;;)
        {
            if (filter_start[0] == '-')
                is_exclude = true;
            else if (filter_start[0] == '^')
                is_anchor_to_start = true;
            else
                break;
            filter_start++;
        }

        const char* filter_end = strstr(filter_start, ",");
        filter_end = filter_end ? filter_end : filter_start + strlen(filter_start);
        is_anchor_to_end = filter_end[-1] == '$';
        if (is_anchor_to_end)
            filter_end--;

        if (str_iequal("all", filter_start, filter_end))
            include = !is_exclude;
        else if (str_iequal("tests", filter_start, filter_end))
            include = (test->Group == ImGuiTestGroup_Tests) ? !is_exclude : include;
        else if (str_iequal("perfs", filter_start, filter_end))
            include = (test->Group == ImGuiTestGroup_Perfs) ? !is_exclude : include;
        else
        {
            // General filtering
            for (int n = 0; n < 2; n++)
            {
                const char* name = (n == 0) ? test->Name : test->Category;

                bool match = true;

                // "foo" - match a substring.
                if (!is_anchor_to_start && !is_anchor_to_end)
                    match = ImStristr(name, nullptr, filter_start, filter_end) != nullptr;

                // "^foo" - match start of the string.
                // "foo$" - match end of the string.
                // FIXME: (minor) '^aaa$' will incorrectly match 'aaabbbaaa'.
                if (is_anchor_to_start)
                    match &= ImStrnicmp(name, filter_start, filter_end - filter_start) == 0;
                if (is_anchor_to_end)
                    match &= str_iendswith(name, filter_start, filter_end);

                if (match)
                {
                    include = is_exclude ? false : true;
                    break;
                }
            }
        }

        while (filter_end[0] == ',' || filter_end[0] == '$')
            filter_end++;
        filter_start = filter_end;
    }
    return include;
}

void ImGuiTestEngine_QueueTests(ImGuiTestEngine* engine, ImGuiTestGroup group, const char* filter_str, ImGuiTestRunFlags run_flags)
{
    IM_ASSERT(group >= ImGuiTestGroup_Unknown && group < ImGuiTestGroup_COUNT);
    for (int n = 0; n < engine->TestsAll.Size; n++)
    {
        ImGuiTest* test = engine->TestsAll[n];
        if (group != ImGuiTestGroup_Unknown && test->Group != group)
            continue;

        if (filter_str != nullptr)
            if (!ImGuiTestEngine_PassFilter(test, filter_str))
                continue;

        ImGuiTestEngine_QueueTest(engine, test, run_flags);
    }
}

void ImGuiTestEngine_UpdateTestsSourceLines(ImGuiTestEngine* engine)
{
    engine->TestsSourceLinesDirty = false;
    if (engine->TestsAll.empty())
        return;

    struct TestAndSourceLine { ImGuiTest* Test; int SourceLine; };

    ImPool<ImVector<TestAndSourceLine>> db;
    for (ImGuiTest* test : engine->TestsAll)
    {
        if (test->SourceFile == nullptr)
            continue;
        ImGuiID srcfile_hash = ImHashStr(test->SourceFile);
        ImVector<TestAndSourceLine>* srcfile_tests = db.GetOrAddByKey(srcfile_hash);
        srcfile_tests->push_back({ test, test->SourceLine });
    }

    int pool_size = db.GetMapSize();
    for (int map_n = 0; map_n < pool_size; map_n++)
        if (ImVector<TestAndSourceLine>* srcfile_tests = db.TryGetMapData(map_n))
        {
            ImQsort(srcfile_tests->Data, (size_t)srcfile_tests->Size, sizeof(TestAndSourceLine), [](const void* lhs, const void* rhs)
            { return ((const TestAndSourceLine*)lhs)->SourceLine - ((const TestAndSourceLine*)rhs)->SourceLine; });
            for (int test_n = 0; test_n < srcfile_tests->Size - 1; test_n++)
            {
                TestAndSourceLine& tasl = (*srcfile_tests)[test_n];
                IM_ASSERT(tasl.Test->SourceLine == tasl.SourceLine);
                tasl.Test->SourceLineEnd = (*srcfile_tests)[test_n + 1].SourceLine - 1;
            }
        }
}

// count_remaining could be >0 if e.g. called during a crash handler or aborting a run.
void ImGuiTestEngine_GetResultSummary(ImGuiTestEngine* engine, ImGuiTestEngineResultSummary* out_results)
{
    int count_tested = 0;
    int count_success = 0;
    int count_remaining = 0;
    for (int n = 0; n < engine->TestsAll.Size; n++)
    {
        ImGuiTest* test = engine->TestsAll[n];
        if (test->Output.Status == ImGuiTestStatus_Unknown)
            continue;
        if (test->Output.Status == ImGuiTestStatus_Queued)
        {
            count_remaining++;
            continue;
        }
        IM_ASSERT(test->Output.Status != ImGuiTestStatus_Running);
        count_tested++;
        if (test->Output.Status == ImGuiTestStatus_Success)
            count_success++;
    }
    out_results->CountTested = count_tested;
    out_results->CountSuccess = count_success;
    out_results->CountInQueue = count_remaining;
}

// Get a copy of the test list
void ImGuiTestEngine_GetTestList(ImGuiTestEngine* engine, ImVector<ImGuiTest*>* out_tests)
{
    *out_tests = engine->TestsAll;
}

// Get a copy of the test queue
void ImGuiTestEngine_GetTestQueue(ImGuiTestEngine* engine, ImVector<ImGuiTestRunTask>* out_tests)
{
    *out_tests = engine->TestsQueue;
}

static void ImGuiTestEngine_UpdateHooks(ImGuiTestEngine* engine)
{
    ImGuiContext* ui_ctx = engine->UiContextTarget;
    IM_ASSERT(ui_ctx->TestEngine == engine);
    bool want_hooking = false;

    //if (engine->TestContext != nullptr)
    //    want_hooking = true;

    if (engine->InfoTasks.Size > 0)
        want_hooking = true;
    if (engine->FindByLabelTask.InSuffix != nullptr)
        want_hooking = true;
    if (engine->GatherTask.InParentID != 0)
        want_hooking = true;

    // Update test engine specific hooks
    ui_ctx->TestEngineHookItems = want_hooking;
}

struct ImGuiTestContextUiContextBackup
{
    ImGuiIO             IO;
#if IMGUI_VERSION_NUM >= 19103
    ImGuiPlatformIO     PlatformIO;
#endif
    ImGuiStyle          Style;
    ImGuiDebugLogFlags  DebugLogFlags;
    ImGuiKeyChord       ConfigNavWindowingKeyNext;
    ImGuiKeyChord       ConfigNavWindowingKeyPrev;
#if IMGUI_VERSION_NUM >= 19123
    ImGuiErrorCallback  ErrorCallback;
    void*               ErrorCallbackUserData;
#endif

    void Backup(ImGuiContext& g)
    {
        IO = g.IO;
#if IMGUI_VERSION_NUM >= 19103
        PlatformIO = g.PlatformIO;
#endif
        Style = g.Style;
        DebugLogFlags = g.DebugLogFlags;
#if IMGUI_VERSION_NUM >= 18837
        ConfigNavWindowingKeyNext = g.ConfigNavWindowingKeyNext;
        ConfigNavWindowingKeyPrev = g.ConfigNavWindowingKeyPrev;
#endif
#if IMGUI_VERSION_NUM >= 19123
        ErrorCallback = g.ErrorCallback;
        ErrorCallbackUserData = g.ErrorCallbackUserData;
#endif
        memset(IO.MouseDown, 0, sizeof(IO.MouseDown));
        for (int n = 0; n < IM_ARRAYSIZE(IO.KeysData); n++)
            IO.KeysData[n].Down = false;
    }

    void Restore(ImGuiContext& g)
    {
#if IMGUI_VERSION_NUM < 18993
        IO.MetricsActiveAllocations = g.IO.MetricsActiveAllocations;
#endif
        g.IO = IO;
#if IMGUI_VERSION_NUM >= 19103
        // FIXME: This will invalidate pointers platform_io.Monitors[].
        // User is not expected to point to monitor ever, but some may do that....
        //g.PlatformIO = PlatformIO;
        RestoreClipboardFuncs(g); // We only need to restore this for now. We'll find if we need more.
#endif
        g.Style = Style;
        g.DebugLogFlags = DebugLogFlags;
#if IMGUI_VERSION_NUM >= 18837
        g.ConfigNavWindowingKeyNext = ConfigNavWindowingKeyNext;
        g.ConfigNavWindowingKeyPrev = ConfigNavWindowingKeyPrev;
#endif
#if IMGUI_VERSION_NUM >= 19123
        g.ErrorCallback = ErrorCallback;
        g.ErrorCallbackUserData = ErrorCallbackUserData;
#endif
    }
    void RestoreClipboardFuncs(ImGuiContext& g)
    {
#if IMGUI_VERSION_NUM >= 19103
        g.PlatformIO.Platform_GetClipboardTextFn = PlatformIO.Platform_GetClipboardTextFn;
        g.PlatformIO.Platform_SetClipboardTextFn = PlatformIO.Platform_SetClipboardTextFn;
        g.PlatformIO.Platform_ClipboardUserData = PlatformIO.Platform_ClipboardUserData;
#else
        g.IO.GetClipboardTextFn = IO.GetClipboardTextFn;
        g.IO.SetClipboardTextFn = IO.SetClipboardTextFn;
        g.IO.ClipboardUserData = IO.ClipboardUserData;
#endif
    }
};

// FIXME: Work toward simplifying this function?
void ImGuiTestEngine_RunTest(ImGuiTestEngine* engine, ImGuiTestContext* parent_ctx, ImGuiTest* test, ImGuiTestRunFlags run_flags)
{
    ImGuiTestContext stack_ctx;
    ImGuiCaptureArgs stack_capture_args;
    ImGuiTestContext* ctx;

    if (run_flags & ImGuiTestRunFlags_ShareTestContext)
    {
        // Reuse existing test context
        IM_ASSERT(parent_ctx != nullptr);
        ctx = parent_ctx;
    }
    else
    {
        // Create a test context
        ctx = &stack_ctx;
        ctx->Engine = engine;
        ctx->EngineIO = &engine->IO;
        ctx->Inputs = &engine->Inputs;
        ctx->CaptureArgs = &stack_capture_args;
        ctx->UserVars = nullptr;
        ctx->PerfStressAmount = engine->IO.PerfStressAmount;
#ifdef IMGUI_HAS_DOCK
        ctx->HasDock = true;
#else
        ctx->HasDock = false;
#endif
    }

    ImGuiTestOutput* test_output;
    if (parent_ctx == nullptr)
    {
        ctx->Test = test;
        test_output = ctx->TestOutput = &test->Output;
        test_output->StartTime = ImTimeGetInMicroseconds();
    }
    else
    {
        ctx->Test = parent_ctx->Test;
        test_output = ctx->TestOutput = parent_ctx->TestOutput;
    }

    if (engine->Abort)
    {
        test_output->Status = ImGuiTestStatus_Unknown;
        if (parent_ctx == nullptr)
            test_output->EndTime = test_output->StartTime;
        ctx->Test = nullptr;
        ctx->TestOutput = nullptr;
        ctx->CaptureArgs = nullptr;
        return;
    }

    test_output->Status = ImGuiTestStatus_Running;

    ctx->RunFlags = run_flags;
    ctx->UiContext = engine->UiContextActive;

    engine->TestContext = ctx;
    ImGuiTestEngine_UpdateHooks(engine);

    void* backup_user_vars = nullptr;
    ImGuiTestGenericVars backup_generic_vars;
    if (run_flags & ImGuiTestRunFlags_ShareVars)
    {
        // Share user vars and generic vars
        IM_CHECK_SILENT(parent_ctx != nullptr);
        IM_CHECK_SILENT(test->VarsSize == parent_ctx->Test->VarsSize);
        IM_CHECK_SILENT(test->VarsConstructor == parent_ctx->Test->VarsConstructor);
        IM_CHECK_SILENT(test->VarsPostConstructor == parent_ctx->Test->VarsPostConstructor);
        IM_CHECK_SILENT(test->VarsPostConstructorUserFn == parent_ctx->Test->VarsPostConstructorUserFn);
        IM_CHECK_SILENT(test->VarsDestructor == parent_ctx->Test->VarsDestructor);
        if ((run_flags & ImGuiTestRunFlags_ShareTestContext) == 0)
        {
            ctx->GenericVars = parent_ctx->GenericVars;
            ctx->UserVars = parent_ctx->UserVars;
        }
    }
    else
    {
        // Create user vars
        if (run_flags & ImGuiTestRunFlags_ShareTestContext)
        {
            backup_user_vars = parent_ctx->UserVars;
            backup_generic_vars = parent_ctx->GenericVars;
        }
        ctx->GenericVars.Clear();
        if (test->VarsConstructor != nullptr)
        {
            ctx->UserVars = IM_ALLOC(test->VarsSize);
            memset(ctx->UserVars, 0, test->VarsSize);
            test->VarsConstructor(ctx->UserVars);
            if (test->VarsPostConstructor != nullptr && test->VarsPostConstructorUserFn != nullptr)
                test->VarsPostConstructor(ctx, ctx->UserVars, test->VarsPostConstructorUserFn);
        }
    }

    // Log header
    if (parent_ctx == nullptr)
    {
        ctx->LogEx(ImGuiTestVerboseLevel_Info, ImGuiTestLogFlags_NoHeader, "----------------------------------------------------------------------"); // Intentionally TTY only (just before clear: make it a flag?)
        test_output->Log.Clear();
        ctx->LogWarning("Test: '%s' '%s'..", test->Category, test->Name);
    }
    else
    {
        ctx->LogWarning("Child Test: '%s' '%s'..", test->Category, test->Name);
        ctx->LogDebug("(ShareVars=%d ShareTestContext=%d)", (run_flags & ImGuiTestRunFlags_ShareVars) ? 1 : 0, (run_flags & ImGuiTestRunFlags_ShareTestContext) ? 1 : 0);
    }

    // Clear ImGui inputs to avoid key/mouse leaks from one test to another
    ImGuiTestEngine_ClearInput(engine);

    // Backup entire IO and style. Allows tests modifying them and not caring about restoring state.
    ImGuiTestContextUiContextBackup backup_ui_context;
    backup_ui_context.Backup(*ctx->UiContext);

    // Setup IO: software mouse cursor, viewport support
    ImGuiIO& io = ctx->UiContext->IO;
    if (engine->IO.ConfigMouseDrawCursor)
        io.MouseDrawCursor = true;
#ifdef IMGUI_HAS_VIEWPORT
    // We always fill io.MouseHoveredViewport manually (maintained in ImGuiTestInputs::SimulatedIO)
    // so ensure we don't leave a chance to Dear ImGui to interpret things differently.
    // FIXME: As written, this would prevent tests from toggling ImGuiConfigFlags_ViewportsEnable and have correct value for ImGuiBackendFlags_HasMouseHoveredViewport
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        io.BackendFlags |= ImGuiBackendFlags_HasMouseHoveredViewport;
    else
        io.BackendFlags &= ~ImGuiBackendFlags_HasMouseHoveredViewport;
#endif

    // Setup IO: override clipboard
    if ((ctx->RunFlags & ImGuiTestRunFlags_GuiFuncOnly) == 0)
    {
#if IMGUI_VERSION_NUM >= 19103
        ImGuiPlatformIO& platform_io = ctx->UiContext->PlatformIO;
        platform_io.Platform_GetClipboardTextFn = [](ImGuiContext* ui_ctx) -> const char*
        {
            ImGuiTestContext* ctx = (ImGuiTestContext*)ui_ctx->PlatformIO.Platform_ClipboardUserData;
            return ctx->Clipboard.empty() ? "" : ctx->Clipboard.Data;
        };
        platform_io.Platform_SetClipboardTextFn = [](ImGuiContext* ui_ctx, const char* text)
        {
            ImGuiTestContext* ctx = (ImGuiTestContext*)ui_ctx->PlatformIO.Platform_ClipboardUserData;
            ctx->Clipboard.resize((int)strlen(text) + 1);
            strcpy(ctx->Clipboard.Data, text);
        };
        platform_io.Platform_ClipboardUserData = ctx;
#else
        io.GetClipboardTextFn = [](void* user_data) -> const char*
        {
            ImGuiTestContext* ctx = (ImGuiTestContext*)user_data;
            return ctx->Clipboard.empty() ? "" : ctx->Clipboard.Data;
        };
        io.SetClipboardTextFn = [](void* user_data, const char* text)
        {
            ImGuiTestContext* ctx = (ImGuiTestContext*)user_data;
            ctx->Clipboard.resize((int)strlen(text) + 1);
            strcpy(ctx->Clipboard.Data, text);
        };
        io.ClipboardUserData = ctx;
#endif
    }

    // Setup IO: error handling
    ImGuiTestEngine_ErrorRecoverySetup(engine);

    // Mark as currently running the TestFunc (this is the only time when we are allowed to yield)
    IM_ASSERT(ctx->ActiveFunc == ImGuiTestActiveFunc_None || ctx->ActiveFunc == ImGuiTestActiveFunc_TestFunc);
    ImGuiTestActiveFunc backup_active_func = ctx->ActiveFunc;
    ctx->ActiveFunc = ImGuiTestActiveFunc_TestFunc;
    ctx->FirstGuiFrame = (test->GuiFunc != nullptr) ? true : false;
    ctx->FrameCount = parent_ctx ? parent_ctx->FrameCount : 0;
    ctx->ErrorCounter = 0;
    ctx->SetRef("");
    ctx->SetInputMode(ImGuiInputSource_Mouse);
    ctx->UiContext->NavInputSource = ImGuiInputSource_Keyboard;
    ctx->Clipboard.clear();

    // Warm up GUI
    // - We need one mandatory frame running GuiFunc before running TestFunc
    // - We add a second frame, to avoid running tests while e.g. windows are typically appearing for the first time, hidden,
    // measuring their initial size. Most tests are going to be more meaningful with this stabilized base.
    if (!(test->Flags & ImGuiTestFlags_NoGuiWarmUp))
    {
        ctx->FrameCount -= 2;
        ctx->Yield();
        if (test_output->Status == ImGuiTestStatus_Running) // To allow GuiFunc calling Finish() in first frame
            ctx->Yield();
    }
    ctx->FirstTestFrameCount = ctx->FrameCount;

    // Call user test function (optional)
    if (ctx->RunFlags & ImGuiTestRunFlags_GuiFuncOnly)
    {
        // No test function
        while (!engine->Abort && test_output->Status == ImGuiTestStatus_Running)
            ctx->Yield();
    }
    else
    {
        if (test->TestFunc)
        {
            // Test function
            test->TestFunc(ctx);

            // In case test failed without finishing gif capture - finish it here. This may trigger due to user error or
            // due to IM_SUSPEND_TESTFUNC() terminating TestFunc() early.
            if (engine->CaptureContext.IsCapturingVideo())
            {
                ImGuiCaptureArgs* args = engine->CaptureCurrentArgs;
                ImGuiTestEngine_CaptureEndVideo(engine, args);
                //ImFileDelete(args->OutSavedFileName);
                ctx->LogWarning("Recovered from missing CaptureEndVideo()");
            }
        }
        else
        {
            // No test function
            if (test->Flags & ImGuiTestFlags_NoAutoFinish)
                while (!engine->Abort && test_output->Status == ImGuiTestStatus_Running)
                    ctx->Yield();
        }

        // Capture failure screenshot.
        if (ctx->IsError() && engine->IO.ConfigCaptureOnError)
        {
            // FIXME-VIEWPORT: Tested windows may be in their own viewport. This only captures everything in main viewport. Capture tool may be extended to capture viewport windows as well. This would leave out OS windows which may be a cause of failure.
            ImGuiCaptureArgs args;
            args.InFlags = ImGuiCaptureFlags_Instant;
            args.InCaptureRect.Min = ImGui::GetMainViewport()->Pos;
            args.InCaptureRect.Max = args.InCaptureRect.Min + ImGui::GetMainViewport()->Size;
            ImFormatString(args.InOutputFile, IM_ARRAYSIZE(args.InOutputFile), "output/failures/%s_%04d.png", ctx->Test->Name, ctx->ErrorCounter);
            if (ImGuiTestEngine_CaptureScreenshot(engine, &args))
                ctx->LogDebug("Saved '%s' (%d*%d pixels)", args.InOutputFile, (int)args.OutImageSize.x, (int)args.OutImageSize.y);
        }

        // Recover missing End*/Pop* calls.
        ImGuiTestEngine_ErrorRecoveryRun(engine);

        if (engine->IO.ConfigRunSpeed != ImGuiTestRunSpeed_Fast)
            ctx->SleepStandard();

        // Stop in GuiFunc mode
        if (engine->IO.ConfigKeepGuiFunc && ctx->IsError())
        {
            // Position mouse cursor
            ctx->UiContext->IO.WantSetMousePos = true;
            ctx->UiContext->IO.MousePos = engine->Inputs.MousePosValue;

            // Restore backend clipboard functions
            backup_ui_context.RestoreClipboardFuncs(*ctx->UiContext);

            // Unhide foreign windows (may be useful sometimes to inspect GuiFunc state... sometimes not)
            //ctx->ForeignWindowsUnhideAll();
        }

        // Keep GuiFunc spinning
        // FIXME-TESTS: after an error, this is not visible in the UI because status is not _Running anymore...
        if (engine->IO.ConfigKeepGuiFunc)
        {
            if (engine->TestsQueue.Size == 1 || test_output->Status == ImGuiTestStatus_Error)
            {
#if IMGUI_VERSION_NUM >= 18992
                ImGui::TeleportMousePos(engine->Inputs.MousePosValue);
#endif
                while (engine->IO.ConfigKeepGuiFunc && !engine->Abort)
                {
                    ctx->RunFlags |= ImGuiTestRunFlags_GuiFuncOnly;
                    ctx->Yield();
                }
            }
        }
    }

    IM_ASSERT(engine->CaptureCurrentArgs == nullptr && "Active capture was not terminated in the test code.");

    // Process and display result/status
    test_output->EndTime = ImTimeGetInMicroseconds();
    if (test_output->Status == ImGuiTestStatus_Running)
        test_output->Status = ImGuiTestStatus_Success;
    if (engine->Abort && test_output->Status != ImGuiTestStatus_Error)
        test_output->Status = ImGuiTestStatus_Unknown;

    // Log result
    if (test_output->Status == ImGuiTestStatus_Success)
    {
        if ((ctx->RunFlags & ImGuiTestRunFlags_NoSuccessMsg) == 0)
            ctx->LogInfo("Success.");
    }
    else if (engine->Abort)
        ctx->LogWarning("Aborted.");
    else if (test_output->Status == ImGuiTestStatus_Error)
        ctx->LogError("%s test failed.", test->Name);
    else
        ctx->LogWarning("Unknown status.");

    // Additional yields to avoid consecutive tests who may share identifiers from missing their window/item activation.
    ctx->RunFlags |= ImGuiTestRunFlags_GuiFuncDisable;
    ctx->Yield(3);

    // Restore active func
    ctx->ActiveFunc = backup_active_func;
    if (parent_ctx)
        parent_ctx->FrameCount = ctx->FrameCount;

    // Restore backed up IO and style
    backup_ui_context.Restore(*ctx->UiContext);

    if (run_flags & ImGuiTestRunFlags_ShareVars)
    {
        // Share generic vars?
        if ((run_flags & ImGuiTestRunFlags_ShareTestContext) == 0)
            parent_ctx->GenericVars = ctx->GenericVars;
    }
    else
    {
        // Destruct user vars
        if (test->VarsConstructor != nullptr)
        {
            test->VarsDestructor(ctx->UserVars);
            if (ctx->UserVars)
                IM_FREE(ctx->UserVars);
            ctx->UserVars = nullptr;
        }
        if (run_flags & ImGuiTestRunFlags_ShareTestContext)
        {
            parent_ctx->UserVars = backup_user_vars;
            parent_ctx->GenericVars = backup_generic_vars;
        }
    }

    // 'ctx' at this point is either a local variable or shared with parent.
    //ctx->Test = nullptr;
    //ctx->TestOutput = nullptr;
    //ctx->CaptureArgs = nullptr;

    IM_ASSERT(engine->TestContext == ctx);
    engine->TestContext = parent_ctx;
}

#if IMGUI_VERSION_NUM < 19123
static void LogAsWarningFunc(void* user_data, const char* fmt, ...)
{
    ImGuiTestContext* ctx = (ImGuiTestContext*)user_data;
    va_list args;
    va_start(args, fmt);
    ctx->LogExV(ImGuiTestVerboseLevel_Warning, ImGuiTestLogFlags_None, fmt, args);
    va_end(args);
}

static void LogAsDebugFunc(void* user_data, const char* fmt, ...)
{
    ImGuiTestContext* ctx = (ImGuiTestContext*)user_data;
    va_list args;
    va_start(args, fmt);
    ctx->LogExV(ImGuiTestVerboseLevel_Debug, ImGuiTestLogFlags_None, fmt, args);
    va_end(args);
}
#else
static void LogAsWarningFunc(ImGuiContext*, void* user_data, const char* msg)
{
    ImGuiContext& g = *GImGui;
    ImGuiTestContext* ctx = (ImGuiTestContext*)user_data;
    ImGuiWindow* window = g.CurrentWindow;
    ctx->LogEx(ImGuiTestVerboseLevel_Warning, ImGuiTestLogFlags_None, "In '%s': %s", window ? window->Name : "nullptr", msg);
}
static void LogAsDebugFunc(ImGuiContext*, void* user_data, const char* msg)
{
    ImGuiContext& g = *GImGui;
    ImGuiTestContext* ctx = (ImGuiTestContext*)user_data;
    ImGuiWindow* window = g.CurrentWindow;
    ctx->LogEx(ImGuiTestVerboseLevel_Debug, ImGuiTestLogFlags_None, "In '%s': %s", window ? window->Name : "nullptr", msg);
}
#endif

void ImGuiTestEngine_ErrorRecoverySetup(ImGuiTestEngine* engine)
{
    ImGuiTestContext* ctx = engine->TestContext;
    IM_ASSERT(ctx != nullptr);
    IM_ASSERT(ctx->Test != nullptr);
#if IMGUI_VERSION_NUM >= 19123
    if ((ctx->Test->Flags & ImGuiTestFlags_NoRecoveryWarnings) == 0)
    {
        ctx->UiContext->ErrorCallback = LogAsWarningFunc;
        ctx->UiContext->ErrorCallbackUserData = ctx;
    }
    else
    {
        ctx->UiContext->ErrorCallback = LogAsDebugFunc;
        ctx->UiContext->ErrorCallbackUserData = ctx;
    }
    ctx->UiContext->IO.ConfigErrorRecoveryEnableAssert = ((ctx->Test->Flags & ImGuiTestFlags_NoRecoveryWarnings) == 0 && ctx->TestOutput->Status != ImGuiTestStatus_Error);
#else
    IM_UNUSED(ctx);
#endif
}

void ImGuiTestEngine_ErrorRecoveryRun(ImGuiTestEngine* engine)
{
    ImGuiTestContext* ctx = engine->TestContext;
    IM_ASSERT(ctx != nullptr);
    IM_ASSERT(ctx->Test != nullptr);
    ImGuiTestEngine_ErrorRecoverySetup(engine);

#if IMGUI_VERSION_NUM < 19123
    // If we are _already_ in a test error state, recovering is normal so we'll hide the log.
    const bool verbose = (ctx->TestOutput->Status != ImGuiTestStatus_Error) || (engine->IO.ConfigVerboseLevel >= ImGuiTestVerboseLevel_Debug);
    if (verbose && (ctx->Test->Flags & ImGuiTestFlags_NoRecoveryWarnings) == 0)
        ImGui::ErrorCheckEndFrameRecover(LogAsWarningFunc, ctx);
    else
        ImGui::ErrorCheckEndFrameRecover(LogAsDebugFunc, ctx);
#else
    // This would automatically be done in EndFrame() but doing it here means we get a report earlier and in the right co-routine.
    // And the state we entered in happens to be the NewFrame() state (hence using g.StackSizesInNewFrame)
    ImGuiContext& g = *GImGui;
    ImGui::ErrorRecoveryTryToRecoverState(&g.StackSizesInNewFrame);
#endif
}

//-------------------------------------------------------------------------
// [SECTION] CRASH HANDLING
//-------------------------------------------------------------------------
// - ImGuiTestEngine_CrashHandler()
// - ImGuiTestEngine_InstallDefaultCrashHandler()
//-------------------------------------------------------------------------

void ImGuiTestEngine_CrashHandler()
{
    ImGuiContext& g = *GImGui;
    ImGuiTestEngine* engine = (ImGuiTestEngine*)g.TestEngine;
    ImGuiTest* crashed_test = (engine->TestContext && engine->TestContext->Test) ? engine->TestContext->Test : nullptr;

    ImOsConsoleSetTextColor(ImOsConsoleStream_StandardError, ImOsConsoleTextColor_BrightRed);
    if (crashed_test != nullptr)
        fprintf(stderr, "**ImGuiTestEngine_CrashHandler()** Crashed while running \"%s\" :(\n", crashed_test->Name);
    else
        fprintf(stderr, "**ImGuiTestEngine_CrashHandler()** Crashed :(\n");

    static bool handled = false;
    if (handled)
        return;
    handled = true;

    // Write stop times, because thread executing tests will no longer run.
    engine->BatchEndTime = ImTimeGetInMicroseconds();
    if (crashed_test && crashed_test->Output.Status == ImGuiTestStatus_Running)
    {
        crashed_test->Output.Status = ImGuiTestStatus_Error;
        crashed_test->Output.EndTime = engine->BatchEndTime;
    }

    // Export test run results.
    ImGuiTestEngine_Export(engine);
    ImGuiTestEngine_PrintResultSummary(engine);
}

#ifdef _WIN32
static LONG WINAPI ImGuiTestEngine_CrashHandlerWin32(LPEXCEPTION_POINTERS)
{
    ImGuiTestEngine_CrashHandler();
    return EXCEPTION_EXECUTE_HANDLER;
}
#else
static void ImGuiTestEngine_CrashHandlerUnix(int signal)
{
    IM_UNUSED(signal);
    ImGuiTestEngine_CrashHandler();
    abort();
}
#endif

void ImGuiTestEngine_InstallDefaultCrashHandler()
{
#ifdef _WIN32
    SetUnhandledExceptionFilter(&ImGuiTestEngine_CrashHandlerWin32);
#elif !IMGUI_TEST_ENGINE_IS_GAME_CONSOLE
    // Install a crash handler to relevant signals.
    struct sigaction action = {};
    action.sa_handler = ImGuiTestEngine_CrashHandlerUnix;
    action.sa_flags = SA_SIGINFO;
    sigaction(SIGILL, &action, nullptr);
    sigaction(SIGABRT, &action, nullptr);
    sigaction(SIGFPE, &action, nullptr);
    sigaction(SIGSEGV, &action, nullptr);
    sigaction(SIGPIPE, &action, nullptr);
    sigaction(SIGBUS, &action, nullptr);
#endif
}


//-------------------------------------------------------------------------
// [SECTION] HOOKS FOR CORE LIBRARY
//-------------------------------------------------------------------------
// - ImGuiTestEngineHook_ItemAdd()
// - ImGuiTestEngineHook_ItemAdd_GatherTask()
// - ImGuiTestEngineHook_ItemInfo()
// - ImGuiTestEngineHook_ItemInfo_ResolveFindByLabel()
// - ImGuiTestEngineHook_Log()
// - ImGuiTestEngineHook_AssertFunc()
//-------------------------------------------------------------------------

// This is rather slow at it runs on all items but only during a GatherItems() operations.
static void ImGuiTestEngineHook_ItemAdd_GatherTask(ImGuiContext* ui_ctx, ImGuiTestEngine* engine, ImGuiID id, const ImRect& bb, const ImGuiLastItemData* item_data)
{
    ImGuiContext& g = *ui_ctx;
    ImGuiWindow* window = g.CurrentWindow;
    ImGuiTestGatherTask* task = &engine->GatherTask;

    if ((task->InLayerMask & (1 << window->DC.NavLayerCurrent)) == 0)
        return;

    const ImGuiID parent_id = window->IDStack.Size ? window->IDStack.back() : 0;
    const ImGuiID gather_parent_id = task->InParentID;
    int result_depth = -1;
    if (gather_parent_id == parent_id)
    {
        result_depth = 0;
    }
    else
    {
        const int max_depth = task->InMaxDepth;

        // When using a 'PushID(label); Widget(""); PopID();` pattern flatten as 1 deep instead of 2 for simplicity.
        // We do this by offsetting our depth level.
        int curr_depth = (id == parent_id) ? -1 : 0;

        ImGuiWindow* curr_window = window;
        while (result_depth == -1 && curr_window != nullptr)
        {
            const int id_stack_size = curr_window->IDStack.Size;
            for (ImGuiID* p_id_stack = curr_window->IDStack.Data + id_stack_size - 1; p_id_stack >= curr_window->IDStack.Data; p_id_stack--, curr_depth++)
            {
                if (curr_depth >= max_depth)
                    break;
                if (*p_id_stack == gather_parent_id)
                {
                    result_depth = curr_depth;
                    break;
                }
            }

            // Recurse in child (could be policy/option in GatherTask)
            if (curr_window->Flags & ImGuiWindowFlags_ChildWindow)
                curr_window = curr_window->ParentWindow;
            else
                curr_window = nullptr;
        }
    }

    if (result_depth != -1)
    {
        ImGuiTestItemInfo* item = task->OutList->Pool.GetOrAddByKey(id); // Add
        item->TimestampMain = engine->FrameCount;
        item->ID = id;
        item->ParentID = parent_id;
        item->Window = window;
        item->RectFull = item->RectClipped = bb;
        item->RectClipped.ClipWithFull(window->ClipRect);      // This two step clipping is important, we want RectClipped to stays within RectFull
        item->RectClipped.ClipWithFull(item->RectFull);
        item->NavLayer = window->DC.NavLayerCurrent;
        item->Depth = result_depth;
#if IMGUI_VERSION_NUM >= 19135
        item->ItemFlags = item_data ? item_data->ItemFlags : ImGuiItemFlags_None;
#else
        item->ItemFlags = item_data ? item_data->InFlags : ImGuiItemFlags_None;
#endif
        item->StatusFlags = item_data ? item_data->StatusFlags : ImGuiItemStatusFlags_None;
        task->LastItemInfo = item;
    }
}

void ImGuiTestEngineHook_ItemAdd(ImGuiContext* ui_ctx, ImGuiID id, const ImRect& bb, const ImGuiLastItemData* item_data)
{
    ImGuiTestEngine* engine = (ImGuiTestEngine*)ui_ctx->TestEngine;
    engine->UiContextHasHooks = true;

    IM_ASSERT(id != 0);
    ImGuiContext& g = *ui_ctx;
    ImGuiWindow* window = g.CurrentWindow;

    // FIXME-OPT: Early out if there are no active Info/Gather tasks.

    // Info Tasks
    if (ImGuiTestInfoTask* task = ImGuiTestEngine_FindInfoTask(engine, id))
    {
        ImGuiTestItemInfo* item = &task->Result;
        item->TimestampMain = engine->FrameCount;
        item->ID = id;
        item->ParentID = window->IDStack.Size ? window->IDStack.back() : 0;
        item->Window = window;
        item->RectFull = item->RectClipped = bb;
        item->RectClipped.ClipWithFull(window->ClipRect);      // This two step clipping is important, we want RectClipped to stays within RectFull
        item->RectClipped.ClipWithFull(item->RectFull);
        item->NavLayer = window->DC.NavLayerCurrent;
        item->Depth = 0;
#if IMGUI_VERSION_NUM >= 19135
        item->ItemFlags = item_data ? item_data->ItemFlags : ImGuiItemFlags_None;
#else
        item->ItemFlags = item_data ? item_data->InFlags : ImGuiItemFlags_None;
#endif
        item->StatusFlags = item_data ? item_data->StatusFlags : ImGuiItemStatusFlags_None;
    }

    // Gather Task (only 1 can be active)
    if (engine->GatherTask.InParentID != 0)
        ImGuiTestEngineHook_ItemAdd_GatherTask(ui_ctx, engine, id, bb, item_data);
}

#if IMGUI_VERSION_NUM < 18934
void    ImGuiTestEngineHook_ItemAdd(ImGuiContext* ui_ctx, const ImRect& bb, ImGuiID id)
{
    ImGuiTestEngineHook_ItemAdd(ui_ctx, id, bb, nullptr);
}
#endif

// Task is submitted in TestFunc by ItemInfo() -> ItemInfoHandleWildcardSearch()
#ifdef IMGUI_HAS_IMSTR
static void ImGuiTestEngineHook_ItemInfo_ResolveFindByLabel(ImGuiContext* ui_ctx, ImGuiID id, const ImStrv label, ImGuiItemStatusFlags flags)
#else
static void ImGuiTestEngineHook_ItemInfo_ResolveFindByLabel(ImGuiContext* ui_ctx, ImGuiID id, const char* label, ImGuiItemStatusFlags flags)
#endif
{
    // At this point "label" is a match for the right-most name in user wildcard (e.g. the "bar" of "**/foo/bar"
    ImGuiContext& g = *ui_ctx;
    ImGuiTestEngine* engine = (ImGuiTestEngine*)ui_ctx->TestEngine;
    IM_UNUSED(label); // Match ABI of caller function (faster call)

    // Test for matching status flags
    ImGuiTestFindByLabelTask* label_task = &engine->FindByLabelTask;
    if (ImGuiItemStatusFlags filter_flags = label_task->InFilterItemStatusFlags)
        if (!(filter_flags & flags))
            return;

    // Test for matching PREFIX (the "window" of "window/**/foo/bar" or the "" of "/**/foo/bar")
    // FIXME-TESTS: Stack depth limit?
    // FIXME-TESTS: Recurse back into parent window limit?
    bool match_prefix = false;
    if (label_task->InPrefixId == 0)
    {
        match_prefix = true;
    }
    else
    {
        // Recurse back into parent, so from "WindowA" with SetRef("WindowA") it is possible to use "**/Button" to reach "WindowA/ChildXXXX/Button"
        for (ImGuiWindow* window = g.CurrentWindow; window != nullptr && !match_prefix; window = window->ParentWindow)
        {
            const int id_stack_size = window->IDStack.Size;
            for (ImGuiID* p_id_stack = window->IDStack.Data + id_stack_size - 1; p_id_stack >= window->IDStack.Data; p_id_stack--)
                if (*p_id_stack == label_task->InPrefixId)
                {
                    match_prefix = true;
                    break;
                }
        }
    }
    if (!match_prefix)
        return;

    // Test for full matching SUFFIX (the "foo/bar" or "window/**/foo/bar")
    // Because at this point we have only compared the prefix and the right-most label (the "window" and "bar" or "window/**/foo/bar")
    // FIXME-TESTS: The entire suffix must be inside the final window:
    // - In theory, someone could craft a suffix that contains sub-window, e.g. "SomeWindow/**/SomeChild_XXXX/SomeItem" and this will fail.
    // - Once we make child path easier to access we can fix that.
    if (label_task->InSuffixDepth > 1) // This is merely an early out: for Depth==1 the compare has already been done in ImGuiTestEngineHook_ItemInfo()
    {
        ImGuiWindow* window = g.CurrentWindow;
        const int id_stack_size = window->IDStack.Size;
        int id_stack_pos = id_stack_size - label_task->InSuffixDepth;

        // At this point, IN MOST CASES (BUT NOT ALL) this should be the case:
        //    ImHashStr(label, 0, g.CurrentWindow->IDStack.back()) == id
        // It's not always the case as we have situations where we call IMGUI_TEST_ENGINE_ITEM_INFO() outside of the right stack location:
        //    e.g. Begin(), or items using the PushID(label); SubItem(""); PopID(); idiom.
        // If you are curious or need to understand this more in depth, uncomment this assert to detect them:
        //    ImGuiID tmp_id = ImHashStr(label, 0, g.CurrentWindow->IDStack.back());
        //    IM_ASSERT(tmp_id == id);
        // The "Try with parent" case is designed to handle that. May need further tuning.

        ImGuiID base_id = id_stack_pos >= 0 ? window->IDStack.Data[id_stack_pos] : 0;   // base_id correspond to the "**"
        ImGuiID find_id = ImHashDecoratedPath(label_task->InSuffix, nullptr, base_id);     // hash the whole suffix e.g. "foo/bar" over our base
        if (id != find_id)
        {
            // Try with parent
            base_id = id_stack_pos > 0 ? window->IDStack.Data[id_stack_pos - 1] : 0;
            find_id = ImHashDecoratedPath(label_task->InSuffix, nullptr, base_id);
            if (id != find_id)
                return;
        }
    }

    // Success
    label_task->OutItemId = id;
}

// label is optional
#ifdef IMGUI_HAS_IMSTR
void ImGuiTestEngineHook_ItemInfo(ImGuiContext* ui_ctx, ImGuiID id, ImStrv label, ImGuiItemStatusFlags flags)
#else
void ImGuiTestEngineHook_ItemInfo(ImGuiContext* ui_ctx, ImGuiID id, const char* label, ImGuiItemStatusFlags flags)
#endif
{
    ImGuiTestEngine* engine = (ImGuiTestEngine*)ui_ctx->TestEngine;

    IM_ASSERT(id != 0);
    ImGuiContext& g = *ui_ctx;
    //ImGuiWindow* window = g.CurrentWindow;
    //IM_ASSERT(window->DC.LastItemId == id || window->DC.LastItemId == 0); // Need _ItemAdd() to be submitted before _ItemInfo()

    // Update Info Task status flags
    if (ImGuiTestInfoTask* task = ImGuiTestEngine_FindInfoTask(engine, id))
    {
        ImGuiTestItemInfo* item = &task->Result;
        item->TimestampStatus = g.FrameCount;
        item->StatusFlags = flags;
        if (label)
            ImStrncpy(item->DebugLabel, label, IM_ARRAYSIZE(item->DebugLabel));
    }

    // Update Gather Task status flags
    if (engine->GatherTask.LastItemInfo && engine->GatherTask.LastItemInfo->ID == id)
    {
        ImGuiTestItemInfo* item = engine->GatherTask.LastItemInfo;
        item->TimestampStatus = g.FrameCount;
        item->StatusFlags = flags;
        if (label)
            ImStrncpy(item->DebugLabel, label, IM_ARRAYSIZE(item->DebugLabel));
    }

    // Update Find by Label Task
    // FIXME-TESTS FIXME-OPT: Compare by hashes instead of strcmp to support "###" operator.
    // Perhaps we could use strcmp() if we detect that ### is not used, that would be faster.
    ImGuiTestFindByLabelTask* label_task = &engine->FindByLabelTask;
    if (label && label_task->InSuffixLastItem && label_task->OutItemId == 0)
#ifdef IMGUI_HAS_IMSTR
        if (label_task->InSuffixLastItemHash == ImHashStr(label))
#else
        if (label_task->InSuffixLastItemHash == ImHashStr(label, 0))
#endif
            ImGuiTestEngineHook_ItemInfo_ResolveFindByLabel(ui_ctx, id, label, flags);
}

// Forward core/user-land text to test log
// This is called via the user-land IMGUI_TEST_ENGINE_LOG() macro.
void ImGuiTestEngineHook_Log(ImGuiContext* ui_ctx, const char* fmt, ...)
{
    ImGuiTestEngine* engine = (ImGuiTestEngine*)ui_ctx->TestEngine;
    if (engine == nullptr || engine->TestContext == nullptr)
        return;

    va_list args;
    va_start(args, fmt);
    engine->TestContext->LogExV(ImGuiTestVerboseLevel_Debug, ImGuiTestLogFlags_None, fmt, args);
    va_end(args);
}

// Helper to output extra information (e.g. current test) during an assert.
// Your custom assert code may optionally want to call this.
void ImGuiTestEngine_AssertLog(const char* expr, const char* file, const char* function, int line)
{
    if (ImGuiTestEngine* engine = GImGuiTestEngine)
        if (ImGuiTestContext* ctx = engine->TestContext)
        {
            ctx->LogError("Assert: '%s'", expr);
            ctx->LogWarning("In %s:%d, function %s()", file, line, function);
            if (ImGuiTest* test = ctx->Test)
                ctx->LogWarning("While running test: %s %s", test->Category, test->Name);
        }
}

// Used by IM_CHECK_OP() macros
ImGuiTextBuffer* ImGuiTestEngine_GetTempStringBuilder()
{
    static ImGuiTextBuffer builder;
    builder.Buf.resize(1);
    builder.Buf[0] = 0;
    return &builder;
}

// Out of convenience for main library we allow this to be called before TestEngine is initialized.
const char* ImGuiTestEngine_FindItemDebugLabel(ImGuiContext* ui_ctx, ImGuiID id)
{
    if (ui_ctx->TestEngine == nullptr || id == 0)
        return nullptr;
    if (ImGuiTestItemInfo* id_info = ImGuiTestEngine_FindItemInfo((ImGuiTestEngine*)ui_ctx->TestEngine, id, ""))
        return id_info->DebugLabel;
    return nullptr;
}

//-------------------------------------------------------------------------
// [SECTION] CHECK/ERROR FUNCTIONS FOR TESTS
//-------------------------------------------------------------------------
// - ImGuiTestEngine_Check()
// - ImGuiTestEngine_Error()
//-------------------------------------------------------------------------

// Return true to request a debugger break
bool ImGuiTestEngine_Check(const char* file, const char* func, int line, ImGuiTestCheckFlags flags, bool result, const char* expr)
{
    ImGuiTestEngine* engine = GImGuiTestEngine;
    (void)func;

    // Removed absolute path from output so we have deterministic output (otherwise __FILE__ gives us machine dending output)
    const char* file_without_path = file ? ImPathFindFilename(file) : "";

    if (ImGuiTestContext* ctx = engine->TestContext)
    {
        ImGuiTest* test = ctx->Test;
        //ctx->LogDebug("IM_CHECK(%s)", expr);
        if (!result)
        {
            if (!(ctx->RunFlags & ImGuiTestRunFlags_GuiFuncOnly))
                test->Output.Status = ImGuiTestStatus_Error;

            if (file)
                ctx->LogError("Error %s:%d '%s'", file_without_path, line, expr);
            else
                ctx->LogError("Error '%s'", expr);
            ctx->ErrorCounter++;
        }
        else if (!(flags & ImGuiTestCheckFlags_SilentSuccess))
        {
            if (file)
                ctx->LogInfo("OK %s:%d '%s'", file_without_path, line, expr);
            else
                ctx->LogInfo("OK '%s'", expr);
        }
    }
    else
    {
        IM_ASSERT(0 && "No active tests!");
    }

    if (result == false && engine->IO.ConfigStopOnError && !engine->Abort)
        engine->Abort = true; //ImGuiTestEngine_Abort(engine);
    if (result == false && engine->IO.ConfigBreakOnError && !engine->Abort)
        return true;

    return false;
}

bool ImGuiTestEngine_CheckStrOp(const char* file, const char* func, int line, ImGuiTestCheckFlags flags, const char* op, const char* lhs_var, const char* lhs_value, const char* rhs_var, const char* rhs_value, bool* out_res)
{
    int res_strcmp = strcmp(lhs_value, rhs_value);
    bool res = 0;
    if (strcmp(op, "==") == 0)
        res = (res_strcmp == 0);
    else if (strcmp(op, "!=") == 0)
        res = (res_strcmp != 0);
    else
        IM_ASSERT(0);
    *out_res = res;

    ImGuiTextBuffer buf; // FIXME-OPT: Now we can probably remove that allocation

    bool lhs_is_literal = lhs_var[0] == '\"';
    bool rhs_is_literal = rhs_var[0] == '\"';
    if (strchr(lhs_value, '\n') != nullptr || strchr(rhs_value, '\n') != nullptr)
    {
        // Multi line strings
        size_t lhs_value_len = strlen(lhs_value);
        size_t rhs_value_len = strlen(rhs_value);
        if (lhs_value_len > 0 && lhs_value[lhs_value_len - 1] == '\n') // Strip trailing carriage return as we are adding one ourselves
            lhs_value_len--;
        if (rhs_value_len > 0 && rhs_value[rhs_value_len - 1] == '\n')
            rhs_value_len--;
        buf.appendf(
            "\n"
            "---------------------------------------- // lhs: %s\n"
            "%.*s\n"
            "---------------------------------------- // rhs: %s, compare op: %s\n"
            "%.*s\n"
            "----------------------------------------\n",
            lhs_is_literal ? "literal" : lhs_var,
            (int)lhs_value_len, lhs_value,
            rhs_is_literal ? "literal" : rhs_var,
            op,
            (int)rhs_value_len, rhs_value);
    }
    else
    {
        // Single line strings
        buf.appendf(
            "%s [\"%s\"] %s %s [\"%s\"]",
            lhs_is_literal ? "" : lhs_var, lhs_value,
            op,
            rhs_is_literal ? "" : rhs_var, rhs_value);
    }


    return ImGuiTestEngine_Check(file, func, line, flags, res, buf.c_str());
}

bool ImGuiTestEngine_Error(const char* file, const char* func, int line, ImGuiTestCheckFlags flags, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    Str256 buf;
    buf.setfv(fmt, args);
    bool ret = ImGuiTestEngine_Check(file, func, line, flags, false, buf.c_str());
    va_end(args);

    ImGuiTestEngine* engine = GImGuiTestEngine;
    if (engine && engine->Abort)
        return false;
    return ret;
}

//-------------------------------------------------------------------------
// [SECTION] SETTINGS
//-------------------------------------------------------------------------
// FIXME: In our wildest dreams we could provide a imgui_club/ serialization helper that would be
// easy to use in both the ReadLine and WriteAll functions.
//-------------------------------------------------------------------------

static void*    ImGuiTestEngine_SettingsReadOpen(ImGuiContext*, ImGuiSettingsHandler*, const char* name)
{
    if (strcmp(name, "Data") != 0)
        return nullptr;
    return (void*)1;
}

static bool     SettingsTryReadString(const char* line, const char* prefix, char* out_buf, size_t out_buf_size)
{
    // Could also use scanf() with "%[^\n]" but it won't bound check.
    size_t prefix_len = strlen(prefix);
    if (strncmp(line, prefix, prefix_len) != 0)
        return false;
    line += prefix_len;
    IM_ASSERT(out_buf_size >= strlen(line) + 1);
    ImFormatString(out_buf, out_buf_size, "%s", line);
    return true;
}

static bool     SettingsTryReadString(const char* line, const char* prefix, Str* out_str)
{
    // Could also use scanf() with "%[^\n]" but it won't bound check.
    size_t prefix_len = strlen(prefix);
    if (strncmp(line, prefix, prefix_len) != 0)
        return false;
    line += prefix_len;
    out_str->set(line);
    return true;
}

static void     ImGuiTestEngine_SettingsReadLine(ImGuiContext* ui_ctx, ImGuiSettingsHandler*, void* entry, const char* line)
{
    ImGuiTestEngine* e = (ImGuiTestEngine*)ui_ctx->TestEngine;
    IM_ASSERT(e != nullptr);
    IM_ASSERT(e->UiContextTarget == ui_ctx);
    IM_UNUSED(entry);

    int n = 0;
    /**/ if (SettingsTryReadString(line, "FilterTests=", e->UiFilterTests))                                                         { }
    else if (SettingsTryReadString(line, "FilterPerfs=", e->UiFilterPerfs))                                                         { }
    else if (sscanf(line, "LogHeight=%f", &e->UiLogHeight) == 1)                                                                    { }
    else if (sscanf(line, "CaptureTool=%d", &n) == 1)                                                                               { e->UiCaptureToolOpen = (n != 0); }
    else if (sscanf(line, "PerfTool=%d", &n) == 1)                                                                                  { e->UiPerfToolOpen = (n != 0); }
    else if (sscanf(line, "StackTool=%d", &n) == 1)                                                                                 { e->UiStackToolOpen = (n != 0); }
    else if (sscanf(line, "CaptureEnabled=%d", &n) == 1)                                                                            { e->IO.ConfigCaptureEnabled = (n != 0); }
    else if (sscanf(line, "CaptureOnError=%d", &n) == 1)                                                                            { e->IO.ConfigCaptureOnError = (n != 0); }
    else if (SettingsTryReadString(line, "VideoCapturePathToEncoder=", e->IO.VideoCaptureEncoderPath, IM_ARRAYSIZE(e->IO.VideoCaptureEncoderPath))) { }
    else if (SettingsTryReadString(line, "VideoCaptureParamsToEncoder=", e->IO.VideoCaptureEncoderParams, IM_ARRAYSIZE(e->IO.VideoCaptureEncoderParams))) { }
    else if (SettingsTryReadString(line, "GifCaptureParamsToEncoder=", e->IO.GifCaptureEncoderParams, IM_ARRAYSIZE(e->IO.GifCaptureEncoderParams))) { }
    else if (SettingsTryReadString(line, "VideoCaptureExtension=", e->IO.VideoCaptureExtension, IM_ARRAYSIZE(e->IO.VideoCaptureExtension))) { }
}

static void     ImGuiTestEngine_SettingsWriteAll(ImGuiContext* ui_ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buf)
{
    ImGuiTestEngine* engine = (ImGuiTestEngine*)ui_ctx->TestEngine;
    IM_ASSERT(engine != nullptr);
    IM_ASSERT(engine->UiContextTarget == ui_ctx);

    buf->appendf("[%s][Data]\n", handler->TypeName);
    buf->appendf("FilterTests=%s\n", engine->UiFilterTests->c_str());
    buf->appendf("FilterPerfs=%s\n", engine->UiFilterPerfs->c_str());
    buf->appendf("LogHeight=%.0f\n", engine->UiLogHeight);
    buf->appendf("CaptureTool=%d\n", engine->UiCaptureToolOpen);
    buf->appendf("PerfTool=%d\n", engine->UiPerfToolOpen);
    buf->appendf("StackTool=%d\n", engine->UiStackToolOpen);
    buf->appendf("CaptureEnabled=%d\n", engine->IO.ConfigCaptureEnabled);
    buf->appendf("CaptureOnError=%d\n", engine->IO.ConfigCaptureOnError);
    buf->appendf("VideoCapturePathToEncoder=%s\n", engine->IO.VideoCaptureEncoderPath);
    buf->appendf("VideoCaptureParamsToEncoder=%s\n", engine->IO.VideoCaptureEncoderParams);
    buf->appendf("GifCaptureParamsToEncoder=%s\n", engine->IO.GifCaptureEncoderParams);
    buf->appendf("VideoCaptureExtension=%s\n", engine->IO.VideoCaptureExtension);
    buf->appendf("\n");
}

//-------------------------------------------------------------------------
// [SECTION] ImGuiTestLog
//-------------------------------------------------------------------------

void ImGuiTestLog::Clear()
{
    Buffer.clear();
    LineInfo.clear();
    memset(&CountPerLevel, 0, sizeof(CountPerLevel));
}

// Output:
// - If 'buffer != nullptr': all extracted lines are appended to 'buffer'. Use 'buffer->c_str()' on your side to obtain the text.
// - Return value: number of lines extracted (should be equivalent to number of '\n' inside buffer->c_str()).
// - You may call the function with buffer == nullptr to only obtain a count without getting the data.
// Verbose levels are inclusive:
// - To get ONLY Error:                     Use level_min == ImGuiTestVerboseLevel_Error, level_max = ImGuiTestVerboseLevel_Error
// - To get ONLY Error and Warnings:        Use level_min == ImGuiTestVerboseLevel_Error, level_max = ImGuiTestVerboseLevel_Warning
// - To get All Errors, Warnings, Debug...  Use level_min == ImGuiTestVerboseLevel_Error, level_max = ImGuiTestVerboseLevel_Trace
int ImGuiTestLog::ExtractLinesForVerboseLevels(ImGuiTestVerboseLevel level_min, ImGuiTestVerboseLevel level_max, ImGuiTextBuffer* out_buffer)
{
    IM_ASSERT(level_min <= level_max);

    // Return count
    int count = 0;
    if (out_buffer == nullptr)
    {
        for (int n = level_min; n <= level_max; n++)
            count += CountPerLevel[n];
        return count;
    }

    // Extract lines and return count
    for (auto& line_info : LineInfo)
        if (line_info.Level >= level_min && line_info.Level <= level_max)
        {
            const char* line_begin = Buffer.c_str() + line_info.LineOffset;
            const char* line_end = strchr(line_begin, '\n');
            out_buffer->append(line_begin, line_end[0] == '\n' ? line_end + 1 : line_end);
            count++;
        }
    return count;
}

void ImGuiTestLog::UpdateLineOffsets(ImGuiTestEngineIO* engine_io, ImGuiTestVerboseLevel level, const char* start)
{
    IM_UNUSED(engine_io);
    IM_ASSERT(Buffer.begin() <= start && start < Buffer.end());
    const char* p_begin = start;
    const char* p_end = Buffer.end();
    const char* p = p_begin;
    while (p < p_end)
    {
        const char* p_bol = p;
        const char* p_eol = strchr(p, '\n');

        bool last_empty_line = (p_bol + 1 == p_end);
        if (!last_empty_line)
        {
            int offset = (int)(p_bol - Buffer.c_str());
            LineInfo.push_back({level, offset});
            CountPerLevel[level] += 1;
        }
        p = p_eol ? p_eol + 1 : nullptr;
    }
}

//-------------------------------------------------------------------------
// [SECTION] ImGuiTest
//-------------------------------------------------------------------------

ImGuiTest::~ImGuiTest()
{
    if (NameOwned)
        ImGui::MemFree((char*)Name);
}

void ImGuiTest::SetOwnedName(const char* name)
{
    IM_ASSERT(!NameOwned);
    NameOwned = true;
    Name = ImStrdup(name);
}

//-------------------------------------------------------------------------
