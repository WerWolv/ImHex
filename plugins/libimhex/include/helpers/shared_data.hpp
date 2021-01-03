#pragma once

#include <functional>
#include <vector>

#include <helpers/event.hpp>
#include <imgui.h>

namespace hex { class SharedData; }

namespace hex::plugin::internal {
    void initializePlugin(SharedData &sharedData);
}

namespace hex {

    namespace prv { class Provider; }

    class SharedData {
        SharedData() = default;
    public:
        SharedData(const SharedData&) = delete;
        SharedData(SharedData&&) = delete;

        static auto& get() {
            static SharedData instance;

            return instance;
        }

        friend void hex::plugin::internal::initializePlugin(SharedData &sharedData);
        friend class Window;

    private:

        void initializeData() {
            static std::vector<EventHandler> eventHandlersStorage;
            static std::vector<std::function<void()>> deferredCallsStorage;
            static prv::Provider *currentProviderStorage;
            static ImVec2 windowPosStorage, windowSizeStorage;

            this->imguiContext      = ImGui::GetCurrentContext();
            this->eventHandlers     = &eventHandlersStorage;
            this->deferredCalls     = &deferredCallsStorage;
            this->currentProvider   = &currentProviderStorage;

            this->windowPos         = &windowPosStorage;
            this->windowSize        = &windowSizeStorage;
        }

        void initializeData(const SharedData &other) {
            this->imguiContext      = other.imguiContext;
            this->eventHandlers     = other.eventHandlers;
            this->deferredCalls     = other.deferredCalls;
            this->currentProvider   = other.currentProvider;

            this->windowPos         = other.windowPos;
            this->windowSize        = other.windowSize;
        }

    public:
        ImGuiContext *imguiContext;
        std::vector<EventHandler> *eventHandlers;
        std::vector<std::function<void()>> *deferredCalls;
        prv::Provider **currentProvider;

        ImVec2 *windowPos;
        ImVec2 *windowSize;
    };

}