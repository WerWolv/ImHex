// dear imgui test engine
// (ui)
// If you run tests in an interactive or visible application, you may want to call ImGuiTestEngine_ShowTestEngineWindows()

// This file is governed by the "Dear ImGui Test Engine License".
// Details of the license are provided in the LICENSE.txt file in the same directory.

// Provide access to:
// - "Dear ImGui Test Engine" main interface
// - "Dear ImGui Capture Tool"
// - "Dear ImGui Perf Tool"
// - other core debug functions: Metrics, Debug Log

#pragma once

#ifndef IMGUI_VERSION
#include "imgui.h"      // IMGUI_API
#endif

// Forward declarations
struct ImGuiTestEngine;

// Functions
IMGUI_API void  ImGuiTestEngine_ShowTestEngineWindows(ImGuiTestEngine* engine, bool* p_open);
IMGUI_API void  ImGuiTestEngine_OpenSourceFile(ImGuiTestEngine* engine, const char* source_filename, int source_line_no);
