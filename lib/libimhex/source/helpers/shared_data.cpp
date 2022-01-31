#include <hex/helpers/shared_data.hpp>

#include <nlohmann/json.hpp>

namespace hex {

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
    std::vector<ContentRegistry::Interface::impl::SidebarItem> SharedData::sidebarItems;
    std::vector<ContentRegistry::Interface::impl::DrawCallback> SharedData::toolbarItems;
    std::vector<ContentRegistry::Interface::impl::Layout> SharedData::layouts;

    std::map<Shortcut, std::function<void()>> SharedData::globalShortcuts;

    std::mutex SharedData::tasksMutex;
    std::list<Task *> SharedData::runningTasks;

    std::vector<std::string> SharedData::providerNames;

    std::vector<ContentRegistry::DataProcessorNode::impl::Entry> SharedData::dataProcessorNodes;
    u32 SharedData::dataProcessorNodeIdCounter = 1;
    u32 SharedData::dataProcessorLinkIdCounter = 1;
    u32 SharedData::dataProcessorAttrIdCounter = 1;

    std::vector<ContentRegistry::DataFormatter::impl::Entry> SharedData::dataFormatters;
    std::vector<ContentRegistry::FileHandler::impl::Entry> SharedData::fileHandlers;

    std::list<fs::path> SharedData::recentFilePaths;

    ImFontAtlas *SharedData::fontAtlas;
    ImFontConfig SharedData::fontConfig;
    ImVec2 SharedData::windowPos;
    ImVec2 SharedData::windowSize;

    float SharedData::globalScale;
    float SharedData::fontScale;

    std::map<std::string, std::any> SharedData::sharedVariables;
}
