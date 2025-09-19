// dear imgui
// (context when a running test + end user automation API)
// This is the main (if not only) interface that your Tests will be using.

// This file is governed by the "Dear ImGui Test Engine License".
// Details of the license are provided in the LICENSE.txt file in the same directory.

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_te_context.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_te_engine.h"
#include "imgui_te_internal.h"
#include "imgui_te_perftool.h"
#include "imgui_te_utils.h"
#include "thirdparty/Str/Str.h"

//-------------------------------------------------------------------------
// [SECTION] ImGuiTestRefDesc
//-------------------------------------------------------------------------

ImGuiTestRefDesc::ImGuiTestRefDesc(const ImGuiTestRef& ref)
{
    if (ref.Path && ref.ID != 0)
        ImFormatString(Buf, IM_ARRAYSIZE(Buf), "'%s' (id 0x%08X)", ref.Path, ref.ID);
    else if (ref.Path)
        ImFormatString(Buf, IM_ARRAYSIZE(Buf), "'%s'", ref.Path);
    else
        ImFormatString(Buf, IM_ARRAYSIZE(Buf), "0x%08X", ref.ID);
}

ImGuiTestRefDesc::ImGuiTestRefDesc(const ImGuiTestRef& ref, const ImGuiTestItemInfo& item)
{
    if (ref.Path && item.ID != 0)
        ImFormatString(Buf, IM_ARRAYSIZE(Buf), "'%s' (id 0x%08X)", ref.Path, item.ID);
    else if (ref.Path)
        ImFormatString(Buf, IM_ARRAYSIZE(Buf), "'%s'", ref.Path);
    else
        ImFormatString(Buf, IM_ARRAYSIZE(Buf), "0x%08X (label \"%s\")", ref.ID, item.DebugLabel);
}

//-------------------------------------------------------------------------
// [SECTION] ImGuiTestContextDepthScope
//-------------------------------------------------------------------------

// Helper to increment/decrement the function depth (so our log entry can be padded accordingly)
#define IM_TOKENCONCAT_INTERNAL(x, y)                   x ## y
#define IM_TOKENCONCAT(x, y)                            IM_TOKENCONCAT_INTERNAL(x, y)
#define IMGUI_TEST_CONTEXT_REGISTER_DEPTH(_THIS)        ImGuiTestContextDepthScope IM_TOKENCONCAT(depth_register, __LINE__)(_THIS)

struct ImGuiTestContextDepthScope
{
    ImGuiTestContext* TestContext;
    ImGuiTestContextDepthScope(ImGuiTestContext* ctx) { TestContext = ctx; TestContext->ActionDepth++; }
    ~ImGuiTestContextDepthScope() { TestContext->ActionDepth--; }
};

//-------------------------------------------------------------------------
// [SECTION] Enum names helpers
//-------------------------------------------------------------------------

inline const char* GetActionName(ImGuiTestAction action)
{
    switch (action)
    {
    case ImGuiTestAction_Unknown:       return "Unknown";
    case ImGuiTestAction_Hover:         return "Hover";
    case ImGuiTestAction_Click:         return "Click";
    case ImGuiTestAction_DoubleClick:   return "DoubleClick";
    case ImGuiTestAction_Check:         return "Check";
    case ImGuiTestAction_Uncheck:       return "Uncheck";
    case ImGuiTestAction_Open:          return "Open";
    case ImGuiTestAction_Close:         return "Close";
    case ImGuiTestAction_Input:         return "Input";
    case ImGuiTestAction_NavActivate:   return "NavActivate";
    case ImGuiTestAction_COUNT:
    default:                            return "N/A";
    }
}

inline const char* GetActionVerb(ImGuiTestAction action)
{
    switch (action)
    {
    case ImGuiTestAction_Unknown:       return "Unknown";
    case ImGuiTestAction_Hover:         return "Hovered";
    case ImGuiTestAction_Click:         return "Clicked";
    case ImGuiTestAction_DoubleClick:   return "DoubleClicked";
    case ImGuiTestAction_Check:         return "Checked";
    case ImGuiTestAction_Uncheck:       return "Unchecked";
    case ImGuiTestAction_Open:          return "Opened";
    case ImGuiTestAction_Close:         return "Closed";
    case ImGuiTestAction_Input:         return "Input";
    case ImGuiTestAction_NavActivate:   return "NavActivated";
    case ImGuiTestAction_COUNT:
    default:                            return "N/A";
    }
}


//-------------------------------------------------------------------------
// [SECTION] ImGuiTestContext
// This is the interface that most tests will interact with.
//-------------------------------------------------------------------------

void    ImGuiTestContext::LogEx(ImGuiTestVerboseLevel level, ImGuiTestLogFlags flags, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    LogExV(level, flags, fmt, args);
    va_end(args);
}

void    ImGuiTestContext::LogExV(ImGuiTestVerboseLevel level, ImGuiTestLogFlags flags, const char* fmt, va_list args)
{
    ImGuiTestContext* ctx = this;
    //ImGuiTest* test = ctx->Test;

    IM_ASSERT(level > ImGuiTestVerboseLevel_Silent && level < ImGuiTestVerboseLevel_COUNT);

    if (level == ImGuiTestVerboseLevel_Debug && ctx->ActionDepth > 1)
        level = ImGuiTestVerboseLevel_Trace;

    // Log all messages that we may want to print in future.
    if (EngineIO->ConfigVerboseLevelOnError < level)
        return;

    ImGuiTestLog* log = &ctx->TestOutput->Log;
    const int prev_size = log->Buffer.size();

    //const char verbose_level_char = ImGuiTestEngine_GetVerboseLevelName(level)[0];
    //if (flags & ImGuiTestLogFlags_NoHeader)
    //    log->Buffer.appendf("[%c] ", verbose_level_char);
    //else
    //    log->Buffer.appendf("[%c] [%04d] ", verbose_level_char, ctx->FrameCount);
    if ((flags & ImGuiTestLogFlags_NoHeader) == 0)
        log->Buffer.appendf("[%04d] ", ctx->FrameCount);

    if (level >= ImGuiTestVerboseLevel_Debug)
        log->Buffer.appendf("-- %*s", ImMax(0, (ctx->ActionDepth - 1) * 2), "");
    log->Buffer.appendfv(fmt, args);
    log->Buffer.append("\n");

    log->UpdateLineOffsets(EngineIO, level, log->Buffer.begin() + prev_size);
    LogToTTY(level, log->Buffer.c_str() + prev_size);
    LogToDebugger(level, log->Buffer.c_str() + prev_size);
}

void    ImGuiTestContext::LogDebug(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    LogExV(ImGuiTestVerboseLevel_Debug, ImGuiTestLogFlags_None, fmt, args);
    va_end(args);
}

void ImGuiTestContext::LogInfo(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    LogExV(ImGuiTestVerboseLevel_Info, ImGuiTestLogFlags_None, fmt, args);
    va_end(args);
}

void ImGuiTestContext::LogWarning(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    LogExV(ImGuiTestVerboseLevel_Warning, ImGuiTestLogFlags_None, fmt, args);
    va_end(args);
}

void ImGuiTestContext::LogError(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    LogExV(ImGuiTestVerboseLevel_Error, ImGuiTestLogFlags_None, fmt, args);
    va_end(args);
}

void    ImGuiTestContext::LogToTTY(ImGuiTestVerboseLevel level, const char* message, const char* message_end)
{
    IM_ASSERT(level > ImGuiTestVerboseLevel_Silent && level < ImGuiTestVerboseLevel_COUNT);

    if (!EngineIO->ConfigLogToTTY)
        return;

    ImGuiTestContext* ctx = this;
    ImGuiTestOutput* test_output = ctx->TestOutput;
    ImGuiTestLog* log = &test_output->Log;

    if (test_output->Status == ImGuiTestStatus_Error)
    {
        // Current test failed.
        if (!CachedLinesPrintedToTTY)
        {
            // Print all previous logged messages first
            // FIXME: Can't use ExtractLinesAboveVerboseLevel() because we want to keep error level...
            CachedLinesPrintedToTTY = true;
            for (int i = 0; i < log->LineInfo.Size; i++)
            {
                ImGuiTestLogLineInfo& line_info = log->LineInfo[i];
                if (line_info.Level > EngineIO->ConfigVerboseLevelOnError)
                    continue;
                char* line_begin = log->Buffer.Buf.Data + line_info.LineOffset;
                char* line_end = strchr(line_begin, '\n');
                LogToTTY(line_info.Level, line_begin, line_end + 1);
            }
            // We already printed current line as well, so return now.
            return;
        }
        // Otherwise print only current message. If we are executing here log level already is within range of
        // ConfigVerboseLevelOnError setting.
    }
    else if (EngineIO->ConfigVerboseLevel < level)
    {
        // Skip printing messages of lower level than configured.
        return;
    }

    switch (level)
    {
    case ImGuiTestVerboseLevel_Warning:
        ImOsConsoleSetTextColor(ImOsConsoleStream_StandardOutput, ImOsConsoleTextColor_BrightYellow);
        break;
    case ImGuiTestVerboseLevel_Error:
        ImOsConsoleSetTextColor(ImOsConsoleStream_StandardOutput, ImOsConsoleTextColor_BrightRed);
        break;
    default:
        ImOsConsoleSetTextColor(ImOsConsoleStream_StandardOutput, ImOsConsoleTextColor_White);
        break;
    }
    if (message_end)
        fprintf(stdout, "%.*s", (int)(message_end - message), message);
    else
        fprintf(stdout, "%s", message);
    ImOsConsoleSetTextColor(ImOsConsoleStream_StandardOutput, ImOsConsoleTextColor_White);
    fflush(stdout);
}

void        ImGuiTestContext::LogToDebugger(ImGuiTestVerboseLevel level, const char* message)
{
    IM_ASSERT(level > ImGuiTestVerboseLevel_Silent && level < ImGuiTestVerboseLevel_COUNT);

    if (!EngineIO->ConfigLogToDebugger)
        return;

    if (EngineIO->ConfigVerboseLevel < level)
        return;

    switch (level)
    {
    default:
        break;
    case ImGuiTestVerboseLevel_Error:
        ImOsOutputDebugString("[error] ");
        break;
    case ImGuiTestVerboseLevel_Warning:
        ImOsOutputDebugString("[warn.] ");
        break;
    case ImGuiTestVerboseLevel_Info:
        ImOsOutputDebugString("[info ] ");
        break;
    case ImGuiTestVerboseLevel_Debug:
        ImOsOutputDebugString("[debug] ");
        break;
    case ImGuiTestVerboseLevel_Trace:
        ImOsOutputDebugString("[trace] ");
        break;
    }

    ImOsOutputDebugString(message);
}

void    ImGuiTestContext::LogBasicUiState()
{
    ImGuiID item_hovered_id = UiContext->HoveredIdPreviousFrame;
    ImGuiID item_active_id = UiContext->ActiveId;
    ImGuiTestItemInfo* item_hovered_info = item_hovered_id ? ImGuiTestEngine_FindItemInfo(Engine, item_hovered_id, "") : nullptr;
    ImGuiTestItemInfo* item_active_info = item_active_id ? ImGuiTestEngine_FindItemInfo(Engine, item_active_id, "") : nullptr;
    LogDebug("Hovered: 0x%08X (\"%s\"), Active:  0x%08X(\"%s\")",
        item_hovered_id, item_hovered_info->ID != 0 ? item_hovered_info->DebugLabel : "",
        item_active_id, item_active_info->ID != 0 ? item_active_info->DebugLabel : "");
}

void    ImGuiTestContext::LogItemList(ImGuiTestItemList* items)
{
    for (const ImGuiTestItemInfo& info : *items)
        LogDebug("- 0x%08X: depth %d: '%s' in window '%s'\n", info.ID, info.Depth, info.DebugLabel, info.Window->Name);
}

void    ImGuiTestContext::Finish(ImGuiTestStatus status)
{
    if (ActiveFunc == ImGuiTestActiveFunc_GuiFunc)
    {
        IM_ASSERT(status == ImGuiTestStatus_Success || status == ImGuiTestStatus_Unknown);
        if (RunFlags & ImGuiTestRunFlags_GuiFuncOnly)
            return;
        if (TestOutput->Status == ImGuiTestStatus_Running)
            TestOutput->Status = status;
    }
    else if (ActiveFunc == ImGuiTestActiveFunc_TestFunc)
    {
        IM_ASSERT(status == ImGuiTestStatus_Unknown); // To set Success from a TestFunc() you can 'return' from it.
        if (TestOutput->Status == ImGuiTestStatus_Running)
            TestOutput->Status = status;
    }
}

void    ImGuiTestContext::Yield(int count)
{
    IM_ASSERT(count > 0);
    while (count > 0)
    {
        ImGuiTestEngine_Yield(Engine);
        count--;
    }
}

// Supported values for ImGuiTestRunFlags:
// - ImGuiTestRunFlags_NoError: if child test fails, return false and do not mark parent test as failed.
// - ImGuiTestRunFlags_ShareVars: share generic vars and custom vars between child and parent tests.
// - ImGuiTestRunFlags_ShareTestContext
ImGuiTestStatus ImGuiTestContext::RunChildTest(const char* child_test_name, ImGuiTestRunFlags run_flags)
{
    if (IsError())
        return ImGuiTestStatus_Error;

    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    LogDebug("RunChildTest %s", child_test_name);

    ImGuiTest* child_test = ImGuiTestEngine_FindTestByName(Engine, nullptr, child_test_name);
    IM_CHECK_SILENT_RETV(child_test != nullptr, ImGuiTestStatus_Error);
    IM_CHECK_SILENT_RETV(child_test != Test, ImGuiTestStatus_Error); // Can't recursively run same test.

    ImGuiTestStatus parent_status = TestOutput->Status;
    TestOutput->Status = ImGuiTestStatus_Running;
    ImGuiTestEngine_RunTest(Engine, this, child_test, run_flags);
    ImGuiTestStatus child_status = TestOutput->Status;

    // Restore parent status
    TestOutput->Status = parent_status;
    if (child_status == ImGuiTestStatus_Error && (run_flags & ImGuiTestRunFlags_NoError) == 0)
        TestOutput->Status = ImGuiTestStatus_Error;

    // Return child status
    LogDebug("(returning to parent test)");
    return child_status;
}

// Return true to request aborting TestFunc
// Called via IM_SUSPEND_TESTFUNC()
bool    ImGuiTestContext::SuspendTestFunc(const char* file, int line)
{
    if (IsError())
        return false;

    if (file != nullptr)
    {
        file = ImPathFindFilename(file);
        LogError("SuspendTestFunc() at %s:%d", file, line);
    }
    else
    {
        LogError("SuspendTestFunc()");
    }

    // Save relevant state.
    // FIXME-TESTS: Saving/restoring window z-order could be desirable.
    ImVec2 mouse_pos = Inputs->MousePosValue;
    ImGuiTestRunFlags run_flags = RunFlags;
#if IMGUI_VERSION_NUM >= 18992
    ImGui::TeleportMousePos(mouse_pos);
#endif

    RunFlags |= ImGuiTestRunFlags_GuiFuncOnly;
    TestOutput->Status = ImGuiTestStatus_Suspended;
    while (TestOutput->Status == ImGuiTestStatus_Suspended && !Abort)
        Yield();
    TestOutput->Status = ImGuiTestStatus_Running;

    // Restore relevant state.
    RunFlags = run_flags;
    Inputs->MousePosValue = mouse_pos;

    // Terminate TestFunc on abort, continue otherwise.
    return Abort;
}

// Sleep a given amount of time (unless running in Fast mode: there it will Yield once)
void    ImGuiTestContext::Sleep(float time)
{
    if (IsError())
        return;

    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    if (EngineIO->ConfigRunSpeed == ImGuiTestRunSpeed_Fast)
    {
        LogEx(ImGuiTestVerboseLevel_Trace, ImGuiTestLogFlags_None, "Sleep(%.2f) -> Yield() in fast mode", time);
        //ImGuiTestEngine_AddExtraTime(Engine, time); // We could add time, for now we have no use for it...
        ImGuiTestEngine_Yield(Engine);
    }
    else
    {
        LogEx(ImGuiTestVerboseLevel_Trace, ImGuiTestLogFlags_None, "Sleep(%.2f)", time);
        while (time > 0.0f && !Abort)
        {
            ImGuiTestEngine_Yield(Engine);
            time -= UiContext->IO.DeltaTime;
        }
    }
}

// This is useful when you need to wait a certain amount of time (even in Fast mode)
// Sleep for a given clock time from the point of view of the Dear ImGui context, without affecting wall clock time of the running application.
// FIXME: This makes sense for apps only relying on io.DeltaTime.
void    ImGuiTestContext::SleepNoSkip(float time, float framestep_in_second)
{
    if (IsError())
        return;

    LogDebug("SleepNoSkip %f seconds (in %f increments)", time, framestep_in_second);
    while (time > 0.0f && !Abort)
    {
        ImGuiTestEngine_SetDeltaTime(Engine, framestep_in_second);
        ImGuiTestEngine_Yield(Engine);
        time -= UiContext->IO.DeltaTime;
    }
}

void    ImGuiTestContext::SleepShort()
{
    if (EngineIO->ConfigRunSpeed != ImGuiTestRunSpeed_Fast)
        Sleep(EngineIO->ActionDelayShort);
}

void    ImGuiTestContext::SleepStandard()
{
    if (EngineIO->ConfigRunSpeed != ImGuiTestRunSpeed_Fast)
        Sleep(EngineIO->ActionDelayStandard);
}

void ImGuiTestContext::SetInputMode(ImGuiInputSource input_mode)
{
    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    LogDebug("SetInputMode %d", input_mode);

    IM_ASSERT(input_mode == ImGuiInputSource_Mouse || input_mode == ImGuiInputSource_Keyboard || input_mode == ImGuiInputSource_Gamepad);
    InputMode = input_mode;

    if (InputMode == ImGuiInputSource_Keyboard || InputMode == ImGuiInputSource_Gamepad)
    {
#if IMGUI_VERSION_NUM >= 19136
        ImGui::SetNavCursorVisible(true);
        UiContext->NavHighlightItemUnderNav = true;
#else
        UiContext->NavDisableHighlight = false;
        UiContext->NavDisableMouseHover = true;
#endif
    }
    else
    {
#if IMGUI_VERSION_NUM >= 19136
        ImGui::SetNavCursorVisible(false);
        UiContext->NavHighlightItemUnderNav = false;
#else
        UiContext->NavDisableHighlight = true;
        UiContext->NavDisableMouseHover = false;
#endif
    }
}

// Shortcut for when we have a window pointer, avoid mistakes with slashes in child names.
void ImGuiTestContext::SetRef(ImGuiWindow* window)
{
    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    IM_CHECK_SILENT(window != nullptr);
    LogDebug("SetRef '%s' 0x%08X", window->Name, window->ID);

    // We grab the ID directly and avoid ImHashDecoratedPath so "/" in window names are not ignored.
    size_t len = strlen(window->Name);
    IM_ASSERT(len < IM_ARRAYSIZE(RefStr) - 1);
    strcpy(RefStr, window->Name);
    RefID = RefWindowID = window->ID;

    MouseSetViewport(window);

    // Automatically uncollapse by default
    if (!(OpFlags & ImGuiTestOpFlags_NoAutoUncollapse))
        WindowCollapse(window->ID, false);
}

// It is exceptionally OK to call SetRef() in GuiFunc, as a facility to call seeded ctx->GetId() in GuiFunc.
// FIXME-TESTS: May be good to focus window when docked? Otherwise locate request won't even see an item?
void ImGuiTestContext::SetRef(ImGuiTestRef ref)
{
    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    if (ActiveFunc == ImGuiTestActiveFunc_TestFunc)
        LogDebug("SetRef '%s' 0x%08X", ref.Path ? ref.Path : "nullptr", ref.ID);

    if (ref.Path)
    {
        size_t len = strlen(ref.Path);
        IM_ASSERT(len < IM_ARRAYSIZE(RefStr) - 1);

        strcpy(RefStr, ref.Path);
        RefID = GetID(ref.Path, ImGuiTestRef());
    }
    else
    {
        RefStr[0] = 0;
        RefID = ref.ID;
    }
    RefWindowID = 0;

    // Early out
    if (ref.IsEmpty())
        return;

    // Try to infer window
    // (This is in order to set viewport, uncollapse window, and store its base id for leading "/" operator)

    // (0) Windows is fully specified in path?
    ImGuiWindow* window = GetWindowByRef("");

    // (1) Try first element of ref path, it is most likely a window name and item lookup won't be necessary.
    if (window == nullptr && ref.Path != nullptr)
    {
        // "Window/SomeItem" -> search for "Window"
        const char* name_begin = ref.Path;
        while (*name_begin == '/') // Skip leading slashes
            name_begin++;
        const char* name_end = name_begin - 1;
        do
        {
            name_end = strchr(name_end + 1, '/');
        } while (name_end != nullptr && name_end > name_begin && name_end[-1] == '\\');
        window = GetWindowByRef(ImHashDecoratedPath(name_begin, name_end));
    }

    if (ActiveFunc == ImGuiTestActiveFunc_GuiFunc)
        return;

    // (2) Ref was specified as an ID and points to an item therefore item lookup is unavoidable.
    // FIXME: Maybe display something in log when that happens?
    if (window == nullptr)
    {
        ImGuiTestItemInfo item_info = ItemInfo(RefID, ImGuiTestOpFlags_NoError);
        if (item_info.ID != 0)
            window = item_info.Window;
    }

    // Set viewport and base ID for single "/" operator.
    if (window)
    {
        RefWindowID = window->ID;
        MouseSetViewport(window);
    }

    // Automatically uncollapse by default
    if (window && !(OpFlags & ImGuiTestOpFlags_NoAutoUncollapse))
        WindowCollapse(window->ID, false);
}

ImGuiTestRef ImGuiTestContext::GetRef()
{
    return RefID;
}

ImGuiWindow* ImGuiTestContext::GetWindowByRef(ImGuiTestRef ref)
{
    ImGuiID window_id = GetID(ref);
    ImGuiWindow* window = ImGui::FindWindowByID(window_id);
    return window;
}

ImGuiID ImGuiTestContext::GetID(ImGuiTestRef ref)
{
    if (ref.ID)
        return ref.ID;

    return GetID(ref, RefID);
}

// Refer to Wiki to read details
// https://github.com/ocornut/imgui_test_engine/wiki/Named-References
// - Meaning of leading "//" ................. "//rootnode" : ignore SetRef
// - Meaning of leading "//$FOCUSED" ......... "//$FOCUSED/node" : "node" in currently focused window
// - Meaning of leading "/" .................. "/node" : move to root of window pointed by SetRef() when SetRef() uses a path
// - Meaning of $$xxxx literal encoding ...... "list/$$1" : hash of "list" + hash if (int)1, equivalent of PushID("hello"); PushID(1);
//// - Meaning of leading "../" .............. "../node" : move back 1 level from SetRef path() when SetRef() uses a path // Unimplemented
// FIXME: "//$FOCUSED/.." is currently not usable.
ImGuiID ImGuiTestContext::GetID(ImGuiTestRef ref, ImGuiTestRef seed_ref)
{
    ImGuiContext& g = *UiContext;

    if (ref.ID)
        return ref.ID; // FIXME: What if seed_ref != 0

    // Handle special $FOCUSED variable.
    // (Note that we don't and can't really support a "$HOVERED" equivalent for the hovered window.
    //  Why? Because it is extremely fragile to use: with late translation of variable held in string,
    //  it is extremely common that the "expected" hovered window at the time of passing the string has
    //  changed in later uses of the same reference.)
    // You can however easily use:
    //   SetRef(g.HoveredWindow->ID);
    const char* FOCUSED_PREFIX = "//$FOCUSED";
    const size_t FOCUSED_PREFIX_LEN = 10;

    const char* path = ref.Path ? ref.Path : "";
    if (strncmp(path, FOCUSED_PREFIX, FOCUSED_PREFIX_LEN) == 0)
        if (path[FOCUSED_PREFIX_LEN] == '/' || path[FOCUSED_PREFIX_LEN] == 0)
        {
            path += FOCUSED_PREFIX_LEN;
            if (path[0] == '/')
                path++;
            if (g.NavWindow)
                seed_ref = g.NavWindow->ID;
            else
                LogError("\"//$FOCUSED\" was used with no focused window!");
        }

    if (path[0] == '/')
    {
        path++;
        if (path[0] == '/')
        {
            // "//" : Double-slash prefix resets ID seed to 0.
            seed_ref = ImGuiTestRef();
        }
        else
        {
            // "/" : Single-slash prefix sets seed to the "current window", which a parent window containing an item with RefID id.
            if (ActiveFunc == ImGuiTestActiveFunc_GuiFunc)
                seed_ref = ImGuiTestRef(g.CurrentWindow->ID);
            else
                seed_ref = RefWindowID;
        }
    }

    return ImHashDecoratedPath(path, nullptr, seed_ref.Path ? GetID(seed_ref) : seed_ref.ID);
}

#ifndef IMGUI_DISABLE_OBSOLETE_FUNCTIONS
ImGuiID ImGuiTestContext::GetIDByInt(int n)
{
    return ImHashData(&n, sizeof(n), GetID(RefID));
}

ImGuiID ImGuiTestContext::GetIDByInt(int n, ImGuiTestRef seed_ref)
{
    return ImHashData(&n, sizeof(n), GetID(seed_ref));
}

ImGuiID ImGuiTestContext::GetIDByPtr(void* p)
{
    return ImHashData(&p, sizeof(p), GetID(RefID));
}

ImGuiID ImGuiTestContext::GetIDByPtr(void* p, ImGuiTestRef seed_ref)
{
    return ImHashData(&p, sizeof(p), GetID(seed_ref));
}
#endif

ImVec2 ImGuiTestContext::GetMainMonitorWorkPos()
{
#ifdef IMGUI_HAS_VIEWPORT
    if (UiContext->IO.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        const ImGuiPlatformMonitor* monitor = ImGui::GetViewportPlatformMonitor(ImGui::GetMainViewport());
        return monitor->WorkPos;
    }
#endif
    return ImGui::GetMainViewport()->WorkPos;
}

ImVec2 ImGuiTestContext::GetMainMonitorWorkSize()
{
#ifdef IMGUI_HAS_VIEWPORT
    if (UiContext->IO.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        const ImGuiPlatformMonitor* monitor = ImGui::GetViewportPlatformMonitor(ImGui::GetMainViewport());
        return monitor->WorkSize;
    }
#endif
    return ImGui::GetMainViewport()->WorkSize;
}

#if IMGUI_TEST_ENGINE_ENABLE_CAPTURE
static bool ImGuiTestContext_CanCaptureScreenshot(ImGuiTestContext* ctx)
{
    ImGuiTestEngineIO* io = ctx->EngineIO;
    return io->ConfigCaptureEnabled;
}

static bool ImGuiTestContext_CanCaptureVideo(ImGuiTestContext* ctx)
{
    ImGuiTestEngineIO* io = ctx->EngineIO;
    return io->ConfigCaptureEnabled && ImFileExist(io->VideoCaptureEncoderPath);
}
#endif

bool ImGuiTestContext::CaptureAddWindow(ImGuiTestRef ref)
{
    ImGuiWindow* window = GetWindowByRef(ref);
    IM_CHECK_SILENT_RETV(window != nullptr, false);
    CaptureArgs->InCaptureWindows.push_back(window);
    return true;
}

static void CaptureInitAutoFilename(ImGuiTestContext* ctx, const char* ext)
{
    IM_ASSERT(ext != nullptr && ext[0] == '.');

    if (ctx->CaptureArgs->InOutputFile[0] == 0)
        ctx->CaptureSetExtension(ext); // Reset extension of specified filename or auto-generate a new filename.
}

bool ImGuiTestContext::CaptureScreenshot(int capture_flags)
{
    if (IsError())
        return false;

    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    LogInfo("CaptureScreenshot()");
    ImGuiCaptureArgs* args = CaptureArgs;
    args->InFlags = capture_flags;

    // Auto filename
    CaptureInitAutoFilename(this, ".png");

#if IMGUI_TEST_ENGINE_ENABLE_CAPTURE
    // Way capture tool is implemented doesn't prevent ClampWindowPos() from running,
    // so we disable that feature at the moment. (imgui_test_engine/#33)
    ImGuiIO& io = ImGui::GetIO();
    bool backup_io_config_move_window_from_title_bar_only = io.ConfigWindowsMoveFromTitleBarOnly;
    if (capture_flags & ImGuiCaptureFlags_StitchAll)
        io.ConfigWindowsMoveFromTitleBarOnly = false;

    bool can_capture = ImGuiTestContext_CanCaptureScreenshot(this);
    if (!can_capture)
        args->InFlags |= ImGuiCaptureFlags_NoSave;

    bool ret = ImGuiTestEngine_CaptureScreenshot(Engine, args);
    if (can_capture)
        LogInfo("Saved '%s' (%d*%d pixels)", args->InOutputFile, (int)args->OutImageSize.x, (int)args->OutImageSize.y);
    else
        LogWarning("Skipped saving '%s' (%d*%d pixels) (enable in 'Misc->Options')", args->InOutputFile, (int)args->OutImageSize.x, (int)args->OutImageSize.y);

    if (capture_flags & ImGuiCaptureFlags_StitchAll)
        io.ConfigWindowsMoveFromTitleBarOnly = backup_io_config_move_window_from_title_bar_only;

    return ret;
#else
    IM_UNUSED(args);
    LogWarning("Skipped screenshot capture: disabled by IMGUI_TEST_ENGINE_ENABLE_CAPTURE=0.");
    return false;
#endif
}

void ImGuiTestContext::CaptureReset()
{
    *CaptureArgs = ImGuiCaptureArgs();
}

// FIXME-TESTS: Add ImGuiCaptureFlags_NoHideOtherWindows
void ImGuiTestContext::CaptureScreenshotWindow(ImGuiTestRef ref, int capture_flags)
{
    CaptureReset();
    CaptureAddWindow(ref);
    CaptureScreenshot(capture_flags);
}

void ImGuiTestContext::CaptureSetExtension(const char* ext)
{
    IM_ASSERT(ext && ext[0] == '.');
    ImGuiCaptureArgs* args = CaptureArgs;
    if (args->InOutputFile[0] == 0)
    {
        ImFormatString(args->InOutputFile, IM_ARRAYSIZE(args->InOutputFile), "output/captures/%s_%04d%s", Test->Name, CaptureCounter, ext);
        CaptureCounter++;
    }
    else
    {
        char* filename_ext = (char*)ImPathFindExtension(args->InOutputFile);
        ImStrncpy(filename_ext, ext, (size_t)(filename_ext - args->InOutputFile));
    }
}

bool ImGuiTestContext::CaptureBeginVideo()
{
    if (IsError())
        return false;

    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    LogInfo("CaptureBeginVideo()");
    ImGuiCaptureArgs* args = CaptureArgs;

    // Auto filename
    CaptureInitAutoFilename(this, EngineIO->VideoCaptureExtension);

#if IMGUI_TEST_ENGINE_ENABLE_CAPTURE
    bool can_capture = ImGuiTestContext_CanCaptureVideo(this);
    if (!can_capture)
        args->InFlags |= ImGuiCaptureFlags_NoSave;
    return ImGuiTestEngine_CaptureBeginVideo(Engine, args);
#else
    IM_UNUSED(args);
    LogWarning("Skipped recording GIF: disabled by IMGUI_TEST_ENGINE_ENABLE_CAPTURE=0.");
    return false;
#endif
}

bool ImGuiTestContext::CaptureEndVideo()
{
    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    LogInfo("CaptureEndVideo()");
    ImGuiCaptureArgs* args = CaptureArgs;

    bool ret = Engine->CaptureContext.IsCapturingVideo() && ImGuiTestEngine_CaptureEndVideo(Engine, args);
    if (!ret)
        return false;

#if IMGUI_TEST_ENGINE_ENABLE_CAPTURE
    // In-progress capture was canceled by user. Delete incomplete file.
    if (IsError())
    {
        //ImFileDelete(args->OutSavedFileName);
        return false;
    }
    bool can_capture = ImGuiTestContext_CanCaptureVideo(this);
    if (can_capture)
    {
        LogInfo("Saved '%s' (%d*%d pixels)", args->InOutputFile, (int)args->OutImageSize.x, (int)args->OutImageSize.y);
    }
    else
    {
        if (!EngineIO->ConfigCaptureEnabled)
            LogWarning("Skipped saving '%s' video because: io.ConfigCaptureEnabled == false (enable in Misc->Options)", args->InOutputFile);
        else
            LogWarning("Skipped saving '%s' video because: Video Encoder not found.", args->InOutputFile);
    }
#endif

    return ret;
}

// Handle wildcard search on the TestFunc side.
// Results will be resolved on the Gui side via the following call-chain:
//   IMGUI_TEST_ENGINE_ITEM_INFO() -> ImGuiTestEngineHook_ItemInfo() -> ImGuiTestEngineHook_ItemInfo_ResolveFindByLabel()
ImGuiID ImGuiTestContext::ItemInfoHandleWildcardSearch(const char* wildcard_prefix_start, const char* wildcard_prefix_end, const char* wildcard_suffix_start)
{
    LogDebug("Wildcard matching..");

    // Wildcard matching
    // Note that task->InPrefixId may be 0 as well (= we don't know the window)
    ImGuiTestFindByLabelTask* task = &Engine->FindByLabelTask;
    if (wildcard_prefix_start < wildcard_prefix_end)
        task->InPrefixId = ImHashDecoratedPath(wildcard_prefix_start, wildcard_prefix_end, RefID);
    else
        task->InPrefixId = RefID;
    task->OutItemId = 0;

    // Advance pointer to point it to the last label
    task->InSuffix = task->InSuffixLastItem = wildcard_suffix_start;
    for (const char* c = task->InSuffix; *c; c++)
        if (*c == '/')
            task->InSuffixLastItem = c + 1;
    task->InSuffixLastItemHash = ImHashStr(task->InSuffixLastItem, 0, 0);

    // Count number of labels
    task->InSuffixDepth = 1;
    for (const char* c = wildcard_suffix_start; *c; c++)
        if (*c == '/')
            task->InSuffixDepth++;

    int retries = 0;
    while (retries < 2 && task->OutItemId == 0)
    {
        ImGuiTestEngine_Yield(Engine);
        retries++;
    }

    // Wildcard matching requires item to be visible, because clipped items are unaware of their labels. Try panning through entire window, searching for target item.
    // (Scrollbar position restoration in theory may be desirable, however it interferes with typical use of found item)
    // FIXME-TESTS: This doesn't recurse properly into each child..
    // FIXME: Down the line if we refactor ItemAdd() return value to distinguish render-clipping vs logic-clipping etc, we should instead temporarily enable a "no clip"
    // mode without the need for scrolling.
    if (task->OutItemId == 0)
    {
        ImGuiTestItemInfo base_item = ItemInfo(task->InPrefixId, ImGuiTestOpFlags_NoError);
        ImGuiWindow* window = (base_item.ID != 0) ? base_item.Window : GetWindowByRef(task->InPrefixId);
        if (window)
        {
            ImVec2 rect_size = window->InnerRect.GetSize();
            for (float scroll_x = 0.0f; task->OutItemId == 0; scroll_x += rect_size.x)
            {
                for (float scroll_y = 0.0f; task->OutItemId == 0; scroll_y += rect_size.y)
                {
                    window->Scroll.x = scroll_x;
                    window->Scroll.y = scroll_y;

                    retries = 0;
                    while (retries < 2 && task->OutItemId == 0)
                    {
                        ImGuiTestEngine_Yield(Engine);
                        retries++;
                    }
                    if (window->Scroll.y >= window->ScrollMax.y)
                        break;
                }
                if (window->Scroll.x >= window->ScrollMax.x)
                    break;
            }
        }
    }
    ImGuiID full_id = task->OutItemId;

    // FIXME: InFilterItemStatusFlags is intentionally not cleared here, because it is set in ItemAction() and reused in later calls to ItemInfo() to resolve ambiguities.
    task->InPrefixId = 0;
    task->InSuffix = task->InSuffixLastItem = nullptr;
    task->InSuffixLastItemHash = 0;
    task->InSuffixDepth = 0;
    task->OutItemId = 0;    // -V1048   // Variable 'OutItemId' was assigned the same value. False-positive, because value of OutItemId could be modified from other thread during ImGuiTestEngine_Yield() call.

    return full_id;
}

static void ItemInfoErrorLog(ImGuiTestContext* ctx, ImGuiTestRef ref, ImGuiID full_id, ImGuiTestOpFlags flags)
{
    if (flags & ImGuiTestOpFlags_NoError)
        return;

    if (ctx->Engine->UiContextHasHooks == false)
        IM_ERRORF_NOHDR("%s", "IMGUI DOES NOT SEEM COMPILED WITH '#define IMGUI_ENABLE_TEST_ENGINE'!\nMAKE SURE THAT BOTH 'imgui' AND 'imgui_test_engine' ARE USING THE SAME 'imconfig' FILE.");

    // Prefixing the string with / ignore the reference/current ID
    Str256 msg;
    if (ref.Path && ref.Path[0] == '/' && ctx->RefStr[0] != 0)
        msg.setf("Unable to locate item: '%s' (0x%08X)", ref.Path, full_id);
    else if (ref.Path && full_id != 0)
        msg.setf("Unable to locate item: '%s/%s' (0x%08X)", ctx->RefStr, ref.Path, full_id);
    else if (ref.Path)
        msg.setf("Unable to locate item: '%s/%s' (0x%08X)", ctx->RefStr, ref.Path, full_id);
    else
        msg.setf("Unable to locate item: 0x%08X", ref.ID);

    //if (flags & ImGuiTestOpFlags_NoError)
    //    ctx->LogInfo("Ignored: %s", msg.c_str()); // FIXME
    //else
    IM_ERRORF_NOHDR("%s", msg.c_str());
}

// Supported values for ImGuiTestOpFlags:
// - ImGuiTestOpFlags_NoError
ImGuiTestItemInfo ImGuiTestContext::ItemInfo(ImGuiTestRef ref, ImGuiTestOpFlags flags)
{
    if (IsError())
        return ItemInfoNull();

    const ImGuiTestOpFlags SUPPORTED_FLAGS = ImGuiTestOpFlags_NoError;
    IM_ASSERT((flags & ~SUPPORTED_FLAGS) == 0);

    ImGuiID full_id = 0;

    if (const char* p = ref.Path ? strstr(ref.Path, "**/") : nullptr)
    {
        // Wildcard matching
        // FIXME-TESTS: Need to verify that this is not inhibited by a \, so \**/ should not pass, but \\**/ should :)
        // We could add a simple helpers that would iterate the strings, handling inhibitors, and let you check if a given characters is inhibited or not.
        const char* wildcard_prefix_start = ref.Path;
        const char* wildcard_prefix_end = p;
        const char* wildcard_suffix_start = wildcard_prefix_end + 3;
        full_id = ItemInfoHandleWildcardSearch(wildcard_prefix_start, wildcard_prefix_end, wildcard_suffix_start);
    }
    else
    {
        // Regular matching
        full_id = GetID(ref);
    }

    // If ui_ctx->TestEngineHooksEnabled is not already on (first ItemInfo() task in a while) we'll probably need an extra frame to warmup
    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    ImGuiTestItemInfo* item = nullptr;
    int retries = 0;
    int max_retries = 2;
    int extra_retries_for_appearing = 0;
    while (full_id && retries < max_retries)
    {
        item = ImGuiTestEngine_FindItemInfo(Engine, full_id, ref.Path);

        // While a window is appearing it is likely to be resizing and items moving. Wait an extra frame for things to settle. (FIXME: Could use another source e.g. Hidden? AutoFitFramesX?)
        if (item && item->Window && item->Window->Appearing && extra_retries_for_appearing == 0)
        {
            item = nullptr;
            max_retries++;
            extra_retries_for_appearing++;
        }

        if (item)
            return *item;
        ImGuiTestEngine_Yield(Engine);
        retries++;
    }

    ItemInfoErrorLog(this, ref, full_id, flags);

    return ItemInfoNull();
}

// Supported values for ImGuiTestOpFlags:
// - ImGuiTestOpFlags_NoError
ImGuiTestItemInfo ImGuiTestContext::ItemInfoOpenFullPath(ImGuiTestRef ref, ImGuiTestOpFlags flags)
{
    // First query
    bool can_open_full_path = (ref.Path != nullptr);
    ImGuiTestItemInfo item = ItemInfo(ref, (can_open_full_path ? ImGuiTestOpFlags_NoError : ImGuiTestOpFlags_None) | (flags & ImGuiTestOpFlags_NoError));
    if (item.ID != 0)
        return item;
    if (!can_open_full_path)
        return ItemInfoNull();

    // Tries to auto open intermediaries leading to final path.
    // Note that openables cannot be part of the **/ (else it means we would have to open everything).
    // - Openables can be before the wildcard    "Node2/Node3/**/Button"
    // - Openables can be after the wildcard     "**/Node2/Node3/Lv4/Button"
    int opened_parents = 0;
    for (const char* parent_end = strstr(ref.Path, "/"); parent_end != nullptr; parent_end = strstr(parent_end + 1, "/"))
    {
        // Skip "**/* sections
        if (strncmp(ref.Path, "**/", parent_end - ref.Path) == 0)
            continue;

        Str128 parent_id;
        parent_id.set(ref.Path, parent_end);
        ImGuiTestItemInfo parent_item = ItemInfo(parent_id.c_str(), ImGuiTestOpFlags_NoError);
        if (parent_item.ID != 0)
        {
#ifdef IMGUI_HAS_DOCK
            ImGuiWindow* parent_window = parent_item.Window;
#endif
            if ((parent_item.StatusFlags & ImGuiItemStatusFlags_Openable) != 0 && (parent_item.StatusFlags & ImGuiItemStatusFlags_Opened) == 0)
            {
                // Open intermediary item
                if ((parent_item.ItemFlags & ImGuiItemFlags_Disabled) == 0) // FIXME: Report disabled state in log?
                {
                    ItemAction(ImGuiTestAction_Open, parent_item.ID, ImGuiTestOpFlags_NoAutoOpenFullPath);
                    opened_parents++;
                }
            }
#ifdef IMGUI_HAS_DOCK
            else if (parent_window->ID == parent_item.ID && parent_window->DockIsActive && parent_window->DockTabIsVisible == false)
            {
                // Make tab visible
                ItemClick(parent_item.ID);
                opened_parents++;
            }
#endif
        }
    }
    if (opened_parents > 0)
        item = ItemInfo(ref, (flags & ImGuiTestOpFlags_NoError));

    if (item.ID == 0)
        ItemInfoErrorLog(this, ref, 0, flags);

    return item;
}

// Find a window given a path or an ID.
// In the case of when a path is passed, this handle finding child windows as well.
// e.g.
//   ctx->WindowInfo("//Test Window");                          // OK
//   ctx->WindowInfo("//Test Window/Child/SubChild");           // OK
//   ctx->WindowInfo("//$FOCUSED/Child");                       // OK
//   ctx->SetRef("Test Window); ctx->WindowInfo("Child");       // OK
//   ctx->WindowInfo(GetID("//Test Window"));                   // OK (find by raw ID without a path)
//   ctx->WindowInfo(GetID("//Test Window/Child/SubChild));     // *INCORRECT* GetID() doesn't unmangle child names.
//   ctx->WindowInfo("//Test Window/Button");                   // *INCORRECT* Only finds windows, not items.
// Return:
// - Return pointer is always valid.
// - Valid fields are:
//   - item->ID     : window ID      (may be == 0, if the window doesn't exist)
//   - item->Window : window pointer (may be == nullptr, if the window doesn't exist)
//   - Other fields correspond to the title-bar/tab item of a window, so likely not what you want (same as using IsItemXXX after Begin)
//   - If you want other fields simply get them via the window-> pointer.
// - Likely you may want to feed the return value into SetRef(): e.g. 'ctx->SetRef(item->ID)' or 'ctx->SetRef(WindowInfo("//Window/Child")->ID);'
// Supported values for ImGuiTestOpFlags:
// - ImGuiTestOpFlags_NoError
// Todos:
// - FIXME: Missing support for wildcards.
ImGuiTestItemInfo ImGuiTestContext::WindowInfo(ImGuiTestRef ref, ImGuiTestOpFlags flags)
{
    if (IsError())
        return ItemInfoNull();

    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);

    // Query by ID (not very useful but supported)
    if (ref.ID != 0)
    {
        LogDebug("WindowInfo: by id: 0x%08X", ref.ID);
        IM_ASSERT(ref.Path == nullptr);
        ImGuiWindow* window = GetWindowByRef(ref);
        if (window == nullptr)
        {
            if ((flags & ImGuiTestOpFlags_NoError) == 0)
                LogError("WindowInfo: cannot find window by ID!"); // FIXME: What if we want to query a not-yet-existing window by ID?
            return ItemInfoNull();
        }
        return ItemInfo(window->ID, flags & ImGuiTestOpFlags_NoError);
    }

    // Query by Path: this is where the meat of our work is.
    LogDebug("WindowInfo: by path: '%s'", ref.Path ? ref.Path : "nullptr");
    ImGuiWindow* window = nullptr;
    ImGuiID window_idstack_back = 0;
    const char* current = ref.Path;
    while (*current || window == nullptr)
    {
        // Handle SetRef(), if any (this will also handle "//$FOCUSED" syntax)
        Str128 part_name;
        if (window == nullptr && RefID != 0 && strncmp(ref.Path, "//", 2) != 0)
        {
            window = GetWindowByRef("");
            window_idstack_back = window ? window->ID : 0;
        }
        else
        {
            // Find next part of the path + create a zero-terminated copy for convenience
            const char* part_start = current;
            const char* part_end = ImFindNextDecoratedPartInPath(current);
            if (part_end == nullptr)
            {
                current = part_end = part_start + strlen(part_start);
            }
            else if (part_end > part_start)
            {
                current = part_end;
                part_end--;
                IM_ASSERT(part_end[0] == '/');
            }
            part_name.setf("%.*s", (int)(part_end - part_start), part_start);

            // Find root window or child window
            if (window == nullptr)
            {
                // Root: defer first element to GetID(), this will handle SetRef(), "//" and "//$FOCUSED" syntax.
                ImGuiID window_id = GetID(part_name.c_str());
                window = GetWindowByRef(window_id);
                window_idstack_back = window ? window->ID : 0;
            }
            else
            {
                ImGuiID child_window_id = 0;
                ImGuiWindow* child_window = nullptr;
                {
                    // Child: Attempt 1: Try to BeginChild(const char*) variant and mimic its logic.
                    Str128 child_window_full_name;
#if (IMGUI_VERSION_NUM >= 18996) && (IMGUI_VERSION_NUM < 18999)
                    if (window_idstack_back == window->ID)
                    {
                        child_window_full_name.setf("%s/%s", window->Name, part_name.c_str());
                    }
                    else
#endif
                    {
                        ImGuiID child_item_id = GetID(part_name.c_str(), window_idstack_back);
                        child_window_full_name.setf("%s/%s_%08X", window->Name, part_name.c_str(), child_item_id);
                    }
                    child_window_id = ImHashStr(child_window_full_name.c_str()); // We do NOT use ImHashDecoratedPath()
                    child_window = GetWindowByRef(child_window_id);
                }
                if (child_window == nullptr)
                {
                    // Child: Attempt 2: Try for BeginChild(ImGuiID id) variant and mimic its logic.
                    // FIXME: This only really works when ID passed to BeginChild() was derived from a string.
                    // We could support $$xxxx syntax to encode ID in parameter?
                    ImGuiID child_item_id = GetID(part_name.c_str(), window_idstack_back);
                    Str128f child_window_full_name("%s/%08X", window->Name, child_item_id);
                    child_window_id = ImHashStr(child_window_full_name.c_str()); // We do NOT use ImHashDecoratedPath()
                    child_window = GetWindowByRef(child_window_id);
                }
                if (child_window == nullptr)
                {
                    // Assume that part is an arbitrary PushID(const char*)
                    window_idstack_back = GetID(part_name.c_str(), window_idstack_back);
                }
                else
                {
                    window = child_window;
                    window_idstack_back = window ? window->ID : 0;
                }
            }
        }

        // Process result
        // FIXME: What if we want to query a not-yet-existing window by ID?
        if (window == nullptr)
        {
            if ((flags & ImGuiTestOpFlags_NoError) == 0)
                LogError("WindowInfo: element \"%s\" doesn't seem to exist.", part_name.c_str());
            return ItemInfoNull();
        }
    }

    IM_ASSERT(window != nullptr);
    IM_ASSERT(window_idstack_back != 0);

    // Stopped on "window/node/"
    if (window_idstack_back != 0 && window_idstack_back != window->ID)
    {
        if ((flags & ImGuiTestOpFlags_NoError) == 0)
            LogError("WindowInfo: element doesn't seem to exist or isn't a window.");
        return ItemInfoNull();
    }

    return ItemInfo(window->ID, flags & ImGuiTestOpFlags_NoError);
}

void    ImGuiTestContext::ScrollToTop(ImGuiTestRef ref)
{
    if (IsError())
        return;

    ImGuiWindow* window = GetWindowByRef(ref);
    IM_CHECK_SILENT(window != nullptr);
    if (window->Scroll.y == 0.0f)
        return;
    ScrollToY(ref, 0.0f);
    Yield();
}

void    ImGuiTestContext::ScrollToBottom(ImGuiTestRef ref)
{
    if (IsError())
        return;

    ImGuiWindow* window = GetWindowByRef(ref);
    IM_CHECK_SILENT(window != nullptr);
    if (window->Scroll.y == window->ScrollMax.y)
        return;
    ScrollToY(ref, window->ScrollMax.y);
    Yield();
}

bool    ImGuiTestContext::ScrollErrorCheck(ImGuiAxis axis, float expected, float actual, int* remaining_attempts)
{
    if (IsError())
    {
        (*remaining_attempts)--;
        return false;
    }

    float THRESHOLD = 1.0f;
    if (ImFabs(actual - expected) < THRESHOLD)
        return true;

    (*remaining_attempts)--;
    if (*remaining_attempts > 0)
    {
        LogInfo("Failed to set Scroll%c. Requested %.2f, got %.2f. Will try again.", 'X' + axis, expected, actual);
        return true;
    }
    else
    {
        IM_ERRORF("Failed to set Scroll%c. Requested %.2f, got %.2f. Aborting.", 'X' + axis, expected, actual);
        return false;
    }
}

// FIXME-TESTS: Mostly the same code as ScrollbarEx()
static ImVec2 GetWindowScrollbarMousePositionForScroll(ImGuiWindow* window, ImGuiAxis axis, float scroll_v)
{
    ImGuiContext& g = *GImGui;
    ImRect bb = ImGui::GetWindowScrollbarRect(window, axis);

    // From Scrollbar():
    //float* scroll_v = &window->Scroll[axis];
    const float size_avail_v = window->InnerRect.Max[axis] - window->InnerRect.Min[axis];
    const float size_contents_v = window->ContentSize[axis] + window->WindowPadding[axis] * 2.0f;

    // From ScrollbarEx() onward:

    // V denote the main, longer axis of the scrollbar (= height for a vertical scrollbar)
    const float scrollbar_size_v = bb.Max[axis] - bb.Min[axis];

    // Calculate the height of our grabbable box. It generally represent the amount visible (vs the total scrollable amount)
    // But we maintain a minimum size in pixel to allow for the user to still aim inside.
    const float win_size_v = ImMax(ImMax(size_contents_v, size_avail_v), 1.0f);
    const float grab_h_pixels = ImClamp(scrollbar_size_v * (size_avail_v / win_size_v), g.Style.GrabMinSize, scrollbar_size_v);

    const float scroll_max = ImMax(1.0f, size_contents_v - size_avail_v);
    const float scroll_ratio = ImSaturate(scroll_v / scroll_max);
    const float grab_v = scroll_ratio * (scrollbar_size_v - grab_h_pixels); // Grab position

    ImVec2 position;
    position[axis] = bb.Min[axis] + grab_v + grab_h_pixels * 0.5f;
    position[axis ^ 1] = bb.GetCenter()[axis ^ 1];

    return position;
}

#if IMGUI_VERSION_NUM < 18993
#define ImTrunc ImFloor
#endif

static bool ScrollToWithScrollbar(ImGuiTestContext* ctx, ImGuiWindow* window, ImGuiAxis axis, float scroll_target)
{
    ImGuiContext& g = *ctx->UiContext;
    ctx->Yield();
    ctx->WindowFocus(window->ID);
    if (window->ScrollbarSizes[axis ^ 1] <= 0.0f) // Verify if still exists after yield
        return false;

    const ImRect scrollbar_rect = ImGui::GetWindowScrollbarRect(window, axis);
    const float scrollbar_size_v = scrollbar_rect.Max[axis] - scrollbar_rect.Min[axis];
    const float window_resize_grip_size = ImTrunc(ImMax(g.FontSize * 1.35f, window->WindowRounding + 1.0f + g.FontSize * 0.2f));

    // In case of a very small window, directly use SetScrollX/Y function to prevent resizing it
    // FIXME-TESTS: GetWindowScrollbarMousePositionForScroll() doesn't return the exact value when scrollbar grip is too small
    if (scrollbar_size_v < window_resize_grip_size)
        return false;

    ctx->MouseSetViewport(window);

    const float scroll_src = window->Scroll[axis];
    ImVec2 scrollbar_src_pos = GetWindowScrollbarMousePositionForScroll(window, axis, scroll_src);
    scrollbar_src_pos[axis] = ImMin(scrollbar_src_pos[axis], scrollbar_rect.Min[axis] + scrollbar_size_v - window_resize_grip_size);
    ctx->MouseMoveToPos(scrollbar_src_pos);
    ctx->MouseDown(0);
    ctx->SleepStandard();

    ImVec2 scrollbar_dst_pos = GetWindowScrollbarMousePositionForScroll(window, axis, scroll_target);
    ctx->MouseMoveToPos(scrollbar_dst_pos);
    ctx->MouseUp(0);
    ctx->SleepStandard();

    // Verify that things worked
    const float scroll_result = window->Scroll[axis];
    if (ImFabs(scroll_result - scroll_target) < 1.0f)
        return true;

    // FIXME-TESTS: Investigate
    ctx->LogWarning("Failed to set Scroll%c. Requested %.2f, got %.2f.", 'X' + axis, scroll_target, scroll_result);
    return true;
}

// Supported values for ImGuiTestOpFlags:
// - ImGuiTestOpFlags_NoFocusWindow
void    ImGuiTestContext::ScrollTo(ImGuiTestRef ref, ImGuiAxis axis, float scroll_target, ImGuiTestOpFlags flags)
{
    ImGuiContext& g = *UiContext;
    if (IsError())
        return;

    ImGuiWindow* window = GetWindowByRef(ref);
    IM_CHECK_SILENT(window != nullptr);

    // Early out
    const float scroll_target_clamp = ImClamp(scroll_target, 0.0f, window->ScrollMax[axis]);
    if (ImFabs(window->Scroll[axis] - scroll_target_clamp) < 1.0f)
        return;

    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    const char axis_c = (char)('X' + axis);
    LogDebug("ScrollTo %c %.1f/%.1f", axis_c, scroll_target, window->ScrollMax[axis]);

    if (EngineIO->ConfigRunSpeed == ImGuiTestRunSpeed_Cinematic)
        SleepStandard();

    // Try to use Scrollbar if available
    const ImGuiTestItemInfo scrollbar_item = ItemInfo(ImGui::GetWindowScrollbarID(window, axis), ImGuiTestOpFlags_NoError);
    if (scrollbar_item.ID != 0 && /*EngineIO->ConfigRunSpeed != ImGuiTestRunSpeed_Fast && */ !(flags & ImGuiTestOpFlags_NoFocusWindow))
    {
        if (ScrollToWithScrollbar(this, window, axis, scroll_target_clamp))
        {
            // Verify that things worked
            const float scroll_result = window->Scroll[axis];
            if (ImFabs(scroll_result - scroll_target_clamp) < 1.0f)
                return;

            // FIXME-TESTS: Investigate
            LogWarning("Failed to set Scroll%c. Requested %.2f, got %.2f.", 'X' + axis, scroll_target_clamp, scroll_result);
        }
    }

    // Fallback: manual slow scroll
    // FIXME-TESTS: Consider using mouse wheel, since it can work without taking focus
    int remaining_failures = 3;
    while (!Abort)
    {
        if (ImFabs(window->Scroll[axis] - scroll_target_clamp) < 1.0f)
            break;

        const float scroll_speed = (EngineIO->ConfigRunSpeed == ImGuiTestRunSpeed_Fast) ? FLT_MAX : ImFloor(EngineIO->ScrollSpeed * g.IO.DeltaTime + 0.99f);
        const float scroll_next = ImLinearSweep(window->Scroll[axis], scroll_target, scroll_speed);
        if (axis == ImGuiAxis_X)
            ImGui::SetScrollX(window, scroll_next);
        else
            ImGui::SetScrollY(window, scroll_next);

        // Error handling to avoid getting stuck in this function.
        Yield();
        if (!ScrollErrorCheck(axis, scroll_next, window->Scroll[axis], &remaining_failures))
            break;
    }

    // Need another frame for the result->Rect to stabilize
    Yield();
}

// Supported values for ImGuiTestOpFlags:
// - ImGuiTestOpFlags_NoFocusWindow
void    ImGuiTestContext::ScrollToPos(ImGuiTestRef window_ref, float pos_v, ImGuiAxis axis, ImGuiTestOpFlags flags)
{
    if (IsError())
        return;

    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    LogDebug("ScrollToPos %c %.2f", 'X' + axis, pos_v);

    // Ensure window size and ScrollMax are up-to-date
    Yield();

    ImGuiWindow* window = GetWindowByRef(window_ref);
    IM_CHECK_SILENT(window != NULL);
    float item_curr = pos_v;
    float item_target = ImFloor(window->InnerClipRect.GetCenter()[axis]);
    float scroll_delta = item_target - item_curr;
    float scroll_target = ImClamp(window->Scroll[axis] - scroll_delta, 0.0f, window->ScrollMax[axis]);

    ScrollTo(window->ID, axis, scroll_target, (flags & ImGuiTestOpFlags_NoFocusWindow));
}

void    ImGuiTestContext::ScrollToPosX(ImGuiTestRef window_ref, float pos_x)
{
    ScrollToPos(window_ref, pos_x, ImGuiAxis_X);
}

void    ImGuiTestContext::ScrollToPosY(ImGuiTestRef window_ref, float pos_y)
{
    ScrollToPos(window_ref, pos_y, ImGuiAxis_Y);
}

// Supported values for ImGuiTestOpFlags:
// - ImGuiTestOpFlags_NoFocusWindow
void    ImGuiTestContext::ScrollToItem(ImGuiTestRef ref, ImGuiAxis axis, ImGuiTestOpFlags flags)
{
    if (IsError())
        return;

    // If the item is not currently visible, scroll to get it in the center of our window
    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    ImGuiTestItemInfo item = ItemInfo(ref);
    ImGuiTestRefDesc desc(ref, item);
    LogDebug("ScrollToItem %c %s", 'X' + axis, desc.c_str());

    if (item.ID == 0)
        return;

    // Ensure window size and ScrollMax are up-to-date
    Yield();

    // TabBar are a special case because they have no scrollbar and rely on ScrollButton "<" and ">"
    // FIXME-TESTS: Consider moving to its own function.
    ImGuiContext& g = *UiContext;
    if (axis == ImGuiAxis_X)
        if (ImGuiTabBar* tab_bar = g.TabBars.GetByKey(item.ParentID))
            if (tab_bar->Flags & ImGuiTabBarFlags_FittingPolicyScroll)
            {
                ScrollToTabItem(tab_bar, item.ID);
                return;
            }

    ImGuiWindow* window = item.Window;
    float item_curr = ImFloor(item.RectFull.GetCenter()[axis]);
    float item_target = ImFloor(window->InnerClipRect.GetCenter()[axis]);
    float scroll_delta = item_target - item_curr;
    float scroll_target = ImClamp(window->Scroll[axis] - scroll_delta, 0.0f, window->ScrollMax[axis]);

    ScrollTo(window->ID, axis, scroll_target, (flags & ImGuiTestOpFlags_NoFocusWindow));
}

void    ImGuiTestContext::ScrollToItemX(ImGuiTestRef ref)
{
    ScrollToItem(ref, ImGuiAxis_X);
}

void    ImGuiTestContext::ScrollToItemY(ImGuiTestRef ref)
{
    ScrollToItem(ref, ImGuiAxis_Y);
}

void    ImGuiTestContext::ScrollToTabItem(ImGuiTabBar* tab_bar, ImGuiID tab_id)
{
    if (IsError())
        return;

    // Cancel if "##v", because it's outside the tab_bar rect, and will be considered as "not visible" even if it is!
    //if (GetID("##v") == item->ID)
    //    return;

    IM_CHECK_SILENT(tab_bar != nullptr);
    const ImGuiTabItem* selected_tab_item = ImGui::TabBarFindTabByID(tab_bar, tab_bar->SelectedTabId);
    const ImGuiTabItem* target_tab_item = ImGui::TabBarFindTabByID(tab_bar, tab_id);
    if (target_tab_item == nullptr)
        return;

    int selected_tab_index = tab_bar->Tabs.index_from_ptr(selected_tab_item);
    int target_tab_index = tab_bar->Tabs.index_from_ptr(target_tab_item);

    ImGuiTestRef backup_ref = GetRef();
    SetRef(tab_bar->ID);

    if (selected_tab_index > target_tab_index)
    {
        MouseMove("##<");
        for (int i = 0; i < selected_tab_index - target_tab_index; ++i)
            MouseClick(0);
    }
    else
    {
        MouseMove("##>");
        for (int i = 0; i < target_tab_index - selected_tab_index; ++i)
            MouseClick(0);
    }

    // Skip the scroll animation
    if (EngineIO->ConfigRunSpeed == ImGuiTestRunSpeed_Fast)
    {
        tab_bar->ScrollingAnim = tab_bar->ScrollingTarget;
        Yield();
    }

    SetRef(backup_ref);
}

// Verify that ScrollMax is stable regardless of scrolling position
// - This can break when the layout of clipped items doesn't match layout of unclipped items
// - This can break with non-rounded calls to ItemSize(), namely when the starting position is negative (above visible area)
//   We should ideally be more tolerant of non-rounded sizes passed by the users.
// - One of the net visible effect of an unstable ScrollMax is that the End key would put you at a spot that's not exactly the lowest spot,
//   and so a second press to End would you move again by a few pixels.
// FIXME-TESTS: Make this an iterative, smooth scroll.
void    ImGuiTestContext::ScrollVerifyScrollMax(ImGuiTestRef ref)
{
    ImGuiWindow* window = GetWindowByRef(ref);
    ImGui::SetScrollY(window, 0.0f);
    Yield();
    float scroll_max_0 = window->ScrollMax.y;
    ImGui::SetScrollY(window, window->ScrollMax.y);
    Yield();
    float scroll_max_1 = window->ScrollMax.y;
    IM_CHECK_EQ(scroll_max_0, scroll_max_1);
}

void    ImGuiTestContext::NavMoveTo(ImGuiTestRef ref)
{
    if (IsError())
        return;

    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    ImGuiContext& g = *UiContext;
    ImGuiTestItemInfo item = ItemInfo(ref);
    ImGuiTestRefDesc desc(ref, item);
    LogDebug("NavMove to %s", desc.c_str());

    if (item.ID == 0)
        return;

    if (EngineIO->ConfigRunSpeed == ImGuiTestRunSpeed_Cinematic)
        SleepStandard();

    // Focus window before scrolling/moving so things are nicely visible
    WindowFocus(item.Window->ID);

    // Teleport
    // FIXME-NAV: We should have a nav request feature that does this,
    // except it'll have to queue the request to find rect, then set scrolling, which would incur a 2 frame delay :/
    // FIXME-TESTS-NOT_SAME_AS_END_USER
    IM_ASSERT(g.NavMoveSubmitted == false);
    ImRect rect_rel = item.RectFull;
    rect_rel.Translate(ImVec2(-item.Window->Pos.x, -item.Window->Pos.y));
    ImGui::SetNavID(item.ID, (ImGuiNavLayer)item.NavLayer, 0, rect_rel);
#if IMGUI_VERSION_NUM >= 19136
    ImGui::SetNavCursorVisible(true);
    g.NavHighlightItemUnderNav = true;
#else
    g.NavDisableHighlight = false;
    g.NavDisableMouseHover = true;
#endif
    g.NavMousePosDirty = true;
    ImGui::ScrollToBringRectIntoView(item.Window, item.RectFull);
    while (g.NavMoveSubmitted)
        Yield();
    Yield();

    if (!Abort)
        if (g.NavId != item.ID)
            IM_ERRORF_NOHDR("Unable to set NavId to %s", desc.c_str());
}

void    ImGuiTestContext::NavActivate()
{
    if (IsError())
        return;

    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    LogDebug("NavActivate");
    Yield(); // ?
    KeyPress(ImGuiKey_Space);
}

void    ImGuiTestContext::NavInput()
{
    if (IsError())
        return;

    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    LogDebug("NavInput");
    KeyPress(ImGuiKey_Enter);
}

// Supported values for ImGuiTestOpFlags:
// - ImGuiTestOpFlags_MoveToEdgeL
// - ImGuiTestOpFlags_MoveToEdgeR
// - ImGuiTestOpFlags_MoveToEdgeU
// - ImGuiTestOpFlags_MoveToEdgeD
static ImVec2 GetMouseAimingPos(const ImGuiTestItemInfo& item, ImGuiTestOpFlags flags)
{
    ImRect r = item.RectClipped;
    ImVec2 pos;
    if (flags & ImGuiTestOpFlags_MoveToEdgeL)
        pos.x = (r.Min.x + 1.0f);
    else if (flags & ImGuiTestOpFlags_MoveToEdgeR)
        pos.x = (r.Max.x - 1.0f);
    else
        pos.x = (r.Min.x + r.Max.x) * 0.5f;
    if (flags & ImGuiTestOpFlags_MoveToEdgeU)
        pos.y = (r.Min.y + 1.0f);
    else if (flags & ImGuiTestOpFlags_MoveToEdgeD)
        pos.y = (r.Max.y - 1.0f);
    else
        pos.y = (r.Min.y + r.Max.y) * 0.5f;
    return pos;
}

void ImGuiTestContext::_MakeAimingSpaceOverPos(ImGuiViewport* viewport, ImGuiWindow* over_window, const ImVec2& over_pos)
{
    ImGuiContext& g = *UiContext;

    // if window == nullptr : make space to reach void
    // if window != nullptr : make space to reach window
    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    LogDebug("_MakeAimingSpaceOverPos(over_window = '%s', over_pos = %.2f,%.2f)", over_window ? over_window->Name : "N/A", over_pos.x, over_pos.y);

    const int over_window_n = (over_window != nullptr) ? ImGui::FindWindowDisplayIndex(over_window) : -1;
#if IMGUI_VERSION_NUM < 19183
    const ImVec2 hover_padding = g.WindowsHoverPadding;
#else
    const ImVec2 hover_padding = ImVec2(g.WindowsBorderHoverPadding, g.WindowsBorderHoverPadding);
#endif

    const ImVec2 window_min_pos = over_pos + hover_padding + ImVec2(1.0f, 1.0f);
    for (int window_n = g.Windows.Size - 1; window_n > over_window_n; window_n--)
    {
        ImGuiWindow* window = g.Windows[window_n];
        if (window->WasActive == false)
            continue;
#ifdef IMGUI_HAS_DOCK
        if (window->Viewport != viewport)
            continue;
        if (window->RootWindowDockTree != window)
            continue;
#else
        IM_UNUSED(viewport);
        if (window->RootWindow != window)
            continue;
        if (window->Flags & ImGuiWindowFlags_NoMove)
            continue;
#endif
        if (window->Rect().Contains(window_min_pos))
        {
            WindowMove(window->ID, window_min_pos);

            // Verify that we have managed to move the window..
            if (ImLengthSqr(window->Pos - window_min_pos) >= 1.0f)
            {
                LogWarning("Failed to move window '%s'! While trying to make space to click at (%.2f,%.2f) over window '%s'.",
                    window->Name, over_pos.x, over_pos.y, over_window ? over_window->Name : "N/A");
                //IM_CHECK_EQ(window->Pos, window_min_pos); // Failed to move window to make space
            }
        }
    }
}

static void FocusOrMakeClickableAtPos(ImGuiTestContext* ctx, ImGuiWindow* window, const ImVec2& pos)
{
    IM_ASSERT(window != nullptr);

    // Avoid unnecessary focus
    // While this is generally desirable and much more consistent with user behavior,
    // it make test-engine behavior a little less deterministic.
    // incorrectly written tests could possibly succeed or fail based on position of other windows.
    bool is_covered = ctx->FindHoveredWindowAtPos(pos) != window;
#if IMGUI_VERSION_NUM >= 18944
    bool is_inhibited = ImGui::IsWindowContentHoverable(window) == false;
#else
    bool is_inhibited = false;
#endif
    // FIXME-TESTS-NOT_SAME_AS_END_USER: This has too many side effect, could we do without?
    // - e.g. This can close a modal.
    if (is_covered || is_inhibited)
    {
        // Testing ImGuiWindowFlags_NoBringToFrontOnFocus is similar to what FocusWindow() does
        ImGuiWindow* focus_front_window = window ? window->RootWindow : nullptr;
#ifdef IMGUI_HAS_DOCK
        ImGuiWindow* display_front_window = window ? window->RootWindowDockTree : nullptr;
#else
        ImGuiWindow* display_front_window = window ? window->RootWindow : nullptr;
#endif
        if ((window->Flags | focus_front_window->Flags | display_front_window->Flags) & ImGuiWindowFlags_NoBringToFrontOnFocus)
        {
            // FIXME-TESTS: Aim to make it the common/default path, and WindowBringToFront() the exceptional path!
            ctx->_MakeAimingSpaceOverPos(window->Viewport, window, pos);
        }
        else
        {
            ctx->WindowBringToFront(window->ID);
        }
    }
}

// Conceptucally this could be called ItemHover()
// Supported values for ImGuiTestOpFlags:
// - ImGuiTestOpFlags_NoFocusWindow
// - ImGuiTestOpFlags_NoCheckHoveredId
// - ImGuiTestOpFlags_IsSecondAttempt [used when recursively calling ourself)
// - ImGuiTestOpFlags_MoveToEdgeXXX flags
// FIXME-TESTS: This is too eagerly trying to scroll everything even if already visible.
// FIXME: Maybe ImGuiTestOpFlags_NoCheckHoveredId could be automatic if we detect that another item is active as intended?
void    ImGuiTestContext::MouseMove(ImGuiTestRef ref, ImGuiTestOpFlags flags)
{
    if (IsError())
        return;

    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    ImGuiContext& g = *UiContext;

    ImGuiTestItemInfo item;
    if (flags & ImGuiTestOpFlags_NoAutoOpenFullPath)
        item = ItemInfo(ref);
    else
        item = ItemInfoOpenFullPath(ref);

    ImGuiTestRefDesc desc(ref, item);
    LogDebug("MouseMove to %s", desc.c_str());
    if (item.ID == 0)
        return;

    ImGuiWindow* window = item.Window;
    if (!window->WasActive)
    {
        LogError("Window '%s' is not active!", window->Name);
        return;
    }

    // FIXME-TESTS: If window was not brought to front (because of either ImGuiWindowFlags_NoBringToFrontOnFocus or ImGuiTestOpFlags_NoFocusWindow)
    // then we need to make space by moving other windows away.
    // An easy to reproduce this bug is to run "docking_dockspace_tab_amend" with Test Engine UI over top-left corner, covering the Tools menu.

    // Check visibility and scroll if necessary
    {
#if IMGUI_VERSION_NUM < 19183
        const ImVec2 hover_padding = g.WindowsHoverPadding;
#else
        const ImVec2 hover_padding = ImVec2(g.WindowsBorderHoverPadding, g.WindowsBorderHoverPadding);
#endif
        if (item.NavLayer == ImGuiNavLayer_Main)
        {
            float min_visible_size = 10.0f;
            float min_window_size_x = window->DecoInnerSizeX1 + window->DecoOuterSizeX1 + window->DecoOuterSizeX2 + min_visible_size + hover_padding.x * 2.0f;
            float min_window_size_y = window->DecoInnerSizeY1 + window->DecoOuterSizeY1 + window->DecoOuterSizeY2 + min_visible_size + hover_padding.y * 2.0f;
            if ((window->Size.x < min_window_size_x || window->Size.y < min_window_size_y) && (window->Flags & ImGuiWindowFlags_NoResize) == 0 && (window->Flags & ImGuiWindowFlags_AlwaysAutoResize) == 0)
            {
                LogDebug("MouseMove: Will attempt to resize window to make item in main scrolling layer visible.");
                if (window->Size.x < min_window_size_x)
                    WindowResize(window->ID, ImVec2(min_window_size_x, window->Size.y));
                if (window->Size.y < min_window_size_y)
                    WindowResize(window->ID, ImVec2(window->Size.x, min_window_size_y));
                item = ItemInfo(item.ID);
            }
        }

        ImRect window_r = window->InnerClipRect;
        window_r.Expand(ImVec2(-hover_padding.x, -hover_padding.y));

        ImRect item_r_clipped;
        item_r_clipped.Min.x = ImClamp(item.RectFull.Min.x, window_r.Min.x, window_r.Max.x);
        item_r_clipped.Min.y = ImClamp(item.RectFull.Min.y, window_r.Min.y, window_r.Max.y);
        item_r_clipped.Max.x = ImClamp(item.RectFull.Max.x, window_r.Min.x, window_r.Max.x);
        item_r_clipped.Max.y = ImClamp(item.RectFull.Max.y, window_r.Min.y, window_r.Max.y);

        // In theory all we need is one visible point, but it is generally nicer if we scroll toward visibility.
        // Bias toward reducing amount of horizontal scroll.
        float visibility_ratio_x = (item_r_clipped.GetWidth() + 1.0f) / (item.RectFull.GetWidth() + 1.0f);
        float visibility_ratio_y = (item_r_clipped.GetHeight() + 1.0f) / (item.RectFull.GetHeight() + 1.0f);

        if (item.NavLayer == ImGuiNavLayer_Main)
        {
            if (visibility_ratio_x < 0.70f)
                ScrollToItem(ref, ImGuiAxis_X, ImGuiTestOpFlags_NoFocusWindow);
            if (visibility_ratio_y < 0.90f)
                ScrollToItem(ref, ImGuiAxis_Y, ImGuiTestOpFlags_NoFocusWindow);
        }
        // FIXME: Scroll parent window
    }

    // Menu layer is not scrollable: attempt to resize window.
    if (item.NavLayer == ImGuiNavLayer_Menu)
    {
        // FIXME-TESTS: We designed RectClipped as being within RectFull which is not what we want here. Approximate using window's Max.x
        ImRect window_r = window->Rect();
        if (item.RectFull.Min.x > window_r.Max.x)
        {
            float extra_width_desired = item.RectFull.Max.x - window_r.Max.x; // item->RectClipped.Max.x;
            if (extra_width_desired > 0.0f && (flags & ImGuiTestOpFlags_IsSecondAttempt) == 0)
            {
                LogDebug("MouseMove: Will attempt to resize window to make item in menu layer visible.");
                WindowResize(window->ID, window->Size + ImVec2(extra_width_desired, 0.0f));
            }
        }
    }

    // Update item
    item = ItemInfo(item.ID);

    // FIXME-TESTS-NOT_SAME_AS_END_USER
    ImVec2 pos = item.RectFull.GetCenter();
    if (WindowTeleportToMakePosVisible(window->ID, pos))
        item = ItemInfo(item.ID);

    // Handle the off-chance that e.g. item/window stops being submitted while scrolling (easy to repro by pressing Esc during a long scroll)
    if (item.ID == 0)
    {
        LogError("MouseMove: item doesn't exist anymore (after scrolling)");
        return;
    }

    // Keep a copy of item info
    const ImGuiTestItemInfo item_initial_state = item;

    // Target point
    pos = GetMouseAimingPos(item, flags);

    // Focus window
    if (!(flags & ImGuiTestOpFlags_NoFocusWindow) && item.Window != nullptr)
        FocusOrMakeClickableAtPos(this, item.Window, pos);

    // Another is window active test (in the case focus change has a side effect but also as we have yield an extra frame)
    if (!item.Window->WasActive)
    {
        LogError("MouseMove: Window '%s' is not active (after aiming)", item.Window->Name);
        return;
    }

    MouseSetViewport(item.Window);
    MouseMoveToPos(pos);

    // Focus again in case something made us lost focus (which could happen on a simple hover)
    if (!(flags & ImGuiTestOpFlags_NoFocusWindow))
        FocusOrMakeClickableAtPos(this, item.Window, pos);

    // Check hovering target: may be an item (common) or a window (rare)
    if (!Abort && !(flags & ImGuiTestOpFlags_NoCheckHoveredId))
    {
        ImGuiID hovered_id;
        bool is_hovered_item;

        // Give a few extra frames to validate hovering.
        // In the vast majority of case this will be set on the first attempt,
        // but e.g. blocking popups may need to close based on external logic.
        for (int remaining_attempts = 3; remaining_attempts > 0; remaining_attempts--)
        {
            hovered_id = g.HoveredIdPreviousFrame;
            is_hovered_item = (hovered_id == item.ID);
            if (is_hovered_item)
                break;
            Yield();
        }

        bool is_hovered_window = is_hovered_item ? true : false;
        if (!is_hovered_item)
            for (ImGuiWindow* hovered_window = g.HoveredWindow; hovered_window != nullptr && !is_hovered_window; hovered_window = hovered_window->ParentWindow)
                if (hovered_window->ID == item.ID && hovered_window == item.Window)
                    is_hovered_window = true;

        if (!is_hovered_item && !is_hovered_window)
        {
            // Check if we are accidentally hovering resize grip (which uses ImGuiButtonFlags_FlattenChildren)
            if (!(window->Flags & ImGuiWindowFlags_NoResize) && !(flags & ImGuiTestOpFlags_IsSecondAttempt))
            {
                bool is_hovering_resize_corner = false;
                for (int n = 0; n < 2; n++)
                    is_hovering_resize_corner |= (hovered_id == ImGui::GetWindowResizeCornerID(window, n));
                if (is_hovering_resize_corner)
                {
                    LogDebug("MouseMove: Child obstructed by parent's ResizeGrip, trying to resize window and trying again..");
#if IMGUI_VERSION_NUM < 19172
                    float extra_size = window->CalcFontSize() * 3.0f;
#else
                    float extra_size = window->FontRefSize * 3.0f;
#endif
                    WindowResize(window->ID, window->Size + ImVec2(extra_size, extra_size));
                    MouseMove(ref, flags | ImGuiTestOpFlags_IsSecondAttempt);
                    return;
                }
            }

            // Update item
            item = ItemInfo(item.ID);

            // Log message
            // FIXME: Consider a second attempt?
            ImVec2 pos_old = item_initial_state.RectFull.Min;
            ImVec2 pos_new = item.RectFull.Min;
            ImVec2 size_old = item_initial_state.RectFull.GetSize();
            ImVec2 size_new = item.RectFull.GetSize();
            Str256f error_message(
                "MouseMove: Unable to Hover %s:\n"
                "- Expected item 0x%08X in window '%s', targeted position: (%.1f,%.1f)'\n"
                "- Hovered id was 0x%08X in '%s'.\n"
                "- Before mouse move: Item Pos (%6.1f,%6.1f) Size (%6.1f,%6.1f)\n"
                "- After  mouse move: Item Pos (%6.1f,%6.1f) Size (%6.1f,%6.1f)",
                desc.c_str(),
                item.ID, item.Window ? item.Window->Name : "<nullptr>", pos.x, pos.y,
                hovered_id, g.HoveredWindow ? g.HoveredWindow->Name : "",
                pos_old.x, pos_old.y, size_old.x, size_old.y,
                pos_new.x, pos_new.y, size_new.x, size_new.y);
            IM_ERRORF_NOHDR("%s", error_message.c_str());
        }
    }
}

void    ImGuiTestContext::MouseSetViewport(ImGuiWindow* window)
{
    IM_CHECK_SILENT(window != nullptr);
#ifdef IMGUI_HAS_VIEWPORT
    ImGuiViewportP* viewport = window ? window->Viewport : nullptr;
    ImGuiID viewport_id = viewport ? viewport->ID : 0;
    if (window->Viewport == nullptr)
        IM_CHECK(window->WasActive == false); // only time this is allowed is an inactive window (where the viewport was destroyed)
    if (Inputs->MouseHoveredViewport != viewport_id)
    {
        IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
        LogDebug("MouseSetViewport changing to 0x%08X (window '%s')", viewport_id, window->Name);
        Inputs->MouseHoveredViewport = viewport_id;
        Yield(2);
    }
#else
    IM_UNUSED(window);
#endif
}

// May be 0 to specify "automatic" (based on platform stack, rarely used)
void    ImGuiTestContext::MouseSetViewportID(ImGuiID viewport_id)
{
    if (IsError())
        return;

    if (Inputs->MouseHoveredViewport != viewport_id)
    {
        IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
        LogDebug("MouseSetViewportID changing to 0x%08X", viewport_id);
        Inputs->MouseHoveredViewport = viewport_id;
        ImGuiTestEngine_Yield(Engine);
    }
}

// Make the point at 'pos' (generally expected to be within window's boundaries) visible in the viewport,
// so it can be later focused then clicked.
bool    ImGuiTestContext::WindowTeleportToMakePosVisible(ImGuiTestRef ref, ImVec2 pos)
{
    ImGuiContext& g = *UiContext;
    if (IsError())
        return false;
    ImGuiWindow* window = GetWindowByRef(ref);
    IM_CHECK_SILENT_RETV(window != nullptr, false);

#ifdef IMGUI_HAS_DOCK
    // This is particularly useful for docked windows, as we have to move root dockspace window instead of docket window
    // itself. As a side effect this also adds support for child windows.
    window = window->RootWindowDockTree;
#endif

    ImRect visible_r;
    visible_r.Min = GetMainMonitorWorkPos();
    visible_r.Max = visible_r.Min + GetMainMonitorWorkSize();
    if (!visible_r.Contains(pos))
    {
        // Fallback move window directly to make our item reachable with the mouse.
        // FIXME-TESTS-NOT_SAME_AS_END_USER
        float pad = g.FontSize;
        ImVec2 delta;
        delta.x = (pos.x < visible_r.Min.x) ? (visible_r.Min.x - pos.x + pad) : (pos.x > visible_r.Max.x) ? (visible_r.Max.x - pos.x - pad) : 0.0f;
        delta.y = (pos.y < visible_r.Min.y) ? (visible_r.Min.y - pos.y + pad) : (pos.y > visible_r.Max.y) ? (visible_r.Max.y - pos.y - pad) : 0.0f;
        ImGui::SetWindowPos(window, window->Pos + delta, ImGuiCond_Always);
        LogDebug("WindowTeleportToMakePosVisible '%s' delta (%.1f,%.1f)", window->Name, delta.x, delta.y);
        Yield();
        return true;
    }
    return false;
}

// ignore_list is a nullptr-terminated list of pointers
// Windows that are below all of ignore_list windows are not hidden.
// FIXME-TESTS-NOT_SAME_AS_END_USER: Aim to get rid of this.
void ImGuiTestContext::_ForeignWindowsHideOverPos(const ImVec2& pos, ImGuiWindow** ignore_list)
{
    ImGuiContext& g = *UiContext;
    if (IsError())
        return;

    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    LogDebug("ForeignWindowsHideOverPos (%.0f,%.0f)", pos.x, pos.y);
    IM_CHECK_SILENT(ignore_list != nullptr); // It makes little sense to call this function with an empty list.
    IM_CHECK_SILENT(ignore_list[0] != nullptr);
    //auto& ctx = this;  IM_SUSPEND_TESTFUNC();

    // Find lowest ignored window index. All windows rendering above this index will be hidden. All windows rendering
    // below this index do not prevent interactions with these windows already, and they can be ignored.
    int min_window_index = g.Windows.Size;
    for (int i = 0; ignore_list[i]; i++)
        min_window_index = ImMin(min_window_index, ImGui::FindWindowDisplayIndex(ignore_list[i]));

#if IMGUI_VERSION_NUM < 19183
    const ImVec2 hover_padding = g.WindowsHoverPadding;
#else
    const ImVec2 hover_padding = ImVec2(g.WindowsBorderHoverPadding, g.WindowsBorderHoverPadding);
#endif
    bool hidden_windows = false;
    for (int i = 0; i < g.Windows.Size; i++)
    {
        ImGuiWindow* other_window = g.Windows[i];
        if (other_window->RootWindow == other_window && other_window->WasActive)
        {
            ImRect r = other_window->Rect();
            r.Expand(hover_padding);
            if (r.Contains(pos))
            {
                for (int j = 0; ignore_list[j]; j++)
#ifdef IMGUI_HAS_DOCK
                    if (ignore_list[j]->RootWindowDockTree == other_window->RootWindowDockTree)
#else
                    if (ignore_list[j] == other_window)
#endif
                    {
                        other_window = nullptr;
                        break;
                    }

                if (other_window && ImGui::FindWindowDisplayIndex(other_window) < min_window_index)
                    other_window = nullptr;

                if (other_window)
                {
                    ForeignWindowsToHide.push_back(other_window);
                    hidden_windows = true;
                }
            }
        }
    }
    if (hidden_windows)
        Yield();
}

void    ImGuiTestContext::_ForeignWindowsUnhideAll()
{
    ForeignWindowsToHide.clear();
    Yield();
}

void    ImGuiTestContext::MouseMoveToPos(ImVec2 target)
{
    ImGuiContext& g = *UiContext;
    if (IsError())
        return;

    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    LogDebug("MouseMoveToPos from (%.0f,%.0f) to (%.0f,%.0f)", Inputs->MousePosValue.x, Inputs->MousePosValue.y, target.x, target.y);

    if (EngineIO->ConfigRunSpeed == ImGuiTestRunSpeed_Cinematic)
        SleepStandard();

    // Enforce a mouse move if we are already at destination, to enforce g.NavHighlightItemUnderNav gets cleared.
#if IMGUI_VERSION_NUM >= 19136
    if (g.NavHighlightItemUnderNav && ImLengthSqr(Inputs->MousePosValue - target) < 1.0f)
#else
    if (g.NavDisableMouseHover && ImLengthSqr(Inputs->MousePosValue - target) < 1.0f)
#endif
    {
        Inputs->MousePosValue = target + ImVec2(1.0f, 0.0f);
        ImGuiTestEngine_Yield(Engine);
    }

    if (EngineIO->ConfigRunSpeed == ImGuiTestRunSpeed_Fast)
    {
        Inputs->MousePosValue = target;
        ImGuiTestEngine_Yield(Engine);
        ImGuiTestEngine_Yield(Engine);
        return;
    }

    // Simulate slower movements. We use a slightly curved movement to make the movement look less robotic.

    // Calculate some basic parameters
    const ImVec2 start_pos = Inputs->MousePosValue;
    const ImVec2 delta = target - start_pos;
    const float length2 = ImLengthSqr(delta);
    const float length = (length2 > 0.0001f) ? ImSqrt(length2) : 1.0f;
    const float inv_length = 1.0f / length;

    // Short distance alter speed and wobble
    float base_speed = EngineIO->MouseSpeed;
    float base_wobble = EngineIO->MouseWobble;
    if (length < base_speed * 1.0f)
    {
        // Time = 1.0f -> wobble max, Time = 0.0f -> no wobble
        base_wobble *= length / base_speed;

        // Slow down for short movements(all movement in the 0.0f..1.0f range are remapped to a 0.5f..1.0f seconds)
        if (EngineIO->ConfigRunSpeed == ImGuiTestRunSpeed_Cinematic)
        {
            float approx_time = length / base_speed;
            approx_time = 0.5f + ImSaturate(approx_time * 0.5f);
            base_speed = length / approx_time;
        }
    }

    // Calculate a vector perpendicular to the motion delta
    const ImVec2 perp = ImVec2(delta.y, -delta.x) * inv_length;

    // Calculate how much wobble we want, clamped to max out when the delta is 100 pixels (shorter movements get less wobble)
    const float position_offset_magnitude = ImClamp(length, 1.0f, 100.0f) * base_wobble;

    // Wobble positions, using a sine wave based on position as a cheap way to get a deterministic offset
    ImVec2 intermediate_pos_a = start_pos + (delta * 0.3f);
    ImVec2 intermediate_pos_b = start_pos + (delta * 0.6f);
    intermediate_pos_a += perp * ImSin(intermediate_pos_a.y * 0.1f) * position_offset_magnitude;
    intermediate_pos_b += perp * ImCos(intermediate_pos_b.y * 0.1f) * position_offset_magnitude;

    // We manipulate Inputs->MousePosValue without reading back from g.IO.MousePos because the later is rounded.
    // To handle high framerate it is easier to bypass this rounding.
    float current_dist = 0.0f; // Our current distance along the line (in pixels)
    while (true)
    {
        float move_speed = base_speed * g.IO.DeltaTime;

        //if (g.IO.KeyShift)
        //    move_speed *= 0.1f;

        current_dist += move_speed; // Move along the line

        // Calculate a parametric position on the direct line that we will use for the curve
        float t = current_dist * inv_length;
        t = ImClamp(t, 0.0f, 1.0f);
        t = 1.0f - ((ImCos(t * IM_PI) + 1.0f) * 0.5f); // Generate a smooth curve with acceleration/deceleration

        //ImGui::GetOverlayDrawList()->AddCircle(target, 10.0f, IM_COL32(255, 255, 0, 255));

        if (t >= 1.0f)
        {
            Inputs->MousePosValue = target;
            ImGuiTestEngine_Yield(Engine);
            ImGuiTestEngine_Yield(Engine);
            return;
        }
        else
        {
            // Use a bezier curve through the wobble points
            Inputs->MousePosValue = ImBezierCubicCalc(start_pos, intermediate_pos_a, intermediate_pos_b, target, t);
            //ImGui::GetOverlayDrawList()->AddBezierCurve(start_pos, intermediate_pos_a, intermediate_pos_b, target, IM_COL32(255,0,0,255), 1.0f);
            ImGuiTestEngine_Yield(Engine);
        }
    }
}

// This always teleport the mouse regardless of fast/slow mode. Useful e.g. to set initial mouse position for a GIF recording.
// Supported values for ImGuiTestOpFlags:
// - ImGuiTestOpFlags_NoYield
void	ImGuiTestContext::MouseTeleportToPos(ImVec2 target, ImGuiTestOpFlags flags)
{
    if (IsError())
        return;

    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    LogDebug("MouseTeleportToPos from (%.0f,%.0f) to (%.0f,%.0f)", Inputs->MousePosValue.x, Inputs->MousePosValue.y, target.x, target.y);

    Inputs->MousePosValue = target;
    if ((flags & ImGuiTestOpFlags_NoYield) == 0)
    {
        ImGuiTestEngine_Yield(Engine);
        ImGuiTestEngine_Yield(Engine);
    }
}

void    ImGuiTestContext::MouseDown(ImGuiMouseButton button)
{
    if (IsError())
        return;

    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    LogDebug("MouseDown %d", button);
    if (EngineIO->ConfigRunSpeed == ImGuiTestRunSpeed_Cinematic)
        SleepStandard();

    UiContext->IO.MouseClickedTime[button] = -FLT_MAX; // Prevent accidental double-click from happening ever
    Inputs->MouseButtonsValue |= (1 << button);
    Yield();
}

void    ImGuiTestContext::MouseUp(ImGuiMouseButton button)
{
    if (IsError())
        return;

    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    LogDebug("MouseUp %d", button);
    if (EngineIO->ConfigRunSpeed == ImGuiTestRunSpeed_Cinematic)
        SleepShort();

    Inputs->MouseButtonsValue &= ~(1 << button);
    Yield();
}

// TODO: click time argument (seconds and/or frames)
void    ImGuiTestContext::MouseClick(ImGuiMouseButton button)
{
    if (IsError())
        return;
    MouseClickMulti(button, 1);
}

// TODO: click time argument (seconds and/or frames)
void    ImGuiTestContext::MouseClickMulti(ImGuiMouseButton button, int count)
{
    if (IsError())
        return;

    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    if (count > 1)
        LogDebug("MouseClickMulti %d x%d", button, count);
    else
        LogDebug("MouseClick %d", button);

    // Make sure mouse buttons are released
    IM_ASSERT(count >= 1);
    IM_ASSERT(Inputs->MouseButtonsValue == 0);
    Yield();

    // Press
    UiContext->IO.MouseClickedTime[button] = -FLT_MAX; // Prevent accidental double-click from happening ever

    for (int n = 0; n < count; n++)
    {
        Inputs->MouseButtonsValue = (1 << button);
        if (EngineIO->ConfigRunSpeed == ImGuiTestRunSpeed_Cinematic)
            SleepShort();
        else if (EngineIO->ConfigRunSpeed != ImGuiTestRunSpeed_Fast)
            Yield(2); // Leave enough time for non-alive IDs to expire. (#5325)
        else
            Yield();
        Inputs->MouseButtonsValue = 0;

        if (EngineIO->ConfigRunSpeed != ImGuiTestRunSpeed_Fast)
            Yield(2); // Not strictly necessary but covers more variant.
        else
            Yield();
    }

    // Now NewFrame() has seen the mouse release.
    // Let the imgui frame finish, now e.g. Button() function will return true. Start a new frame.
    Yield();
}

// TODO: click time argument (seconds and/or frames)
void    ImGuiTestContext::MouseDoubleClick(ImGuiMouseButton button)
{
    MouseClickMulti(button, 2);
}

void    ImGuiTestContext::MouseLiftDragThreshold(ImGuiMouseButton button)
{
    if (IsError())
        return;

    ImGuiContext& g = *UiContext;
    g.IO.MouseDragMaxDistanceSqr[button] = (g.IO.MouseDragThreshold * g.IO.MouseDragThreshold) + (g.IO.MouseDragThreshold * g.IO.MouseDragThreshold);
}

ImGuiWindow* ImGuiTestContext::FindHoveredWindowAtPos(const ImVec2& pos)
{
#if IMGUI_VERSION_NUM >= 19062
    ImGuiWindow* hovered_window = nullptr;
    ImGui::FindHoveredWindowEx(pos, true, &hovered_window, nullptr);
    return hovered_window;
#else
    // Modeled on FindHoveredWindow() in imgui.cpp.
    // Ideally that core function would be refactored to avoid this copy.
    // - Need to take account of MovingWindow specificities and early out.
    // - Need to be able to skip viewport compare.
    // So for now we use a custom function.
    ImGuiContext& g = *UiContext;
    ImVec2 padding_regular = g.Style.TouchExtraPadding;
    ImVec2 padding_for_resize = g.IO.ConfigWindowsResizeFromEdges ? g.WindowsHoverPadding : padding_regular;
    for (int i = g.Windows.Size - 1; i >= 0; i--)
    {
        ImGuiWindow* window = g.Windows[i];
        if (!window->Active || window->Hidden)
            continue;
        if (window->Flags & ImGuiWindowFlags_NoMouseInputs)
            continue;

        // Using the clipped AABB, a child window will typically be clipped by its parent (not always)
        ImVec2 hit_padding = (window->Flags & (ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize)) ? padding_regular : padding_for_resize;
        ImRect r = window->OuterRectClipped;
        r.Expand(hit_padding);
        if (!r.Contains(pos))
            continue;

        // Support for one rectangular hole in any given window
        // FIXME: Consider generalizing hit-testing override (with more generic data, callback, etc.) (#1512)
        if (window->HitTestHoleSize.x != 0)
        {
            ImVec2 hole_pos(window->Pos.x + (float)window->HitTestHoleOffset.x, window->Pos.y + (float)window->HitTestHoleOffset.y);
            ImVec2 hole_size((float)window->HitTestHoleSize.x, (float)window->HitTestHoleSize.y);
            if (ImRect(hole_pos, hole_pos + hole_size).Contains(pos))
                continue;
        }

        return window;
    }
    return nullptr;
#endif
}

static bool IsPosOnVoid(ImGuiContext& g, const ImVec2& pos)
{
#if IMGUI_VERSION_NUM < 19183
    const ImVec2 hover_padding = g.WindowsHoverPadding;
#else
    const ImVec2 hover_padding = ImVec2(g.WindowsBorderHoverPadding, g.WindowsBorderHoverPadding);
#endif
    for (ImGuiWindow* window : g.Windows)
#ifdef IMGUI_HAS_DOCK
        if (window->RootWindowDockTree == window && window->WasActive)
#else
        if (window->RootWindow == window && window->WasActive)
#endif
        {
            ImRect r = window->Rect();
            r.Expand(hover_padding);
            if (r.Contains(pos))
                return false;
        }
    return true;
}

// Sample viewport for an easy location with nothing on it.
// FIXME-OPT: If ever any problematic:
// - (1) could iterate g.WindowsFocusOrder[] now that we made the switch of it only containing root windows
// - (2) increase steps iteratively
// - (3) remember last answer and tries it first.
// - (4) shortpath to failure negative if a window covers the whole viewport?
bool    ImGuiTestContext::FindExistingVoidPosOnViewport(ImGuiViewport* viewport, ImVec2* out)
{
    ImGuiContext& g = *UiContext;
    if (IsError())
        return false;

    for (int yn = 0; yn < 20; yn++)
        for (int xn = 0; xn < 20; xn++)
        {
            ImVec2 pos = viewport->Pos + viewport->Size * ImVec2(xn / 20.0f, yn / 20.0f);
            if (!IsPosOnVoid(g, pos))
                continue;
            *out = pos;
            return true;
        }
    return false;
}

ImVec2   ImGuiTestContext::GetPosOnVoid(ImGuiViewport* viewport)
{
    if (IsError())
        return ImVec2();

    ImVec2 void_pos;
    bool found_existing_void_pos = FindExistingVoidPosOnViewport(viewport, &void_pos);
    if (found_existing_void_pos)
        return void_pos;

    // Move windows away
    // FIXME: Should be optional and otherwise error.
    void_pos = viewport->Pos + ImVec2(1, 1);
    _MakeAimingSpaceOverPos(viewport, nullptr, void_pos);

    return void_pos;
}

ImVec2  ImGuiTestContext::GetWindowTitlebarPoint(ImGuiTestRef window_ref)
{
    // FIXME-TESTS: Need to find a -visible- click point. drag_pos may end up being outside of main viewport.
    if (IsError())
        return ImVec2();

    ImGuiWindow* window = GetWindowByRef(window_ref);
    if (window == nullptr)
    {
        IM_ERRORF_NOHDR("Unable to locate ref window: '%s'", window_ref.Path);
        return ImVec2();
    }

    ImVec2 drag_pos;
    for (int n = 0; n < 2; n++)
    {
#ifdef IMGUI_HAS_DOCK
        if (window->DockNode != nullptr && window->DockNode->TabBar != nullptr)
        {
            ImGuiTabBar* tab_bar = window->DockNode->TabBar;
            ImGuiTabItem* tab = ImGui::TabBarFindTabByID(tab_bar, window->TabId);
            IM_ASSERT(tab != nullptr);
            drag_pos = tab_bar->BarRect.Min + ImVec2(tab->Offset + tab->Width * 0.5f, tab_bar->BarRect.GetHeight() * 0.5f);
        }
        else
#endif
        {
#if IMGUI_VERSION_NUM >= 19071
            const float h = window->TitleBarHeight;
#else
            const float h = window->TitleBarHeight();
#endif
            drag_pos = ImFloor(window->Pos + ImVec2(window->Size.x, h) * 0.5f);
        }

        // If we didn't have to teleport it means we can reach the position already
        if (!WindowTeleportToMakePosVisible(window->ID, drag_pos))
            break;
    }
    return drag_pos;
}

// Click position which should have no windows.
// Default to last mouse viewport if viewport not specified.
void    ImGuiTestContext::MouseMoveToVoid(ImGuiViewport* viewport)
{
    ImGuiContext& g = *UiContext;
    if (IsError())
        return;

    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    LogDebug("MouseMoveToVoid");

#ifdef IMGUI_HAS_VIEWPORT
    if (viewport == nullptr && g.MouseViewport && (g.MouseViewport->Flags & ImGuiViewportFlags_CanHostOtherWindows))
        viewport = g.MouseViewport;
#endif
    if (viewport == nullptr)
        viewport = ImGui::GetMainViewport();

    ImVec2 pos = GetPosOnVoid(viewport); // This may call WindowMove and alter mouse viewport.
#ifdef IMGUI_HAS_VIEWPORT
    MouseSetViewportID(viewport->ID);
#endif
    MouseMoveToPos(pos);
    IM_CHECK(g.HoveredWindow == nullptr);
}

void    ImGuiTestContext::MouseClickOnVoid(int mouse_button, ImGuiViewport* viewport)
{
    if (IsError())
        return;

    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    LogDebug("MouseClickOnVoid %d", mouse_button);
    MouseMoveToVoid(viewport);
    MouseClick(mouse_button);
}

void    ImGuiTestContext::MouseDragWithDelta(ImVec2 delta, ImGuiMouseButton button)
{
    ImGuiContext& g = *UiContext;
    if (IsError())
        return;

    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    LogDebug("MouseDragWithDelta %d (%.1f, %.1f)", button, delta.x, delta.y);

    MouseDown(button);
    MouseMoveToPos(g.IO.MousePos + delta);
    MouseUp(button);
}

// Important: always call MouseWheelX()/MouseWheelY() with an understand that holding Shift will swap axises.
// - On Windows/Linux, this swap is done in ImGui::NewFrame()
// - On OSX, this swap is generally done by the backends
// - In simulated test engine, always assume Windows/Linux behavior as we will swap in ImGuiTestEngine_ApplyInputToImGuiContext()
void    ImGuiTestContext::MouseWheel(ImVec2 delta)
{
    if (IsError())
        return;

    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);

    LogDebug("MouseWheel(%g, %g)", delta.x, delta.y);
    if (EngineIO->ConfigRunSpeed == ImGuiTestRunSpeed_Cinematic)
        SleepStandard();

    float td = 0.0f;
    const float scroll_speed = 15.0f; // Units per second.
    while (delta.x != 0.0f || delta.y != 0.0f)
    {
        ImVec2 scroll;
        if (EngineIO->ConfigRunSpeed == ImGuiTestRunSpeed_Fast)
        {
            scroll = delta;
        }
        else
        {
            td += UiContext->IO.DeltaTime;
            scroll = ImFloor(delta * ImVec2(td, td) * scroll_speed);
        }

        if (scroll.x != 0.0f || scroll.y != 0.0f)
        {
            scroll = ImClamp(scroll, ImVec2(ImMin(delta.x, 0.0f), ImMin(delta.y, 0.0f)), ImVec2(ImMax(delta.x, 0.0f), ImMax(delta.y, 0.0f)));
            Inputs->MouseWheel = scroll;
            delta -= scroll;
            td = 0;
        }
        Yield();
    }
}

void    ImGuiTestContext::KeyDown(ImGuiKeyChord key_chord)
{
    if (IsError())
        return;

    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
#if IMGUI_VERSION_NUM >= 19012
    const char* chord_desc = ImGui::GetKeyChordName(key_chord);
#else
    char chord_desc[32];
    ImGui::GetKeyChordName(key_chord, chord_desc, IM_ARRAYSIZE(chord_desc));
#endif
    LogDebug("KeyDown(%s)", chord_desc);
    if (EngineIO->ConfigRunSpeed == ImGuiTestRunSpeed_Cinematic)
        SleepShort();

    Inputs->Queue.push_back(ImGuiTestInput::ForKeyChord(key_chord, true));
    Yield();
    Yield();
}

void    ImGuiTestContext::KeyUp(ImGuiKeyChord key_chord)
{
    if (IsError())
        return;

    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
#if IMGUI_VERSION_NUM >= 19012
    const char* chord_desc = ImGui::GetKeyChordName(key_chord);
#else
    char chord_desc[32];
    ImGui::GetKeyChordName(key_chord, chord_desc, IM_ARRAYSIZE(chord_desc));
#endif
    LogDebug("KeyUp(%s)", chord_desc);
    if (EngineIO->ConfigRunSpeed == ImGuiTestRunSpeed_Cinematic)
        SleepShort();

    Inputs->Queue.push_back(ImGuiTestInput::ForKeyChord(key_chord, false));
    Yield();
    Yield();
}

void    ImGuiTestContext::KeyPress(ImGuiKeyChord key_chord, int count)
{
    if (IsError())
        return;

    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
#if IMGUI_VERSION_NUM >= 19012
    const char* chord_desc = ImGui::GetKeyChordName(key_chord);
#else
    char chord_desc[32];
    ImGui::GetKeyChordName(key_chord, chord_desc, IM_ARRAYSIZE(chord_desc));
#endif
    LogDebug("KeyPress(%s, %d)", chord_desc, count);
    if (EngineIO->ConfigRunSpeed == ImGuiTestRunSpeed_Cinematic)
        SleepShort();

    while (count > 0)
    {
        count--;
        Inputs->Queue.push_back(ImGuiTestInput::ForKeyChord(key_chord, true));
        if (EngineIO->ConfigRunSpeed == ImGuiTestRunSpeed_Cinematic)
            SleepShort();
        else
            Yield();
        Inputs->Queue.push_back(ImGuiTestInput::ForKeyChord(key_chord, false));
        Yield();

        // Give a frame for items to react
        Yield();
    }
}

void    ImGuiTestContext::KeyHold(ImGuiKeyChord key_chord, float time)
{
    if (IsError())
        return;

    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
#if IMGUI_VERSION_NUM >= 19012
    const char* chord_desc = ImGui::GetKeyChordName(key_chord);
#else
    char chord_desc[32];
    ImGui::GetKeyChordName(key_chord, chord_desc, IM_ARRAYSIZE(chord_desc));
#endif
    LogDebug("KeyHold(%s, %.2f sec)", chord_desc, time);
    if (EngineIO->ConfigRunSpeed == ImGuiTestRunSpeed_Cinematic)
        SleepStandard();

    Inputs->Queue.push_back(ImGuiTestInput::ForKeyChord(key_chord, true));
    SleepNoSkip(time, 1 / 100.0f);
    Inputs->Queue.push_back(ImGuiTestInput::ForKeyChord(key_chord, false));
    Yield(); // Give a frame for items to react
}

// No extra yield
void    ImGuiTestContext::KeySetEx(ImGuiKeyChord key_chord, bool is_down, float time)
{
    if (IsError())
        return;

    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
#if IMGUI_VERSION_NUM >= 19012
    const char* chord_desc = ImGui::GetKeyChordName(key_chord);
#else
    char chord_desc[32];
    ImGui::GetKeyChordName(key_chord, chord_desc, IM_ARRAYSIZE(chord_desc));
#endif
    LogDebug("KeySetEx(%s, is_down=%d, time=%.f)", chord_desc, is_down, time);
    Inputs->Queue.push_back(ImGuiTestInput::ForKeyChord(key_chord, is_down));
    if (time > 0.0f)
        SleepNoSkip(time, 1.0f / 100.0f);
}

void    ImGuiTestContext::KeyChars(const char* chars)
{
    if (IsError())
        return;

    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    LogDebug("KeyChars('%s')", chars);
    if (EngineIO->ConfigRunSpeed == ImGuiTestRunSpeed_Cinematic)
        SleepStandard();

    while (*chars)
    {
        unsigned int c = 0;
        int bytes_count = ImTextCharFromUtf8(&c, chars, nullptr);
        chars += bytes_count;
        if (c > 0 && c <= 0xFFFF)
            Inputs->Queue.push_back(ImGuiTestInput::ForChar((ImWchar)c));

        if (EngineIO->ConfigRunSpeed != ImGuiTestRunSpeed_Fast)
            Sleep(1.0f / EngineIO->TypingSpeed);
    }
    Yield();
}

void    ImGuiTestContext::KeyCharsAppend(const char* chars)
{
    if (IsError())
        return;

    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    LogDebug("KeyCharsAppend('%s')", chars);
    KeyPress(ImGuiKey_End);
    KeyChars(chars);
}

void    ImGuiTestContext::KeyCharsAppendEnter(const char* chars)
{
    if (IsError())
        return;

    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    LogDebug("KeyCharsAppendEnter('%s')", chars);
    KeyPress(ImGuiKey_End);
    KeyChars(chars);
    KeyPress(ImGuiKey_Enter);
}

void    ImGuiTestContext::KeyCharsReplace(const char* chars)
{
    if (IsError())
        return;

    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    LogDebug("KeyCharsReplace('%s')", chars);
#if IMGUI_VERSION_NUM < 19063
    KeyPress(ImGuiKey_A | ImGuiMod_Shortcut);
#else
    KeyPress(ImGuiKey_A | ImGuiMod_Ctrl);
#endif
    if (chars[0])
        KeyChars(chars);
    else
        KeyPress(ImGuiKey_Delete);
}

void    ImGuiTestContext::KeyCharsReplaceEnter(const char* chars)
{
    if (IsError())
        return;

    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    LogDebug("KeyCharsReplaceEnter('%s')", chars);
#if IMGUI_VERSION_NUM < 19063
    KeyPress(ImGuiKey_A | ImGuiMod_Shortcut);
#else
    KeyPress(ImGuiKey_A | ImGuiMod_Ctrl);
#endif
    if (chars[0])
        KeyChars(chars);
    else
        KeyPress(ImGuiKey_Delete);
    KeyPress(ImGuiKey_Enter);
}

// depth = 1 -> immediate child of 'parent' in ID Stack
 // FIXME: Configurable filter for InLayerMask. Perhaps we can expose a GatherItemEx() that takes a ImGuiTestGatherTask struct as input.
void    ImGuiTestContext::GatherItems(ImGuiTestItemList* out_list, ImGuiTestRef parent, int depth)
{
    IM_ASSERT(out_list != nullptr);
    IM_ASSERT(depth > 0 || depth == -1);

    if (IsError())
        return;

    ImGuiTestGatherTask* task = &Engine->GatherTask;
    IM_ASSERT(task->InParentID == 0);
    IM_ASSERT(task->LastItemInfo == nullptr);

    // Register gather tasks
    if (depth == -1)
        depth = 99;
    if (parent.ID == 0)
        parent.ID = GetID(parent);
    task->InParentID = parent.ID;
    task->InMaxDepth = depth;
    task->InLayerMask = (1 << ImGuiNavLayer_Main);
    task->OutList = out_list;

    // Keep running while gathering
    // The corresponding hook is ItemAdd() -> ImGuiTestEngineHook_ItemAdd() -> ImGuiTestEngineHook_ItemAdd_GatherTask()
    const int begin_gather_size = out_list->GetSize();
    while (true)
    {
        const int begin_gather_size_for_frame = out_list->GetSize();
        Yield();
        const int end_gather_size_for_frame = out_list->GetSize();
        if (begin_gather_size_for_frame == end_gather_size_for_frame)
            break;
    }
    const int end_gather_size = out_list->GetSize();

    // FIXME-TESTS: To support filter we'd need to process the list here,
    // Because ImGuiTestItemList is a pool (ImVector + map ID->index) we'll need to filter, rewrite, rebuild map

    ImGuiTestItemInfo parent_item = ItemInfo(parent, ImGuiTestOpFlags_NoError);
    LogDebug("GatherItems from %s, %d deep: found %d items.", ImGuiTestRefDesc(parent, parent_item).c_str(), depth, end_gather_size - begin_gather_size);

    task->Clear();
}

// Supported values for ImGuiTestOpFlags:
// - ImGuiTestOpFlags_NoAutoOpenFullPath
// - ImGuiTestOpFlags_NoError
void    ImGuiTestContext::ItemAction(ImGuiTestAction action, ImGuiTestRef ref, ImGuiTestOpFlags flags, void* action_arg)
{
    if (IsError())
        return;

    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);

    // [DEBUG] Breakpoint
    //if (ref.ID == 0x0d4af068)
    //    printf("");

    // FIXME-TESTS: Fix that stuff
    const bool is_wildcard = ref.Path != nullptr && strstr(ref.Path, "**/") != 0;
    if (is_wildcard)
    {
        // This is a fragile way to avoid some ambiguities, we're relying on expected action to further filter by status flags.
        // These flags are not cleared by ItemInfo() because ItemAction() may call ItemInfo() again to get same item and thus it
        // needs these flags to remain in place.
        if (action == ImGuiTestAction_Check || action == ImGuiTestAction_Uncheck)
            Engine->FindByLabelTask.InFilterItemStatusFlags = ImGuiItemStatusFlags_Checkable;
        else if (action == ImGuiTestAction_Open || action == ImGuiTestAction_Close)
            Engine->FindByLabelTask.InFilterItemStatusFlags = ImGuiItemStatusFlags_Openable;
    }

    // Find item
    ImGuiTestItemInfo item;
    if (flags & ImGuiTestOpFlags_NoAutoOpenFullPath)
        item = ItemInfo(ref, (flags & ImGuiTestOpFlags_NoError));
    else
        item = ItemInfoOpenFullPath(ref, (flags & ImGuiTestOpFlags_NoError));

    ImGuiTestRefDesc desc(ref, item);
    LogDebug("Item%s %s%s", GetActionName(action), desc.c_str(), (InputMode == ImGuiInputSource_Mouse) ? "" : " (w/ Nav)");
    if (item.ID == 0)
    {
        if (flags & ImGuiTestOpFlags_NoError)
            LogDebug("Action skipped: Item doesn't exist + used ImGuiTestOpFlags_NoError.");
        return;
    }

    // Automatically uncollapse by default
    if (item.Window && !(OpFlags & ImGuiTestOpFlags_NoAutoUncollapse))
        WindowCollapse(item.Window->ID, false);

    if (action == ImGuiTestAction_Hover)
    {
        MouseMove(ref, flags);
    }
    if (action == ImGuiTestAction_Click || action == ImGuiTestAction_DoubleClick)
    {
        if (InputMode == ImGuiInputSource_Mouse)
        {
            const int mouse_button = (int)(intptr_t)action_arg;
            IM_ASSERT(mouse_button >= 0 && mouse_button < ImGuiMouseButton_COUNT);
            MouseMove(ref, flags);
            if (action == ImGuiTestAction_DoubleClick)
                MouseDoubleClick(mouse_button);
            else
                MouseClick(mouse_button);
        }
        else
        {
            action = ImGuiTestAction_NavActivate;
        }
    }

    if (action == ImGuiTestAction_NavActivate)
    {
        IM_ASSERT(action_arg == nullptr); // Unused
        NavMoveTo(ref);
        NavActivate();
        if (action == ImGuiTestAction_DoubleClick)
            IM_ASSERT(0);
    }
    else if (action == ImGuiTestAction_Input)
    {
        IM_ASSERT(action_arg == nullptr); // Unused
        if (InputMode == ImGuiInputSource_Mouse)
        {
            MouseMove(ref, flags);
            KeyDown(ImGuiMod_Ctrl);
            MouseClick(0);
            KeyUp(ImGuiMod_Ctrl);
        }
        else
        {
            NavMoveTo(ref);
            NavInput();
        }
    }
    else if (action == ImGuiTestAction_Open)
    {
        IM_ASSERT(action_arg == nullptr); // Unused
        if ((item.StatusFlags & ImGuiItemStatusFlags_Opened) == 0)
        {
            MouseMove(ref, flags);

            // Some item may open just by hovering, give them that chance
            item = ItemInfo(item.ID);
            if ((item.StatusFlags & ImGuiItemStatusFlags_Opened) == 0)
            {
                MouseClick(0);
                item = ItemInfo(item.ID);
                if ((item.StatusFlags & ImGuiItemStatusFlags_Opened) == 0)
                {
                    MouseDoubleClick(0); // Attempt a double-click // FIXME-TESTS: let's not start doing those fuzzy things..
                    item = ItemInfo(item.ID);
                    if ((item.StatusFlags & ImGuiItemStatusFlags_Opened) == 0)
                        IM_ERRORF_NOHDR("Unable to Open item: '%s' in '%s'", desc.c_str(), item.Window ? item.Window->Name : "N/A");
                }
            }
            //Yield();
        }
    }
    else if (action == ImGuiTestAction_Close)
    {
        IM_ASSERT(action_arg == nullptr); // Unused
        if ((item.StatusFlags & ImGuiItemStatusFlags_Opened) != 0)
        {
            ItemClick(ref, 0, flags);
            item = ItemInfo(item.ID);
            if ((item.StatusFlags & ImGuiItemStatusFlags_Opened) != 0)
            {
                ItemDoubleClick(ref, flags); // Attempt a double-click
                // FIXME-TESTS: let's not start doing those fuzzy things.. widget should give direction of how to close/open... e.g. do you we close a TabItem?
                item = ItemInfo(item.ID);
                if ((item.StatusFlags & ImGuiItemStatusFlags_Opened) != 0)
                    IM_ERRORF_NOHDR("Unable to Close item: %s", ImGuiTestRefDesc(ref, item).c_str());
            }
            Yield();
        }
    }
    else if (action == ImGuiTestAction_Check)
    {
        IM_ASSERT(action_arg == nullptr); // Unused
        if ((item.StatusFlags & ImGuiItemStatusFlags_Checkable) && !(item.StatusFlags & ImGuiItemStatusFlags_Checked))
        {
            ItemClick(ref, 0, flags);
        }
        ItemVerifyCheckedIfAlive(ref, true); // We can't just IM_ASSERT(ItemIsChecked()) because the item may disappear and never update its StatusFlags any more!
    }
    else if (action == ImGuiTestAction_Uncheck)
    {
        IM_ASSERT(action_arg == nullptr); // Unused
        if ((item.StatusFlags & ImGuiItemStatusFlags_Checkable) && (item.StatusFlags & ImGuiItemStatusFlags_Checked))
        {
            ItemClick(ref, 0, flags);
        }
        ItemVerifyCheckedIfAlive(ref, false); // We can't just IM_ASSERT(ItemIsChecked()) because the item may disappear and never update its StatusFlags any more!
    }

    //if (is_wildcard)
        Engine->FindByLabelTask.InFilterItemStatusFlags = ImGuiItemStatusFlags_None;
}

void    ImGuiTestContext::ItemActionAll(ImGuiTestAction action, ImGuiTestRef ref_parent, const ImGuiTestActionFilter* filter)
{
    int max_depth = filter ? filter->MaxDepth : -1;
    if (max_depth == -1)
        max_depth = 99;
    int max_passes = filter ? filter->MaxPasses : -1;
    if (max_passes == -1)
        max_passes = 99;
    IM_ASSERT(max_depth > 0 && max_passes > 0);

    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    LogDebug("ItemActionAll() %s", GetActionName(action));

    if (!ref_parent.IsEmpty())
    {
        // Open parent's parents
        ImGuiTestItemInfo parent_info = ItemInfoOpenFullPath(ref_parent);
        if (parent_info.ID != 0)
        {
            // Open parent
            if (action == ImGuiTestAction_Open)
                if ((parent_info.StatusFlags & ImGuiItemStatusFlags_Openable) && (parent_info.ItemFlags & ImGuiItemFlags_Disabled) == 0)
                    ItemOpen(ref_parent, ImGuiTestOpFlags_NoError);
        }
    }

    // Find child items
    int actioned_total = 0;
    for (int pass = 0; pass < max_passes; pass++)
    {
        ImGuiTestItemList items;
        GatherItems(&items, ref_parent, max_depth);
        //LogItemList(&items);

        // Find deep most items
        int highest_depth = -1;
        if (action == ImGuiTestAction_Close)
            for (auto& item : items)
                if ((item.StatusFlags & ImGuiItemStatusFlags_Openable) && (item.StatusFlags & ImGuiItemStatusFlags_Opened)) // Not checking Disabled state here
                    highest_depth = ImMax(highest_depth, item.Depth);

        const int actioned_total_at_beginning_of_pass = actioned_total;

        // Process top-to-bottom in most cases
        int scan_start = 0;
        int scan_end = items.GetSize();
        int scan_dir = +1;
        if (action == ImGuiTestAction_Close)
        {
            // Close bottom-to-top because
            // 1) it is more likely to handle same-depth parent/child relationship better (e.g. CollapsingHeader)
            // 2) it gives a nicer sense of symmetry with the corresponding open operation.
            scan_start = items.GetSize() - 1;
            scan_end = -1;
            scan_dir = -1;
        }

        int processed_count_per_depth[8];
        memset(processed_count_per_depth, 0, sizeof(processed_count_per_depth));

        for (int n = scan_start; n != scan_end; n += scan_dir)
        {
            if (IsError())
                break;

            const ImGuiTestItemInfo& item = *items[n];

            if (filter && filter->RequireAllStatusFlags != 0)
                if ((item.StatusFlags & filter->RequireAllStatusFlags) != filter->RequireAllStatusFlags)
                    continue;

            if (filter && filter->RequireAnyStatusFlags != 0)
                if ((item.StatusFlags & filter->RequireAnyStatusFlags) != 0)
                    continue;

            if (filter && filter->MaxItemCountPerDepth != nullptr)
            {
                if (item.Depth < IM_ARRAYSIZE(processed_count_per_depth))
                {
                    if (processed_count_per_depth[item.Depth] >= filter->MaxItemCountPerDepth[item.Depth])
                        continue;
                    processed_count_per_depth[item.Depth]++;
                }
            }

            switch (action)
            {
            case ImGuiTestAction_Hover:
            case ImGuiTestAction_Click:
                ItemAction(action, item.ID);
                actioned_total++;
                break;
            case ImGuiTestAction_Check:
                if ((item.StatusFlags & ImGuiItemStatusFlags_Checkable) && !(item.StatusFlags & ImGuiItemStatusFlags_Checked))
                    if ((item.ItemFlags & ImGuiItemFlags_Disabled) == 0)
                    {
                        ItemAction(action, item.ID);
                        actioned_total++;
                    }
                break;
            case ImGuiTestAction_Uncheck:
                if ((item.StatusFlags & ImGuiItemStatusFlags_Checkable) && (item.StatusFlags & ImGuiItemStatusFlags_Checked))
                    if ((item.ItemFlags & ImGuiItemFlags_Disabled) == 0)
                    {
                        ItemAction(action, item.ID);
                        actioned_total++;
                    }
                break;
            case ImGuiTestAction_Open:
                if ((item.StatusFlags & ImGuiItemStatusFlags_Openable) && !(item.StatusFlags & ImGuiItemStatusFlags_Opened))
                    if ((item.ItemFlags & ImGuiItemFlags_Disabled) == 0)
                    {
                        ItemAction(action, item.ID);
                        actioned_total++;
                    }
                break;
            case ImGuiTestAction_Close:
                if (item.Depth == highest_depth && (item.StatusFlags & ImGuiItemStatusFlags_Openable) && (item.StatusFlags & ImGuiItemStatusFlags_Opened))
                    if ((item.ItemFlags & ImGuiItemFlags_Disabled) == 0)
                    {
                        ItemClose(item.ID);
                        actioned_total++;
                    }
                break;
            default:
                IM_ASSERT(0);
            }
        }

        if (IsError())
            break;

        if (action == ImGuiTestAction_Hover)
            break;
        if (actioned_total_at_beginning_of_pass == actioned_total)
            break;
    }
    LogDebug("%s %d items in total!", GetActionVerb(action), actioned_total);
}

void    ImGuiTestContext::ItemOpenAll(ImGuiTestRef ref_parent, int max_depth, int max_passes)
{
    ImGuiTestActionFilter filter;
    filter.MaxDepth = max_depth;
    filter.MaxPasses = max_passes;
    ItemActionAll(ImGuiTestAction_Open, ref_parent, &filter);
}

void    ImGuiTestContext::ItemCloseAll(ImGuiTestRef ref_parent, int max_depth, int max_passes)
{
    ImGuiTestActionFilter filter;
    filter.MaxDepth = max_depth;
    filter.MaxPasses = max_passes;
    ItemActionAll(ImGuiTestAction_Close, ref_parent, &filter);
}

void    ImGuiTestContext::ItemInputValue(ImGuiTestRef ref, int value)
{
    char buf[32];
    ImFormatString(buf, IM_ARRAYSIZE(buf), "%d", value);
    ItemInput(ref);
    KeyCharsReplaceEnter(buf);
}

void    ImGuiTestContext::ItemInputValue(ImGuiTestRef ref, float value)
{
    char buf[32];
    ImFormatString(buf, IM_ARRAYSIZE(buf), "%f", value);
    ItemInput(ref);
    KeyCharsReplaceEnter(buf);
}

void    ImGuiTestContext::ItemInputValue(ImGuiTestRef ref, const char* value)
{
    ItemInput(ref);
    KeyCharsReplaceEnter(value);
}

// Supported values for ImGuiTestOpFlags:
// - ImGuiTestOpFlags_NoError
bool    ImGuiTestContext::ItemReadAsScalar(ImGuiTestRef ref, ImGuiDataType data_type, void* out_data, ImGuiTestOpFlags flags)
{
    if (IsError())
        return false;

    const ImGuiDataTypeInfo* data_type_info = ImGui::DataTypeGetInfo(data_type);
    const ImGuiTestOpFlags SUPPORTED_FLAGS = ImGuiTestOpFlags_NoError;
    IM_ASSERT((flags & ~SUPPORTED_FLAGS) == 0);

    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    LogDebug("ItemSelectReadValue '%s' 0x%08X as %s", ref.Path ? ref.Path : "nullptr", ref.ID, data_type_info->Name);
    IM_CHECK_SILENT_RETV(out_data != nullptr, false);

    Str256 backup_clipboard = ImGui::GetClipboardText();

    ItemInput(ref, flags);
#if IMGUI_VERSION_NUM < 19063
    KeyPress(ImGuiKey_A | ImGuiMod_Shortcut);
    KeyPress(ImGuiKey_C | ImGuiMod_Shortcut);   // Copy to clipboard
#else
    KeyPress(ImGuiKey_A | ImGuiMod_Ctrl);
    KeyPress(ImGuiKey_C | ImGuiMod_Ctrl);       // Copy to clipboard
#endif
    KeyPress(ImGuiKey_Enter);

    const char* clipboard = ImGui::GetClipboardText();
    bool ret = ImGui::DataTypeApplyFromText(clipboard, data_type, out_data, data_type_info->ScanFmt);
    if (ret == false)
    {
        if ((flags & ImGuiTestOpFlags_NoError) == 0)
        {
            LogError("Unable to parse buffer '%s' as %s", clipboard, data_type_info->Name);
            IM_CHECK_RETV(ret, false);
        }
    }
    ImGui::SetClipboardText(backup_clipboard.c_str());

    return ret;
}

int     ImGuiTestContext::ItemReadAsInt(ImGuiTestRef ref)
{
    int v = 0;
    ItemReadAsScalar(ref, ImGuiDataType_S32, (void*)&v);
    return v;
}

float   ImGuiTestContext::ItemReadAsFloat(ImGuiTestRef ref)
{
    float v = 0.0f;
    ItemReadAsScalar(ref, ImGuiDataType_Float, (void*)&v);
    return v;
}

// Convenient wrapper for ItemSelectAndReadString using our own storage
// Returned pointer is only valid until next call to same function.
const char* ImGuiTestContext::ItemReadAsString(ImGuiTestRef ref)
{
    if (IsError())
        return "";

    size_t required_1 = ItemReadAsString(ref, TempString.Data, TempString.capacity());
    if ((int)required_1 > TempString.capacity())
    {
        TempString.reserve((int)required_1);
        size_t required_2 = ItemReadAsString(ref, TempString.Data, TempString.capacity());
        IM_CHECK_SILENT_RETV(required_1 == required_2, "");
    }
    return TempString.Data;
}

// return required buffer size to store output value (#26, #66)
// write up to out_buf_size to out_buf, always zero-terminated.
// if (out_buf == nulltr) || (out_buf_size < return value), then you want to.
// You'd probably want to wrap this in a helper for your preferred string type.
size_t  ImGuiTestContext::ItemReadAsString(ImGuiTestRef ref, char* out_buf, size_t out_buf_size)
{
    if (IsError())
    {
        if (out_buf_size > 0)
            out_buf[0] = 0;
        return 0;
    }

    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    LogDebug("ItemSelectAndReadString '%s' 0x%08X as string", ref.Path ? ref.Path : "nullptr", ref.ID);
    IM_CHECK_SILENT_RETV(out_buf != nullptr || out_buf_size == 0, false);

    Str256 backup_clipboard = ImGui::GetClipboardText();

    ItemInput(ref);
#if IMGUI_VERSION_NUM < 19063
    KeyPress(ImGuiKey_A | ImGuiMod_Shortcut);
    KeyPress(ImGuiKey_C | ImGuiMod_Shortcut);   // Copy to clipboard
#else
    KeyPress(ImGuiKey_A | ImGuiMod_Ctrl);
    KeyPress(ImGuiKey_C | ImGuiMod_Ctrl);       // Copy to clipboard
#endif
    KeyPress(ImGuiKey_Enter);

    const char* value_str = ImGui::GetClipboardText();
    size_t value_required_buf_size = strlen(value_str) + 1;

    if (out_buf_size > 0)
        ImFormatString(out_buf, out_buf_size, "%.*s", (int)ImMax(value_required_buf_size, out_buf_size), value_str);

    ImGui::SetClipboardText(backup_clipboard.c_str());

    return value_required_buf_size;
}

void    ImGuiTestContext::ItemHold(ImGuiTestRef ref, float time)
{
    if (IsError())
        return;

    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    ImGuiTestRefDesc desc(ref);
    LogDebug("ItemHold %s", desc.c_str());

    MouseMove(ref);

    Yield();
    Inputs->MouseButtonsValue = (1 << 0);
    Sleep(time);
    Inputs->MouseButtonsValue = 0;
    Yield();
}

void    ImGuiTestContext::ItemHoldForFrames(ImGuiTestRef ref, int frames)
{
    if (IsError())
        return;

    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    ImGuiTestRefDesc desc(ref);
    LogDebug("ItemHoldForFrames %s", desc.c_str());

    MouseMove(ref);
    Yield();
    Inputs->MouseButtonsValue = (1 << 0);
    Yield(frames);
    Inputs->MouseButtonsValue = 0;
    Yield();
}

// Used to test opening containers (TreeNode, Tabs) while dragging a payload.
// Hold for 1 second and then release mouse button.
void    ImGuiTestContext::ItemDragOverAndHold(ImGuiTestRef ref_src, ImGuiTestRef ref_dst)
{
    if (IsError())
        return;

    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    ImGuiTestItemInfo item_src = ItemInfo(ref_src);
    ImGuiTestItemInfo item_dst = ItemInfo(ref_dst);
    ImGuiTestRefDesc desc_src(ref_src, item_src);
    ImGuiTestRefDesc desc_dst(ref_dst, item_dst);
    LogDebug("ItemDragOverAndHold %s to %s", desc_src.c_str(), desc_dst.c_str());

    MouseMove(ref_src, ImGuiTestOpFlags_NoCheckHoveredId);
    SleepStandard();
    MouseDown(0);

    // Enforce lifting drag threshold even if both item are exactly at the same location.
    // Don't lift the threshold in the same frame as calling MouseDown() as it can trigger two actions.
    Yield();
    MouseLiftDragThreshold();
    MouseMove(ref_dst, ImGuiTestOpFlags_NoCheckHoveredId);

    SleepNoSkip(1.0f, 1.0f / 10.0f);
    MouseUp(0);
}

void    ImGuiTestContext::ItemDragAndDrop(ImGuiTestRef ref_src, ImGuiTestRef ref_dst, ImGuiMouseButton button)
{
    if (IsError())
        return;

    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    ImGuiTestItemInfo item_src = ItemInfo(ref_src);
    ImGuiTestItemInfo item_dst = ItemInfo(ref_dst);
    ImGuiTestRefDesc desc_src(ref_src, item_src);
    ImGuiTestRefDesc desc_dst(ref_dst, item_dst);
    LogDebug("ItemDragAndDrop %s to %s", desc_src.c_str(), desc_dst.c_str());

    // Try to keep destination window above other windows. MouseMove() operation will avoid focusing destination window
    // as that may steal ActiveID and break operation.
    // FIXME-TESTS: This does not handle a case where source and destination windows overlap.
    if (item_dst.Window != nullptr)
        WindowBringToFront(item_dst.Window->ID);

    // Use item_src/item_dst instead of ref_src/ref_dst so references with e.g. //$FOCUSED are latched once in the ItemInfo() call.
    MouseMove(item_src.ID, ImGuiTestOpFlags_NoCheckHoveredId);
    SleepStandard();
    MouseDown(button);

    // Enforce lifting drag threshold even if both item are exactly at the same location.
    // Don't lift the threshold in the same frame as calling MouseDown() as it can trigger two actions.
    Yield();
    MouseLiftDragThreshold();
    MouseMove(item_dst.ID, ImGuiTestOpFlags_NoCheckHoveredId | ImGuiTestOpFlags_NoFocusWindow);

    SleepStandard();
    MouseUp(button);
}

void    ImGuiTestContext::ItemDragWithDelta(ImGuiTestRef ref_src, ImVec2 pos_delta)
{
    if (IsError())
        return;

    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    ImGuiTestItemInfo item_src = ItemInfo(ref_src);
    ImGuiTestRefDesc desc_src(ref_src, item_src);
    LogDebug("ItemDragWithDelta %s to (%f, %f)", desc_src.c_str(), pos_delta.x, pos_delta.y);

    MouseMove(ref_src, ImGuiTestOpFlags_NoCheckHoveredId);
    SleepStandard();
    MouseDown(0);

    MouseMoveToPos(UiContext->IO.MousePos + pos_delta);
    SleepStandard();
    MouseUp(0);
}

bool    ImGuiTestContext::ItemExists(ImGuiTestRef ref)
{
    ImGuiTestItemInfo item = ItemInfo(ref, ImGuiTestOpFlags_NoError);
    return item.ID != 0;
}

// May want to add support for ImGuiTestOpFlags_NoError if item does not exist?
bool    ImGuiTestContext::ItemIsChecked(ImGuiTestRef ref)
{
    ImGuiTestItemInfo item = ItemInfo(ref);
    return (item.StatusFlags & ImGuiItemStatusFlags_Checked) != 0;
}

// May want to add support for ImGuiTestOpFlags_NoError if item does not exist?
bool    ImGuiTestContext::ItemIsOpened(ImGuiTestRef ref)
{
    ImGuiTestItemInfo item = ItemInfo(ref);
    return (item.StatusFlags & ImGuiItemStatusFlags_Opened) != 0;
}

void    ImGuiTestContext::ItemVerifyCheckedIfAlive(ImGuiTestRef ref, bool checked)
{
    // This is designed to deal with disappearing items which will not update their state,
    // e.g. a checkable menu item in a popup which closes when checked.
    // Otherwise ItemInfo() data is preserved for an additional frame.
    Yield();
    ImGuiTestItemInfo item = ItemInfo(ref, ImGuiTestOpFlags_NoError);
    if (item.ID == 0)
        return;
    if (item.TimestampMain + 1 >= ImGuiTestEngine_GetFrameCount(Engine) && item.TimestampStatus == item.TimestampMain)
        IM_CHECK_SILENT(((item.StatusFlags & ImGuiItemStatusFlags_Checked) != 0) == checked);
}

// FIXME-TESTS: Could this be handled by ItemClose()?
void    ImGuiTestContext::TabClose(ImGuiTestRef ref)
{
    if (IsError())
        return;

    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    ImGuiTestRefDesc desc(ref);
    LogDebug("TabClose %s", desc.c_str());

    // Move into first, then click close button as it appears
    MouseMove(ref);
    ImGuiTestRef backup_ref = GetRef();
    SetRef(GetID(ref));
    ItemClick("#CLOSE");
    SetRef(backup_ref);
}

bool    ImGuiTestContext::TabBarCompareOrder(ImGuiTabBar* tab_bar, const char** tab_order)
{
    if (IsError())
        return false;

    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    LogDebug("TabBarCompareOrder");
    IM_CHECK_SILENT_RETV(tab_bar != nullptr, false);

    // Display
    char buf[256];
    char* buf_end = buf + IM_ARRAYSIZE(buf);

    char* p = buf;
    for (int i = 0; i < tab_bar->Tabs.Size; i++)
        p += ImFormatString(p, buf_end - p, "%s\"%s\"", i ? ", " : " ", ImGui::TabBarGetTabName(tab_bar, &tab_bar->Tabs[i]));
    LogDebug("  Current  {%s }", buf);

    p = buf;
    for (int i = 0; tab_order[i] != nullptr; i++)
        p += ImFormatString(p, buf_end - p, "%s\"%s\"", i ? ", " : " ", tab_order[i]);
    LogDebug("  Expected {%s }", buf);

    // Compare
    for (int i = 0; tab_order[i] != nullptr; i++)
    {
        if (i >= tab_bar->Tabs.Size)
            return false;
        const char* current = ImGui::TabBarGetTabName(tab_bar, &tab_bar->Tabs[i]);
        const char* expected = tab_order[i];
        if (strcmp(current, expected) != 0)
            return false;
    }
    return true;
}

// Automatically insert "##MenuBar" between window and menus.
// Automatically open and navigate sub-menus
// FIXME: Currently assume that any path after the window are sub-menus.
void    ImGuiTestContext::MenuAction(ImGuiTestAction action, ImGuiTestRef ref)
{
    if (IsError())
        return;

    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    ImGuiTestRefDesc desc(ref);
    LogDebug("MenuAction %s", desc.c_str());

    IM_ASSERT(ref.Path != nullptr);

    // MenuAction() doesn't support **/ in most case it would be equivalent to opening all menus to "search".
    // [01] Works:
    //   MenuClick("File/New"):
    // [02] Works:
    //   MenuClick("File");
    //   MenuClick("File/New");
    // [03] Works:
    //   MenuClick("File");
    //   ItemClick("**/New");
    // [04] Doesn't work: (may work in the future)
    //   MenuClick("File");
    //   MenuClick("**/New");
    // [05] Doesn't work: (unlikely to ever work)
    //   MenuClick("**/New");
    if (strncmp(ref.Path, "**/", 3) == 0)
    {
        LogError("\"**/\" is not yet supported by MenuAction().");
        return;
    }

    int depth = 0;
    const char* path = ref.Path;
    const char* path_end = path + strlen(path);

    ImGuiWindow* ref_window = nullptr;
    if ((path[0] == '/' && path[1] == '/') || (RefID == 0))
    {
        const char* end = strstr(path + 2, "/");
        IM_CHECK_SILENT(end != nullptr); // Menu interaction without any menus specified in ref.
        Str64 window_name;
        window_name.append(path, end);
        ref_window = GetWindowByRef(GetID(window_name.c_str()));
        path = end + 1;
        if (ref_window == nullptr)
            LogError("MenuAction: missing ref window (invalid name \"//%s\" ?", window_name.c_str());
    }
    else
    {
        ref_window = GetWindowByRef(RefID);
        if (ref_window == nullptr)
            LogError("MenuAction: missing ref window (invalid SetRef value?)");
    }
    IM_CHECK_SILENT(ref_window != nullptr);  // A ref window must always be set

    ImGuiWindow* current_window = ref_window;
    Str128 buf;
    while (path < path_end && !IsError())
    {
        const char* p = ImStrchrRangeWithEscaping(path, path_end, '/');
        if (p == nullptr)
            p = path_end;

        const bool is_target_item = (p == path_end);
#if IMGUI_VERSION_NUM >= 19174
        if (current_window->Flags & ImGuiWindowFlags_MenuBar)
            buf.setf("//%s/##MenuBar/%.*s", current_window->Name, (int)(p - path), path);    // Click menu in menu bar
#else
        if (current_window->Flags & ImGuiWindowFlags_MenuBar)
            buf.setf("//%s/##menubar/%.*s", current_window->Name, (int)(p - path), path);    // Click menu in menu bar
#endif
        else
            buf.setf("//%s/%.*s", current_window->Name, (int)(p - path), path);              // Click sub menu in its own window

#if IMGUI_VERSION_NUM < 18520
        if (depth == 0 && (current_window->Flags & ImGuiWindowFlags_Popup))
            depth++;
#endif

        // Timestamps updated in hooks submitted in ui code.
        ImGuiTestItemInfo item = ItemInfo(buf.c_str());
        IM_CHECK_SILENT(item.ID != 0);
        if (item.TimestampStatus < UiContext->FrameCount)
        {
            Yield();
            item = ItemInfo(buf.c_str());
            IM_CHECK_SILENT(item.ID != 0);
        }

        if ((item.StatusFlags & ImGuiItemStatusFlags_Opened) == 0) // Open menus can be ignored completely.
        {
            // We cannot move diagonally to a menu item because depending on the angle and other items we cross on our path we could close our target menu.
            // First move horizontally into the menu, then vertically!
            if (depth > 0)
            {
                MouseSetViewport(item.Window);
                if (Inputs->MousePosValue.x <= item.RectFull.Min.x || Inputs->MousePosValue.x >= item.RectFull.Max.x)
                    MouseMoveToPos(ImVec2(item.RectFull.GetCenter().x, Inputs->MousePosValue.y));
                if (Inputs->MousePosValue.y <= item.RectFull.Min.y || Inputs->MousePosValue.y >= item.RectFull.Max.y)
                    MouseMoveToPos(ImVec2(Inputs->MousePosValue.x, item.RectFull.GetCenter().y));
            }

            if (is_target_item)
            {
                // Final item
                ItemAction(action, buf.c_str());
                break;
            }
            else
            {
                // Then aim at the menu item. Menus may be navigated by holding mouse button down by hovering a menu.
                ItemAction(Inputs->MouseButtonsValue ? ImGuiTestAction_Hover : ImGuiTestAction_Click, buf.c_str());
            }
        }
#if IMGUI_VERSION_NUM < 19187
        current_window = GetWindowByRef(Str16f("//##Menu_%02d", depth).c_str());
#else
        current_window = GetWindowByRef(Str16f("//###Menu_%02d", depth).c_str());
#endif
        IM_CHECK_SILENT(current_window != nullptr);

        path = p + 1;
        depth++;
    }
}

void    ImGuiTestContext::MenuActionAll(ImGuiTestAction action, ImGuiTestRef ref_parent)
{
    ImGuiTestItemList items;
    MenuAction(ImGuiTestAction_Open, ref_parent);
    GatherItems(&items, "//$FOCUSED", 1);
    //LogItemList(&items);

    for (auto item : items)
    {
        MenuAction(ImGuiTestAction_Open, ref_parent); // We assume that every interaction will close the menu again

        if (action == ImGuiTestAction_Check || action == ImGuiTestAction_Uncheck)
        {
            ImGuiTestItemInfo info2 = ItemInfo(item.ID); // refresh info
            if ((info2.ItemFlags & ImGuiItemFlags_Disabled) != 0) // FIXME: Report disabled state in log? Make that optional?
                continue;
            if ((info2.StatusFlags & ImGuiItemStatusFlags_Checkable) == 0)
                continue;
        }

        ItemAction(action, item.ID);
    }
}

static bool IsWindowACombo(ImGuiWindow* window)
{
    if ((window->Flags & ImGuiWindowFlags_Popup) == 0)
        return false;
    if (strncmp(window->Name, "##Combo_", strlen("##Combo_")) != 0)
        return false;
    return true;
}

// Usage: ComboClick("ComboName/ItemName");
void    ImGuiTestContext::ComboClick(ImGuiTestRef ref)
{
    if (IsError())
        return;

    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    ImGuiTestRefDesc desc(ref);
    LogDebug("ComboClick %s", desc.c_str());

    IM_ASSERT(ref.Path != nullptr); // Should always pass an actual path, not an ID.

    const char* path = ref.Path;
    const char* path_end = path + strlen(path);
    const char* p = ImStrchrRangeWithEscaping(path, path_end, '/');
    if (p == nullptr)
    {
        LogError("Error: path should contains a / separator, e.g. ComboClick(\"mycombo/myitem\")");
        IM_CHECK(p != nullptr);
    }

    Str128f combo_popup_buf = Str128f("%.*s", (int)(p-path), path);
    ItemClick(combo_popup_buf.c_str());

    ImGuiWindow* popup = GetWindowByRef("//$FOCUSED");
    IM_CHECK_SILENT(popup && IsWindowACombo(popup));

    Str128f combo_item_buf = Str128f("//%s/**/%s", popup->Name, p + 1);
    ItemClick(combo_item_buf.c_str());

    // For if Combo Selectables uses ImGuiSelectableFlags_NoAutoClosePopups
    if (GetWindowByRef("//$FOCUSED") == popup)
        KeyPress(ImGuiKey_Enter);
}

void    ImGuiTestContext::ComboClickAll(ImGuiTestRef ref_parent)
{
    ItemClick(ref_parent);

    ImGuiWindow* popup = GetWindowByRef("//$FOCUSED");
    IM_CHECK_SILENT(popup && IsWindowACombo(popup));

    ImGuiTestItemList items;
    GatherItems(&items, "//$FOCUSED");
    for (auto item : items)
    {
        // Reopen popup when closed
        if (GetWindowByRef("//$FOCUSED") != popup)
            ItemClick(ref_parent);
        ItemClick(item.ID);
    }

    // For if Combo Selectables uses ImGuiSelectableFlags_NoAutoClosePopups
    if (GetWindowByRef("//$FOCUSED") == popup)
        KeyPress(ImGuiKey_Enter);
}

static ImGuiTableColumn* HelperTableFindColumnByName(ImGuiTable* table, const char* name)
{
    for (int i = 0; i < table->Columns.size(); i++)
        if (strcmp(ImGui::TableGetColumnName(table, i), name) == 0)
            return &table->Columns[i];
    return nullptr;
}

void ImGuiTestContext::TableOpenContextMenu(ImGuiTestRef ref, int column_n)
{
    if (IsError())
        return;

    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    ImGuiTestRefDesc desc(ref);
    LogDebug("TableOpenContextMenu %s", desc.c_str());

    ImGuiTable* table = ImGui::TableFindByID(GetID(ref));
    IM_CHECK_SILENT(table != nullptr);

    if (column_n == -1)
        column_n = table->RightMostEnabledColumn;

    IM_CHECK(column_n >= 0 && column_n <= table->ColumnsCount);
    ImGuiTableColumn* column = &table->Columns[column_n];
    IM_CHECK_SILENT(column->IsEnabled);

    ImGuiID header_id = TableGetHeaderID(table, column_n);

    // Make visible
    if (!ItemExists(header_id))
        ScrollToPosX(table->InnerWindow->ID, (column->MinX + column->MaxX) * 0.5f);

    ItemClick(header_id, ImGuiMouseButton_Right);
    Yield();
}

ImGuiSortDirection ImGuiTestContext::TableClickHeader(ImGuiTestRef ref, const char* label, ImGuiKeyChord key_mods)
{
    IM_ASSERT((key_mods & ~ImGuiMod_Mask_) == 0); // Cannot pass keys only mods

    ImGuiTable* table = ImGui::TableFindByID(GetID(ref));
    IM_CHECK_SILENT_RETV(table != nullptr, ImGuiSortDirection_None);

    ImGuiTableColumn* column = HelperTableFindColumnByName(table, label);
    IM_CHECK_SILENT_RETV(column != nullptr, ImGuiSortDirection_None);

    if (key_mods != ImGuiMod_None)
        KeyDown(key_mods);

    ImGuiID header_id = TableGetHeaderID(table, label);

    // Make visible
    if (!ItemExists(header_id))
        ScrollToPosX(table->InnerWindow->ID, (column->MinX + column->MaxX) * 0.5f);

    ItemClick(header_id, ImGuiMouseButton_Left);

    if (key_mods != ImGuiMod_None)
        KeyUp(key_mods);
    return (ImGuiSortDirection)column->SortDirection;
}

void ImGuiTestContext::TableSetColumnEnabled(ImGuiTestRef ref, const char* label, bool enabled)
{
    if (IsError())
        return;

    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    ImGuiTestRefDesc desc(ref);
    LogDebug("TableSetColumnEnabled %s label '%s' enabled = %d", desc.c_str(), label, enabled);

    ImGuiTable* table = ImGui::TableFindByID(GetID(ref));
    IM_CHECK_SILENT(table != NULL);
    ImGuiTableColumn* column = HelperTableFindColumnByName(table, label);
    int column_n = column->IsEnabled ? table->Columns.index_from_ptr(column) : -1;
    TableOpenContextMenu(ref, column_n);

    ImGuiTestRef backup_ref = GetRef();
    SetRef("//$FOCUSED");
    if (enabled)
        ItemCheck(label);
    else
        ItemUncheck(label);
    PopupCloseOne();
    SetRef(backup_ref);
}

void ImGuiTestContext::TableResizeColumn(ImGuiTestRef ref, int column_n, float width)
{
    if (IsError())
        return;

    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    ImGuiTestRefDesc desc(ref);
    LogDebug("TableResizeColumn %s column %d width %.2f", desc.c_str(), column_n, width);

    ImGuiTable* table = ImGui::TableFindByID(GetID(ref));
    IM_CHECK_SILENT(table != nullptr);

    ImGuiID resize_id = ImGui::TableGetColumnResizeID(table, column_n);
    float old_width = table->Columns[column_n].WidthGiven;
    ItemDragWithDelta(resize_id, ImVec2(width - old_width, 0));

    IM_CHECK_EQ(table->Columns[column_n].WidthRequest, width);
}

const ImGuiTableSortSpecs* ImGuiTestContext::TableGetSortSpecs(ImGuiTestRef ref)
{
    ImGuiTable* table = ImGui::TableFindByID(GetID(ref));
    IM_CHECK_SILENT_RETV(table != nullptr, nullptr);

    ImGuiContext& g = *UiContext;
    ImSwap(table, g.CurrentTable);
    const ImGuiTableSortSpecs* sort_specs = ImGui::TableGetSortSpecs();
    ImSwap(table, g.CurrentTable);
    return sort_specs;
}

void    ImGuiTestContext::WindowClose(ImGuiTestRef ref)
{
    if (IsError())
        return;

    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    LogDebug("WindowClose");
    ImGuiTestRef backup_ref = GetRef();
    SetRef(GetID(ref));

#ifdef IMGUI_HAS_DOCK
    // When docked: first move to Tab to make Close Button appear.
    if (ImGuiWindow* window = GetWindowByRef(""))
        if (window->DockIsActive)
            MouseMove(window->TabId);
#endif

    ItemClick("#CLOSE");
    SetRef(backup_ref);
}

void    ImGuiTestContext::WindowCollapse(ImGuiTestRef window_ref, bool collapsed)
{
    if (IsError())
        return;
    ImGuiWindow* window = GetWindowByRef(window_ref);
    if (window == nullptr)
        return;

    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    if (window->Collapsed != collapsed)
    {
        LogDebug("WindowCollapse %d", collapsed);
        ImGuiTestOpFlags backup_op_flags = OpFlags;
        OpFlags |= ImGuiTestOpFlags_NoAutoUncollapse;
        ImGuiTestRef backup_ref = GetRef();
        SetRef(window->ID);
        ItemClick("#COLLAPSE");
        SetRef(backup_ref);
        OpFlags = backup_op_flags;
        Yield();
        IM_CHECK(window->Collapsed == collapsed);
    }
}

// Supported values for ImGuiTestOpFlags:
// - ImGuiTestOpFlags_NoError
void    ImGuiTestContext::WindowFocus(ImGuiTestRef ref, ImGuiTestOpFlags flags)
{
    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    ImGuiTestRefDesc desc(ref);
    LogDebug("WindowFocus('%s')", desc.c_str());

    ImGuiWindow* window = GetWindowByRef(ref);
    IM_CHECK_SILENT(window != nullptr);
    if (window)
    {
        ImGui::FocusWindow(window); // FIXME-TESTS-NOT_SAME_AS_END_USER: In theory should be replaced by click on title-bar or tab?
        Yield();
    }

    // We cannot guarantee this will work 100%
    // - Some modal inhibition may kick-in.
    // - Because merely hovering an item may e.g. open a window or change focus.
    //   In particular this can be the case with MenuItem. So trying to Open a MenuItem may lead to its child opening while hovering,
    //   causing this function to seemingly fail (even if the end goal was reached).
    ImGuiContext& g = *UiContext;
    if ((window != g.NavWindow) && !(flags & ImGuiTestOpFlags_NoError))
        LogDebug("-- Expected focused window '%s', but '%s' got focus back.", window->Name, g.NavWindow ? g.NavWindow->Name : "<nullptr>");
}

// Supported values for ImGuiTestOpFlags:
// - ImGuiTestOpFlags_NoError
// - ImGuiTestOpFlags_NoFocusWindow
// FIXME: In principle most calls to this could be replaced by WindowFocus()?
void    ImGuiTestContext::WindowBringToFront(ImGuiTestRef ref, ImGuiTestOpFlags flags)
{
    ImGuiContext& g = *UiContext;
    if (IsError())
        return;

    ImGuiWindow* window = GetWindowByRef(ref);
    IM_CHECK_SILENT(window != nullptr);

    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    if (window != g.NavWindow && !(flags & ImGuiTestOpFlags_NoFocusWindow))
    {
        LogDebug("WindowBringToFront()->FocusWindow('%s')", window->Name);
        ImGui::FocusWindow(window); // FIXME-TESTS-NOT_SAME_AS_END_USER: In theory should be replaced by click on title-bar or tab?
        Yield(2);
    }
    else if (window->RootWindow != g.Windows.back()->RootWindow)
    {
        LogDebug("BringWindowToDisplayFront('%s') (window.back=%s)", window->Name, g.Windows.back()->Name);
        ImGui::BringWindowToDisplayFront(window); // FIXME-TESTS-NOT_SAME_AS_END_USER: This is not an actually possible action for end-user.
        Yield(2);
    }

    // Same as WindowFocus()
    if ((window != g.NavWindow) && !(flags & ImGuiTestOpFlags_NoError))
        LogDebug("-- Expected focused window '%s', but '%s' got focus back.", window->Name, g.NavWindow ? g.NavWindow->Name : "<nullptr>");
}

// Supported values for ImGuiTestOpFlags:
// - ImGuiTestOpFlags_NoFocusWindow
void    ImGuiTestContext::WindowMove(ImGuiTestRef ref, ImVec2 input_pos, ImVec2 pivot, ImGuiTestOpFlags flags)
{
    if (IsError())
        return;

    ImGuiWindow* window = GetWindowByRef(ref);
    IM_CHECK_SILENT(window != nullptr);

    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    LogDebug("WindowMove '%s' (%.1f,%.1f) ", window->Name, input_pos.x, input_pos.y);
    ImVec2 target_pos = ImFloor(input_pos - pivot * window->Size);
    if (ImLengthSqr(target_pos - window->Pos) < 0.001f)
    {
        //MouseMoveToPos(window->Pos); //??
        return;
    }

    if ((flags & ImGuiTestOpFlags_NoFocusWindow) == 0)
        WindowFocus(window->ID);
    WindowCollapse(window->ID, false);

    MouseSetViewport(window);
    MouseMoveToPos(GetWindowTitlebarPoint(ref));
    //IM_CHECK_SILENT(UiContext->HoveredWindow == window);
    MouseDown(0);

    // Disable docking
#ifdef IMGUI_HAS_DOCK
    if (UiContext->IO.ConfigDockingWithShift)
        KeyUp(ImGuiMod_Shift);
    else
        KeyDown(ImGuiMod_Shift);
#endif

    ImVec2 delta = target_pos - window->Pos;
    MouseMoveToPos(Inputs->MousePosValue + delta);
    Yield();

    MouseUp();
#ifdef IMGUI_HAS_DOCK
    KeyUp(ImGuiMod_Shift);
#endif
    MouseSetViewport(window); // Update in case window has changed viewport
}

void    ImGuiTestContext::WindowResize(ImGuiTestRef ref, ImVec2 size)
{
    if (IsError())
        return;

    ImGuiWindow* window = GetWindowByRef(ref);
    IM_CHECK_SILENT(window != nullptr);
    size = ImFloor(size);

    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    LogDebug("WindowResize '%s' (%.1f,%.1f)", window->Name, size.x, size.y);
    if (ImLengthSqr(size - window->Size) < 0.001f)
        return;

    WindowFocus(window->ID);
    WindowCollapse(window->ID, false);

    // Extra yield as newly created window that have AutoFitFramesX/AutoFitFramesY set are temporarily not submitting their resize widgets. Give them a bit of slack.
    Yield();

    // Aim at resize border or resize corner
    ImGuiID border_x2 = ImGui::GetWindowResizeBorderID(window, ImGuiDir_Right);
    ImGuiID border_y2 = ImGui::GetWindowResizeBorderID(window, ImGuiDir_Down);
    ImGuiID resize_br = ImGui::GetWindowResizeCornerID(window, 0);
    ImGuiID id;
    if (ImAbs(size.x - window->Size.x) < 0.0001f && ItemExists(border_y2))
        id = border_y2;
    else if (ImAbs(size.y - window->Size.y) < 0.0001f && ItemExists(border_x2))
        id = border_x2;
    else
        id = resize_br;
    MouseMove(id, ImGuiTestOpFlags_IsSecondAttempt);

    if (size.x <= 0.0f || size.y <= 0.0f)
    {
        IM_ASSERT(size.x <= 0.0f && size.y <= 0.0f);
        MouseDoubleClick(0);
        Yield();
    }
    else
    {
        MouseDown(0);
        ImVec2 delta = size - window->Size;
        MouseMoveToPos(Inputs->MousePosValue + delta);
        Yield(); // At this point we don't guarantee the final size!
        MouseUp();
    }
    MouseSetViewport(window); // Update in case window has changed viewport
}

void    ImGuiTestContext::PopupCloseOne()
{
    if (IsError())
        return;

    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    LogDebug("PopupCloseOne");
    ImGuiContext& g = *UiContext;
    if (g.OpenPopupStack.Size > 0)
        ImGui::ClosePopupToLevel(g.OpenPopupStack.Size - 1, true);    // FIXME-TESTS-NOT_SAME_AS_END_USER
    Yield();
}

void    ImGuiTestContext::PopupCloseAll()
{
    if (IsError())
        return;

    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    LogDebug("PopupCloseAll");
    ImGuiContext& g = *UiContext;
    if (g.OpenPopupStack.Size > 0)
        ImGui::ClosePopupToLevel(0, true);    // FIXME-TESTS-NOT_SAME_AS_END_USER
    Yield();
}

// Match code in BeginPopupEx()
ImGuiID ImGuiTestContext::PopupGetWindowID(ImGuiTestRef ref)
{
    Str30f popup_name("//##Popup_%08x", GetID(ref));
    return GetID(popup_name.c_str());
}

#ifdef IMGUI_HAS_VIEWPORT
void    ImGuiTestContext::ViewportPlatform_SetWindowPos(ImGuiViewport* viewport, const ImVec2& pos)
{
    if (IsError())
        return;

    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    LogDebug("ViewportPlatform_SetWindowPos(0x%08X, {%.2f,%.2f)", viewport->ID, pos.x, pos.y);
    Inputs->Queue.push_back(ImGuiTestInput::ForViewportSetPos(viewport->ID, pos)); // Queued since this will poke into backend, best to do in main thread.
    Yield(); // Submit to Platform
    Yield(); // Let Dear ImGui next frame see it
}

void    ImGuiTestContext::ViewportPlatform_SetWindowSize(ImGuiViewport* viewport, const ImVec2& size)
{
    if (IsError())
        return;

    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    LogDebug("ViewportPlatform_SetWindowSize(0x%08X, {%.2f,%.2f)", viewport->ID, size.x, size.y);
    Inputs->Queue.push_back(ImGuiTestInput::ForViewportSetSize(viewport->ID, size)); // Queued since this will poke into backend, best to do in main thread.
    Yield(); // Submit to Platform
    Yield(); // Let Dear ImGui next frame see it
}

// Simulate a platform focus WITHOUT a click perceived by dear imgui. Similar to clicking on Platform title bar.
void    ImGuiTestContext::ViewportPlatform_SetWindowFocus(ImGuiViewport* viewport)
{
    if (IsError())
        return;

    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    LogDebug("ViewportPlatform_SetWindowFocus(0x%08X)", viewport->ID);
    Inputs->Queue.push_back(ImGuiTestInput::ForViewportFocus(viewport->ID)); // Queued since this will poke into backend, best to do in main thread.
    Yield(); // Submit to Platform
    Yield(); // Let Dear ImGui next frame see it
}

// Simulate a platform window closure.
void    ImGuiTestContext::ViewportPlatform_CloseWindow(ImGuiViewport* viewport)
{
    if (IsError())
        return;

    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    LogDebug("ViewportPlatform_CloseWindow(0x%08X)", viewport->ID);
    Inputs->Queue.push_back(ImGuiTestInput::ForViewportClose(viewport->ID)); // Queued since this will poke into backend, best to do in main thread.
    Yield(); // Submit to Platform
    Yield(3); // Let Dear ImGui next frame see it
}

#endif

#ifdef IMGUI_HAS_DOCK
// Note: unlike DockBuilder functions, for _nodes_ this require the node to be visible.
// Supported values for ImGuiTestOpFlags:
// - ImGuiTestOpFlags_NoFocusWindow
// FIXME-TESTS: USING ImGuiTestOpFlags_NoFocusWindow leads to increase of ForeignWindowsHideOverPos(), best to avoid
void    ImGuiTestContext::DockInto(ImGuiTestRef src_id, ImGuiTestRef dst_id, ImGuiDir split_dir, bool split_outer, ImGuiTestOpFlags flags)
{
    ImGuiContext& g = *UiContext;
    if (IsError())
        return;

    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);

    ImGuiWindow* window_src = GetWindowByRef(src_id);
    ImGuiWindow* window_dst = GetWindowByRef(dst_id);
    ImGuiDockNode* node_src = ImGui::DockBuilderGetNode(GetID(src_id));
    ImGuiDockNode* node_dst = ImGui::DockBuilderGetNode(GetID(dst_id));
    IM_CHECK_SILENT((window_src != nullptr) != (node_src != nullptr)); // Src must be either a window either a node
    IM_CHECK_SILENT((window_dst != nullptr) != (node_dst != nullptr)); // Dst must be either a window either a node

    // Infer node from window. Not the opposite as docking a node would imply docking all of it.
    if (node_src)
        window_src = node_src->HostWindow;
    if (node_dst)
        window_dst = node_dst->HostWindow;

    Str128f log("DockInto() Src: %s '%s' (0x%08X), Dst: %s '%s' (0x%08X), SplitDir = %d",
        node_src ? "node" : "window", node_src ? "" : window_src->Name, node_src ? node_src->ID : window_src->ID,
        node_dst ? "node" : "window", node_dst ? "" : window_dst->Name, node_dst ? node_dst->ID : window_dst->ID, split_dir);
    LogDebug("%s", log.c_str());

    IM_CHECK_SILENT(window_src != nullptr);
    IM_CHECK_SILENT(window_dst != nullptr);
    IM_CHECK_SILENT(window_src->WasActive);
    IM_CHECK_SILENT(window_dst->WasActive);

    // Avoid focusing if we don't need it (this facilitate avoiding focus flashing when recording animated gifs)
    if (!(flags & ImGuiTestOpFlags_NoFocusWindow))
    {
        if (g.Windows[g.Windows.Size - 2] != window_dst)
            WindowFocus(window_dst->ID);
        if (g.Windows[g.Windows.Size - 1] != window_src)
            WindowFocus(window_src->ID);
    }

    // Aim at title bar or tab or node grab
    ImGuiTestRef ref_src;
    if (node_src)
        ref_src = ImGui::DockNodeGetWindowMenuButtonId(node_src); // Whole node grab
    else
        ref_src = (window_src->DockIsActive ? window_src->TabId : window_src->MoveId); // FIXME-TESTS FIXME-DOCKING: Identify tab
    MouseMove(ref_src, ImGuiTestOpFlags_NoCheckHoveredId);
    SleepStandard();

    // Start dragging source, so it gets undocked already, because we calculate target position
    // (Consider the possibility that dragging this out will move target position)
    MouseDown(0);
    if (g.IO.ConfigDockingWithShift)
        KeyDown(ImGuiMod_Shift);
    Yield();
    MouseLiftDragThreshold();
    if (window_src->DockIsActive)
        MouseMoveToPos(g.IO.MousePos + ImVec2(0, ImGui::GetFrameHeight() * 2.0f));
    else
        Yield(); // A yield is necessary to start the moving, otherwise if by the time we call MouseSetViewport() no frame has elapsed and the viewports differs, dragging will fail.
    // (Button still held)

    // Locate target
    ImVec2 drop_pos;
    bool drop_is_valid = ImGui::DockContextCalcDropPosForDocking(window_dst, node_dst, window_src, node_src, split_dir, split_outer, &drop_pos);
    IM_CHECK_SILENT(drop_is_valid);
    if (!drop_is_valid)
    {
        if (g.IO.ConfigDockingWithShift)
            KeyUp(ImGuiMod_Shift);
        return;
    }

    // Ensure we can reach target
    WindowTeleportToMakePosVisible(window_dst->ID, drop_pos);
    ImGuiWindow* friend_windows[] = { window_src, window_dst, nullptr };
    _ForeignWindowsHideOverPos(drop_pos, friend_windows);

    // Drag
    drop_is_valid = ImGui::DockContextCalcDropPosForDocking(window_dst, node_dst, window_src, node_src, split_dir, split_outer, &drop_pos);
    IM_CHECK_SILENT(drop_is_valid);
    MouseSetViewport(window_dst);
    MouseMoveToPos(drop_pos);
    if (node_src)
        window_src = node_src->HostWindow;  // Dragging a menu button may detach a node and create a new window.
    IM_CHECK_SILENT(g.MovingWindow == window_src);

    Yield(2);    // Docking to dockspace over viewport (needs extra frame) or moving a dock node to another node (needs two extra frames) fails in fast mode without this.
    IM_CHECK_SILENT(g.HoveredWindowUnderMovingWindow && g.HoveredWindowUnderMovingWindow->RootWindowDockTree == window_dst->RootWindowDockTree);

    // Docking will happen on the mouse-up
    const ImGuiID prev_dock_id = window_src->DockId;
    const ImGuiID prev_dock_parent_id = (window_src->DockNode && window_src->DockNode->ParentNode) ? window_src->DockNode->ParentNode->ID : 0;
    const ImGuiID prev_dock_node_as_host_id = window_src->DockNodeAsHost ? window_src->DockNodeAsHost->ID : 0;

    MouseUp(0);

    // Cool down
    if (g.IO.ConfigDockingWithShift)
        KeyUp(ImGuiMod_Shift);
    _ForeignWindowsUnhideAll();
    Yield();
    Yield();

    // Verify docking has succeeded! It's not easy to write a full fledged test, let's go for a simple one.
    if (!(flags & ImGuiTestOpFlags_NoError))
    {
        const ImGuiID curr_dock_id = window_src->DockId;
        const ImGuiID curr_dock_parent_id = (window_src->DockNode && window_src->DockNode->ParentNode) ? window_src->DockNode->ParentNode->ID : 0;
        const ImGuiID curr_dock_node_as_host_id = window_src->DockNodeAsHost ? window_src->DockNodeAsHost->ID : 0;
        IM_CHECK_SILENT((prev_dock_id != curr_dock_id) || (prev_dock_parent_id != curr_dock_parent_id) || (prev_dock_node_as_host_id != curr_dock_node_as_host_id));
    }
}

void    ImGuiTestContext::DockClear(const char* window_name, ...)
{
    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    LogDebug("DockClear");

    va_list args;
    va_start(args, window_name);
    while (window_name != nullptr)
    {
        ImGui::DockBuilderDockWindow(window_name, 0);
        window_name = va_arg(args, const char*);
    }
    va_end(args);

    if (ActiveFunc == ImGuiTestActiveFunc_TestFunc)
        Yield(2); // Give time to rebuild dock in case io.ConfigDockingAlwaysTabBar is set
}

bool    ImGuiTestContext::WindowIsUndockedOrStandalone(ImGuiWindow* window)
{
    if (window->DockNode == nullptr)
        return true;
    return DockIdIsUndockedOrStandalone(window->DockId);
}

bool    ImGuiTestContext::DockIdIsUndockedOrStandalone(ImGuiID dock_id)
{
    if (dock_id == 0)
        return true;
    if (ImGuiDockNode* node = ImGui::DockBuilderGetNode(dock_id))
        if (node->IsFloatingNode() && node->IsLeafNode() && node->Windows.Size == 1)
            return true;
    return false;
}

void    ImGuiTestContext::DockNodeHideTabBar(ImGuiDockNode* node, bool hidden)
{
    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    LogDebug("DockNodeHideTabBar %d", hidden);

    ImGuiTestRef backup_ref = GetRef();
    if (hidden)
    {
        SetRef(node->HostWindow);
        ItemClick(ImGui::DockNodeGetWindowMenuButtonId(node));
        ImGuiID popup_id = PopupGetWindowID(GetID("#WindowMenu", node->ID));
        SetRef(popup_id);
#if IMGUI_VERSION_NUM >= 18910
        ItemClick("###HideTabBar");
#else
        ItemClick("Hide tab bar");
#endif
        IM_CHECK_SILENT(node->IsHiddenTabBar());
    }
    else
    {
        IM_CHECK_SILENT(node->VisibleWindow != nullptr);
        SetRef(node->VisibleWindow);
        ItemClick("#UNHIDE", 0, ImGuiTestOpFlags_MoveToEdgeD | ImGuiTestOpFlags_MoveToEdgeR);
        IM_CHECK_SILENT(!node->IsHiddenTabBar());
    }
    SetRef(backup_ref);
}

void    ImGuiTestContext::UndockNode(ImGuiID dock_id)
{
    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    LogDebug("UndockNode 0x%08X", dock_id);

    ImGuiDockNode* node = ImGui::DockBuilderGetNode(dock_id);
    if (node == nullptr)
        return;
    if (node->IsFloatingNode())
        return;
    if (node->Windows.empty())
        return;

#if IMGUI_VERSION_NUM >= 19071
    const float h = node->Windows[0]->TitleBarHeight;
#else
    const float h = node->Windows[0]->TitleBarHeight();
#endif
    if (!UiContext->IO.ConfigDockingWithShift)
        KeyDown(ImGuiMod_Shift); // Disable docking
    ItemDragWithDelta(ImGui::DockNodeGetWindowMenuButtonId(node), ImVec2(h, h) * -2);
    if (!UiContext->IO.ConfigDockingWithShift)
        KeyUp(ImGuiMod_Shift);
    MouseUp();
}

void    ImGuiTestContext::UndockWindow(const char* window_name)
{
    IM_ASSERT(window_name != nullptr);
    IMGUI_TEST_CONTEXT_REGISTER_DEPTH(this);
    LogDebug("UndockWindow \"%s\"", window_name);

    ImGuiWindow* window = GetWindowByRef(window_name);
    if (!window->DockIsActive)
        return;

#if IMGUI_VERSION_NUM >= 19071
    const float h = window->TitleBarHeight;
#else
    const float h = window->TitleBarHeight();
#endif
    if (!UiContext->IO.ConfigDockingWithShift)
        KeyDown(ImGuiMod_Shift);
    ItemDragWithDelta(window->TabId, ImVec2(h, h) * -2);
    if (!UiContext->IO.ConfigDockingWithShift)
        KeyUp(ImGuiMod_Shift);
    Yield();
}

#endif // #ifdef IMGUI_HAS_DOCK

//-------------------------------------------------------------------------
// ImGuiTestContext - Performance Tools
//-------------------------------------------------------------------------

// Calculate the reference DeltaTime, averaged over PerfIterations/500 frames, with GuiFunc disabled.
void    ImGuiTestContext::PerfCalcRef()
{
    LogDebug("Measuring ref dt...");
    RunFlags |= ImGuiTestRunFlags_GuiFuncDisable;

    ImMovingAverage<double> delta_times;
    delta_times.Init(PerfIterations);
    for (int n = 0; n < PerfIterations && !Abort; n++)
    {
        Yield();
        delta_times.AddSample(UiContext->IO.DeltaTime);
    }

    PerfRefDt = delta_times.GetAverage();
    RunFlags &= ~ImGuiTestRunFlags_GuiFuncDisable;
}

void    ImGuiTestContext::PerfCapture(const char* category, const char* test_name, const char* csv_file)
{
    if (IsError())
        return;

    // Calculate reference average DeltaTime if it wasn't explicitly called by TestFunc
    if (PerfRefDt < 0.0)
        PerfCalcRef();
    IM_ASSERT(PerfRefDt >= 0.0);

    // Yield for the average to stabilize
    LogDebug("Measuring GUI dt...");
    ImMovingAverage<double> delta_times;
    delta_times.Init(PerfIterations);
    for (int n = 0; n < PerfIterations && !Abort; n++)
    {
        Yield();
        delta_times.AddSample(UiContext->IO.DeltaTime);
    }
    if (Abort)
        return;

    double dt_curr = delta_times.GetAverage();
    double dt_ref_ms = PerfRefDt * 1000;
    double dt_delta_ms = (dt_curr - PerfRefDt) * 1000;

    const ImBuildInfo* build_info = ImBuildGetCompilationInfo();

    // Display results
    // FIXME-TESTS: Would be nice if we could submit a custom marker (e.g. branch/feature name)
    LogInfo("[PERF] Conditions: Stress x%d, %s, %s, %s, %s, %s",
        PerfStressAmount, build_info->Type, build_info->Cpu, build_info->OS, build_info->Compiler, build_info->Date);
    LogInfo("[PERF] Result: %+6.3f ms (from ref %+6.3f)", dt_delta_ms, dt_ref_ms);

    ImGuiPerfToolEntry entry;
    entry.Timestamp = Engine->BatchStartTime;
    entry.Category = category ? category : Test->Category;
    entry.TestName = test_name ? test_name : Test->Name;
    entry.DtDeltaMs = dt_delta_ms;
    entry.PerfStressAmount = PerfStressAmount;
    entry.GitBranchName = EngineIO->GitBranchName;
    entry.BuildType = build_info->Type;
    entry.Cpu = build_info->Cpu;
    entry.OS = build_info->OS;
    entry.Compiler = build_info->Compiler;
    entry.Date = build_info->Date;
    ImGuiTestEngine_PerfToolAppendToCSV(Engine->PerfTool, &entry, csv_file);

    // Disable the "Success" message
    RunFlags |= ImGuiTestRunFlags_NoSuccessMsg;
}

//-------------------------------------------------------------------------
