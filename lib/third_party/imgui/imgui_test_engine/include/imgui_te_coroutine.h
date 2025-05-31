// dear imgui test engine
// (coroutine interface + optional implementation)
// Read https://github.com/ocornut/imgui_test_engine/wiki/Setting-Up

// This file is governed by the "Dear ImGui Test Engine License".
// Details of the license are provided in the LICENSE.txt file in the same directory.

#pragma once

#ifndef IMGUI_VERSION
#include "imgui.h"
#endif

//------------------------------------------------------------------------
// Coroutine abstraction
//------------------------------------------------------------------------
// Coroutines should be used like this:
//   ImGuiTestCoroutineHandle handle = CoroutineCreate(<func>, <name>, <ctx>); // name being for debugging, and ctx being an arbitrary user context pointer
//   while (CoroutineRun(handle)) { <do other stuff };
//   CoroutineDestroy(handle);
// The coroutine code itself should call CoroutineYieldFunc() whenever it wants to yield control back to the main thread.
//------------------------------------------------------------------------

// An arbitrary handle used internally to represent coroutines (nullptr indicates no handle)
typedef void* ImGuiTestCoroutineHandle;

// A coroutine main function
typedef void (ImGuiTestCoroutineMainFunc)(void* data);

// Coroutine support interface
// Your app needs to return and implement this.
// You can '#define IMGUI_TEST_ENGINE_ENABLE_COROUTINE_STDTHREAD_IMPL 1' in your imconfig file to use a default implementation using std::thread
// Documentation: https://github.com/ocornut/imgui_test_engine/wiki/Setting-Up
struct IMGUI_API ImGuiTestCoroutineInterface
{
    // Create a new coroutine
    ImGuiTestCoroutineHandle (*CreateFunc)(ImGuiTestCoroutineMainFunc* func, const char* name, void* data);

    // Destroy a coroutine (which must have completed first)
    void                     (*DestroyFunc)(ImGuiTestCoroutineHandle handle);

    // Run a coroutine until it yields or finishes, returning false if finished
    bool                     (*RunFunc)(ImGuiTestCoroutineHandle handle);

    // Yield from a coroutine back to the caller, preserving coroutine state
    void                     (*YieldFunc)();
};

//------------------------------------------------------------------------
// Coroutine implementation using std::thread
// The "coroutine" thread and user's main thread will always block on each other (both threads will NEVER run in parallel)
// It is just an implementation convenience that we provide an implementation using std::thread as it is widely available/standard.
//------------------------------------------------------------------------

#if IMGUI_TEST_ENGINE_ENABLE_COROUTINE_STDTHREAD_IMPL

IMGUI_API ImGuiTestCoroutineInterface*    Coroutine_ImplStdThread_GetInterface();

#endif // #if IMGUI_TEST_ENGINE_ENABLE_COROUTINE_STDTHREAD_IMPL
