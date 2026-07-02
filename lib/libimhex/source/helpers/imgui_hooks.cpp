#include <hex/api/events/events_gui.hpp>

#include <imgui.h>
#include <imgui_internal.h>

#include <hex/api/tutorial_manager.hpp>

#if !defined(IMGUI_TEST_ENGINE)

    void ImGuiTestEngineHook_ItemAdd(ImGuiContext*, ImGuiID id, const ImRect& bb, const ImGuiLastItemData*) {
        hex::TutorialManager::postElementRendered(id, bb);
    }

    void ImGuiTestEngineHook_ItemInfo(ImGuiContext*, ImGuiID, const char*, ImGuiItemStatusFlags) {}
    void ImGuiTestEngineHook_Log(ImGuiContext*, const char*, ...) {}
    const char* ImGuiTestEngine_FindItemDebugLabel(ImGuiContext*, ImGuiID) { return nullptr; }

#endif