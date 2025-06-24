// dear imgui test engine
// (coroutine interface + optional implementation)
// Read https://github.com/ocornut/imgui_test_engine/wiki/Setting-Up

// This file is governed by the "Dear ImGui Test Engine License".
// Details of the license are provided in the LICENSE.txt file in the same directory.

#include "imgui_te_coroutine.h"
#include "imgui.h"

#ifdef _MSC_VER
#pragma warning (disable: 4996)     // 'This function or variable may be unsafe': strcpy, strdup, sprintf, vsnprintf, sscanf, fopen
#endif

//------------------------------------------------------------------------
// Coroutine implementation using std::thread
// This implements a coroutine using std::thread, with a helper thread for each coroutine (with serialised execution, so threads never actually run concurrently)
//------------------------------------------------------------------------

#if IMGUI_TEST_ENGINE_ENABLE_COROUTINE_STDTHREAD_IMPL

#include "imgui_te_utils.h"
#include "thirdparty/Str/Str.h"
#include <thread>
#include <mutex>
#include <condition_variable>

struct Coroutine_ImplStdThreadData
{
    std::thread*            Thread;                 // The thread this coroutine is using
    std::condition_variable StateChange;            // Condition variable notified when the coroutine state changes
    std::mutex              StateMutex;             // Mutex to protect coroutine state
    bool                    CoroutineRunning;       // Is the coroutine currently running? Lock StateMutex before access and notify StateChange on change
    bool                    CoroutineTerminated;    // Has the coroutine terminated? Lock StateMutex before access and notify StateChange on change
    Str64                   Name;                   // The name of this coroutine
};

// The coroutine executing on the current thread (if it is a coroutine thread)
static thread_local Coroutine_ImplStdThreadData* GThreadCoroutine = nullptr;

// The main function for a coroutine thread
static void CoroutineThreadMain(Coroutine_ImplStdThreadData* data, ImGuiTestCoroutineMainFunc func, void* ctx)
{
    // Set our thread name
    ImThreadSetCurrentThreadDescription(data->Name.c_str());

    // Set the thread coroutine
    GThreadCoroutine = data;

    // Wait for initial Run()
    while (1)
    {
        std::unique_lock<std::mutex> lock(data->StateMutex);
        if (data->CoroutineRunning)
            break;
        data->StateChange.wait(lock);
    }

    // Run user code, which will then call Yield() when it wants to yield control
    func(ctx);

    // Mark as terminated
    {
        std::lock_guard<std::mutex> lock(data->StateMutex);

        data->CoroutineTerminated = true;
        data->CoroutineRunning = false;
        data->StateChange.notify_all();
    }
}


static ImGuiTestCoroutineHandle Coroutine_ImplStdThread_Create(ImGuiTestCoroutineMainFunc* func, const char* name, void* ctx)
{
    Coroutine_ImplStdThreadData* data = new Coroutine_ImplStdThreadData();

    data->Name = name;
    data->CoroutineRunning = false;
    data->CoroutineTerminated = false;
    data->Thread = new std::thread(CoroutineThreadMain, data, func, ctx);

    return (ImGuiTestCoroutineHandle)data;
}

static void Coroutine_ImplStdThread_Destroy(ImGuiTestCoroutineHandle handle)
{
    Coroutine_ImplStdThreadData* data = (Coroutine_ImplStdThreadData*)handle;

    IM_ASSERT(data->CoroutineTerminated); // The coroutine needs to run to termination otherwise it may leak all sorts of things and this will deadlock
    if (data->Thread)
    {
        data->Thread->join();

        delete data->Thread;
        data->Thread = nullptr;
    }

    delete data;
    data = nullptr;
}

// Run the coroutine until the next call to Yield(). Returns TRUE if the coroutine yielded, FALSE if it terminated (or had previously terminated)
static bool Coroutine_ImplStdThread_Run(ImGuiTestCoroutineHandle handle)
{
    Coroutine_ImplStdThreadData* data = (Coroutine_ImplStdThreadData*)handle;

    // Wake up coroutine thread
    {
        std::lock_guard<std::mutex> lock(data->StateMutex);

        if (data->CoroutineTerminated)
            return false; // Coroutine has already finished

        data->CoroutineRunning = true;
        data->StateChange.notify_all();
    }

    // Wait for coroutine to stop
    while (1)
    {
        std::unique_lock<std::mutex> lock(data->StateMutex);
        if (!data->CoroutineRunning)
        {
            // Breakpoint here to catch the point where we return from the coroutine
            if (data->CoroutineTerminated)
                return false; // Coroutine finished
            break;
        }
        data->StateChange.wait(lock);
    }

    return true;
}

// Yield the current coroutine (can only be called from a coroutine)
static void Coroutine_ImplStdThread_Yield()
{
    IM_ASSERT(GThreadCoroutine); // This can only be called from a coroutine thread

    Coroutine_ImplStdThreadData* data = GThreadCoroutine;

    // Flag that we are not running any more
    {
        std::lock_guard<std::mutex> lock(data->StateMutex);
        data->CoroutineRunning = false;
        data->StateChange.notify_all();
    }

    // At this point the thread that called RunCoroutine() will leave the "Wait for coroutine to stop" loop
    // Wait until we get started up again
    while (1)
    {
        std::unique_lock<std::mutex> lock(data->StateMutex);
        if (data->CoroutineRunning)
            break; // Breakpoint here if you want to catch the point where execution of this coroutine resumes
        data->StateChange.wait(lock);
    }
}

ImGuiTestCoroutineInterface* Coroutine_ImplStdThread_GetInterface()
{
    static ImGuiTestCoroutineInterface intf;
    intf.CreateFunc = Coroutine_ImplStdThread_Create;
    intf.DestroyFunc = Coroutine_ImplStdThread_Destroy;
    intf.RunFunc = Coroutine_ImplStdThread_Run;
    intf.YieldFunc = Coroutine_ImplStdThread_Yield;
    return &intf;
}

#endif // #if IMGUI_TEST_ENGINE_ENABLE_COROUTINE_STDTHREAD_IMPL
