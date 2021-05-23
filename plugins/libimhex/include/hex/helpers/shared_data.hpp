#pragma once

#include <any>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <vector>

#include <hex/api/content_registry.hpp>
#include <hex/api/imhex_api.hpp>
#include <hex/views/view.hpp>

#include <imgui.h>

#include <nlohmann/json.hpp>

namespace hex { class SharedData; }

namespace hex::plugin::internal {
    void initializePlugin(SharedData &sharedData);
}

namespace hex {

    namespace prv { class Provider; }
    namespace dp { class Node; }
    namespace lang { class PatternData; }

    class View;

    class SharedData {
        SharedData() = default;
    public:
        SharedData(const SharedData&) = delete;
        SharedData(SharedData&&) = delete;

        friend class Window;

        template<typename T>
        static T& getVariable(std::string variableName) {
            return std::any_cast<T&>(SharedData::sharedVariables[variableName]);
        }

        template<typename T>
        static void setVariable(std::string variableName, T value) {
            SharedData::sharedVariables[variableName] = value;
        }

    public:
        static std::vector<std::function<void()>> deferredCalls;
        static prv::Provider *currentProvider;
        static std::map<std::string, std::vector<ContentRegistry::Settings::Entry>> settingsEntries;
        static nlohmann::json settingsJson;
        static std::vector<ContentRegistry::CommandPaletteCommands::Entry> commandPaletteCommands;
        static std::map<std::string, ContentRegistry::PatternLanguageFunctions::Function> patternLanguageFunctions;
        static std::vector<View*> views;
        static std::vector<ContentRegistry::Tools::Entry> toolsEntries;
        static std::vector<ContentRegistry::DataInspector::Entry> dataInspectorEntries;
        static u32 patternPaletteOffset;
        static std::string errorPopupMessage;
        static std::list<ImHexApi::Bookmarks::Entry> bookmarkEntries;
        static std::vector<lang::PatternData*> patternData;

        static std::map<std::string, std::string> languageNames;
        static std::map<std::string, std::vector<LanguageDefinition>> languageDefinitions;
        static std::map<std::string, std::string> loadedLanguageStrings;

        static std::vector<ContentRegistry::Interface::DrawCallback> welcomeScreenEntries;
        static std::vector<ContentRegistry::Interface::DrawCallback> footerItems;

        static std::vector<ContentRegistry::DataProcessorNode::Entry> dataProcessorNodes;
        static u32 dataProcessorNodeIdCounter;
        static u32 dataProcessorLinkIdCounter;
        static u32 dataProcessorAttrIdCounter;

        static std::list<std::string> recentFilePaths;

        static int mainArgc;
        static char **mainArgv;

        static ImVec2 windowPos;
        static ImVec2 windowSize;

    private:
        static std::map<std::string, std::any> sharedVariables;
    };

}