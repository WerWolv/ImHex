#include <hex/helpers/shared_data.hpp>

#include <nlohmann/json.hpp>

namespace hex {

    std::vector<std::function<void()>> SharedData::deferredCalls;

    std::vector<prv::Provider*> SharedData::providers;
    u32 SharedData::currentProvider;

    std::map<std::string, std::vector<ContentRegistry::Settings::Entry>> SharedData::settingsEntries;
    nlohmann::json SharedData::settingsJson;
    std::vector<ContentRegistry::CommandPaletteCommands::Entry> SharedData::commandPaletteCommands;
    std::map<std::string, ContentRegistry::PatternLanguage::Function> SharedData::patternLanguageFunctions;
    std::map<std::string, View*> SharedData::views;
    std::vector<ContentRegistry::Tools::impl::Entry> SharedData::toolsEntries;
    std::vector<ContentRegistry::DataInspector::impl::Entry> SharedData::dataInspectorEntries;
    u32 SharedData::patternPaletteOffset;
    std::string SharedData::popupMessage;
    std::list<ImHexApi::Bookmarks::Entry> SharedData::bookmarkEntries;
    std::vector<pl::PatternData*> SharedData::patternData;

    u32 SharedData::selectableFileIndex;
    std::vector<fs::path> SharedData::selectableFiles;
    std::function<void(fs::path)> SharedData::selectableFileOpenCallback;
    std::vector<nfdfilteritem_t> SharedData::selectableFilesValidExtensions;

    std::map<std::string, std::string> SharedData::languageNames;
    std::map<std::string, std::vector<LanguageDefinition>> SharedData::languageDefinitions;
    std::map<std::string, std::string> SharedData::loadedLanguageStrings;

    ImGuiID SharedData::dockSpaceId;

    std::multimap<u32, ContentRegistry::Interface::impl::MainMenuItem> SharedData::mainMenuItems;
    std::multimap<u32, ContentRegistry::Interface::impl::MenuItem> SharedData::menuItems;

    std::vector<ContentRegistry::Interface::impl::DrawCallback> SharedData::welcomeScreenEntries;
    std::vector<ContentRegistry::Interface::impl::DrawCallback> SharedData::footerItems;
    std::vector<ContentRegistry::Interface::impl::SidebarItem>  SharedData::sidebarItems;
    std::vector<ContentRegistry::Interface::impl::DrawCallback> SharedData::toolbarItems;
    std::vector<ContentRegistry::Interface::impl::Layout> SharedData::layouts;

    std::map<Shortcut, std::function<void()>> SharedData::globalShortcuts;

    std::mutex SharedData::tasksMutex;
    std::list<Task*> SharedData::runningTasks;

    std::vector<std::string> SharedData::providerNames;

    std::vector<ContentRegistry::DataProcessorNode::impl::Entry> SharedData::dataProcessorNodes;
    u32 SharedData::dataProcessorNodeIdCounter = 1;
    u32 SharedData::dataProcessorLinkIdCounter = 1;
    u32 SharedData::dataProcessorAttrIdCounter = 1;

    std::vector<ContentRegistry::DataFormatter::impl::Entry> SharedData::dataFormatters;
    std::vector<ContentRegistry::FileHandler::impl::Entry> SharedData::fileHandlers;

    std::list<fs::path> SharedData::recentFilePaths;

    int SharedData::mainArgc;
    char **SharedData::mainArgv;
    char **SharedData::mainEnvp;

    ImFontAtlas *SharedData::fontAtlas;
    ImFontConfig SharedData::fontConfig;
    ImVec2 SharedData::windowPos;
    ImVec2 SharedData::windowSize;

    float SharedData::globalScale;
    float SharedData::fontScale;

    std::map<std::string, std::any> SharedData::sharedVariables;
}
