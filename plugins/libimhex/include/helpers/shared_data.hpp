#pragma once

#include <any>
#include <functional>
#include <vector>

#include <helpers/event.hpp>
#include <helpers/content_registry.hpp>

#include <imgui.h>
#include <nlohmann/json.hpp>

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

        template<typename T>
        T& getVariable(std::string variableName) {
            return std::any_cast<T&>((*this->sharedVariables)[variableName]);
        }

        template<typename T>
        void setVariable(std::string variableName, T value) {
            (*this->sharedVariables)[variableName] = value;
        }

    private:

        void initializeData() {
            static std::vector<EventHandler> eventHandlersStorage;
            static std::vector<std::function<void()>> deferredCallsStorage;
            static prv::Provider *currentProviderStorage;
            static std::map<std::string, std::vector<ContentRegistry::Settings::Entry>> settingsEntriesStorage;
            static std::map<std::string, std::any> sharedVariablesStorage;
            static ImVec2 windowPosStorage, windowSizeStorage;
            static nlohmann::json settingsJsonStorage;

            this->imguiContext      = ImGui::GetCurrentContext();
            this->eventHandlers     = &eventHandlersStorage;
            this->deferredCalls     = &deferredCallsStorage;
            this->currentProvider   = &currentProviderStorage;
            this->settingsEntries   = &settingsEntriesStorage;
            this->sharedVariables   = &sharedVariablesStorage;
            this->settingsJson      = &settingsJsonStorage;

            this->windowPos         = &windowPosStorage;
            this->windowSize        = &windowSizeStorage;
        }

        void initializeData(const SharedData &other) {
            this->imguiContext      = other.imguiContext;
            this->eventHandlers     = other.eventHandlers;
            this->deferredCalls     = other.deferredCalls;
            this->currentProvider   = other.currentProvider;
            this->settingsEntries   = other.settingsEntries;
            this->sharedVariables   = other.sharedVariables;
            this->settingsJson      = other.settingsJson;

            this->windowPos         = other.windowPos;
            this->windowSize        = other.windowSize;
        }

    public:
        ImGuiContext *imguiContext;
        std::vector<EventHandler> *eventHandlers;
        std::vector<std::function<void()>> *deferredCalls;
        prv::Provider **currentProvider;
        std::map<std::string, std::vector<ContentRegistry::Settings::Entry>> *settingsEntries;
        nlohmann::json *settingsJson;

        ImVec2 *windowPos;
        ImVec2 *windowSize;

    private:
        std::map<std::string, std::any> *sharedVariables;
    };

}