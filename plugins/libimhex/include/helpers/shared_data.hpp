#pragma once

#include <any>
#include <functional>
#include <vector>
#include <map>

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
            static std::map<std::string, Events> customEventsStorage;
            static u32 customEventsLastIdStorage = u32(Events::Events_BuiltinEnd) + 1;
            static std::vector<ContentRegistry::CommandPaletteCommands::Entry> commandPaletteCommandsStorage;

            this->imguiContext              = ImGui::GetCurrentContext();
            this->eventHandlers             = &eventHandlersStorage;
            this->deferredCalls             = &deferredCallsStorage;
            this->currentProvider           = &currentProviderStorage;
            this->settingsEntries           = &settingsEntriesStorage;
            this->sharedVariables           = &sharedVariablesStorage;
            this->windowPos                 = &windowPosStorage;
            this->windowSize                = &windowSizeStorage;
            this->settingsJson              = &settingsJsonStorage;
            this->customEvents              = &customEventsStorage;
            this->customEventsLastId        = &customEventsLastIdStorage;
            this->commandPaletteCommands    = &commandPaletteCommandsStorage;
        }

        void initializeData(const SharedData &other) {
            this->imguiContext              = other.imguiContext;
            this->eventHandlers             = other.eventHandlers;
            this->deferredCalls             = other.deferredCalls;
            this->currentProvider           = other.currentProvider;
            this->settingsEntries           = other.settingsEntries;
            this->sharedVariables           = other.sharedVariables;
            this->windowPos                 = other.windowPos;
            this->windowSize                = other.windowSize;
            this->settingsJson              = other.settingsJson;
            this->customEvents              = other.customEvents;
            this->customEventsLastId        = other.customEventsLastId;
            this->commandPaletteCommands    = other.commandPaletteCommands;
        }

    public:
        ImGuiContext *imguiContext;
        std::vector<EventHandler> *eventHandlers;
        std::vector<std::function<void()>> *deferredCalls;
        prv::Provider **currentProvider;
        std::map<std::string, std::vector<ContentRegistry::Settings::Entry>> *settingsEntries;
        nlohmann::json *settingsJson;
        std::map<std::string, Events> *customEvents;
        u32 *customEventsLastId;
        std::vector<ContentRegistry::CommandPaletteCommands::Entry> *commandPaletteCommands;

        ImVec2 *windowPos;
        ImVec2 *windowSize;

    private:
        std::map<std::string, std::any> *sharedVariables;
    };

}