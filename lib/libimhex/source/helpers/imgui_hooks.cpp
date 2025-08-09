#include <hex/api/events/events_gui.hpp>

#include <imgui.h>
#include <imgui_internal.h>

#include <array>

#if !defined(IMGUI_TEST_ENGINE)

    void ImGuiTestEngineHook_ItemAdd(ImGuiContext*, ImGuiID id, const ImRect& bb, const ImGuiLastItemData*) {
        std::array<float, 4> boundingBox = { bb.Min.x, bb.Min.y, bb.Max.x, bb.Max.y };
        hex::EventImGuiElementRendered::post(id, boundingBox);
    }

    void ImGuiTestEngineHook_ItemInfo(ImGuiContext*, ImGuiID, const char*, ImGuiItemStatusFlags) {}
    void ImGuiTestEngineHook_Log(ImGuiContext*, const char*, ...) {}
    const char* ImGuiTestEngine_FindItemDebugLabel(ImGuiContext*, ImGuiID) { return nullptr; }

#endif