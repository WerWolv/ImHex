#pragma once

#include <any>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

#include <hex/api/content_registry.hpp>
#include <hex/api/imhex_api.hpp>
#include <hex/api/task.hpp>
#include <hex/views/view.hpp>

#include <imgui.h>

#include <nlohmann/json_fwd.hpp>

namespace hex { class SharedData; }

namespace hex::plugin::internal {
    void initializePlugin(SharedData &sharedData);
}

namespace hex {

    namespace prv { class Provider; }
    namespace dp { class Node; }
    namespace pl { class PatternData; }

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

        static void clearVariables() {
            SharedData::sharedVariables.clear();
        }

    public:
        static std::vector<std::function<void()>> deferredCalls;

        static std::vector<prv::Provider*> providers;
        static u32 currentProvider;

        static std::map<std::string, std::vector<ContentRegistry::Settings::Entry>> settingsEntries;
        static nlohmann::json settingsJson;
        static std::vector<ContentRegistry::CommandPaletteCommands::Entry> commandPaletteCommands;
        static std::map<std::string, ContentRegistry::PatternLanguage::Function> patternLanguageFunctions;
        static std::map<std::string, View*> views;
        static std::vector<ContentRegistry::Tools::impl::Entry> toolsEntries;
        static std::vector<ContentRegistry::DataInspector::impl::Entry> dataInspectorEntries;
        static u32 patternPaletteOffset;
        static std::string popupMessage;
        static std::list<ImHexApi::Bookmarks::Entry> bookmarkEntries;
        static std::vector<pl::PatternData*> patternData;

        static std::map<std::string, std::string> languageNames;
        static std::map<std::string, std::vector<LanguageDefinition>> languageDefinitions;
        static std::map<std::string, std::string> loadedLanguageStrings;

        static ImGuiID dockSpaceId;

        static std::multimap<u32, ContentRegistry::Interface::impl::MainMenuItem> mainMenuItems;
        static std::multimap<u32, ContentRegistry::Interface::impl::MenuItem> menuItems;
        static std::vector<ContentRegistry::Interface::impl::DrawCallback> welcomeScreenEntries;
        static std::vector<ContentRegistry::Interface::impl::DrawCallback> footerItems;
        static std::vector<ContentRegistry::Interface::impl::DrawCallback> toolbarItems;
        static std::vector<ContentRegistry::Interface::impl::SidebarItem> sidebarItems;
        static std::vector<ContentRegistry::Interface::impl::Layout> layouts;

        static std::map<Shortcut, std::function<void()>> globalShortcuts;

        static std::mutex tasksMutex;
        static std::list<Task*> runningTasks;

        static std::vector<std::string> providerNames;

        static std::vector<ContentRegistry::DataProcessorNode::impl::Entry> dataProcessorNodes;
        static u32 dataProcessorNodeIdCounter;
        static u32 dataProcessorLinkIdCounter;
        static u32 dataProcessorAttrIdCounter;

        static std::vector<ContentRegistry::DataFormatter::impl::Entry> dataFormatters;
        static std::vector<ContentRegistry::FileHandler::impl::Entry> fileHandlers;

        static std::list<fs::path> recentFilePaths;

        static int mainArgc;
        static char **mainArgv;
        static char **mainEnvp;

        static ImFontAtlas *fontAtlas;
        static ImFontConfig fontConfig;
        static ImVec2 windowPos;
        static ImVec2 windowSize;

        static float globalScale;
        static float fontScale;

    private:
        static std::map<std::string, std::any> sharedVariables;
    };

}
