#include <hex/helpers/shared_data.hpp>

namespace hex {

    std::vector<EventHandler> SharedData::eventHandlers;
    std::vector<std::function<void()>> SharedData::deferredCalls;
    prv::Provider *SharedData::currentProvider;
    std::map<std::string, std::vector<ContentRegistry::Settings::Entry>> SharedData::settingsEntries;
    nlohmann::json SharedData::settingsJson;
    std::map<std::string, Events> SharedData::customEvents;
    u32 SharedData::customEventsLastId;
    std::vector<ContentRegistry::CommandPaletteCommands::Entry> SharedData::commandPaletteCommands;
    std::map<std::string, ContentRegistry::PatternLanguageFunctions::Function> SharedData::patternLanguageFunctions;
    std::vector<std::unique_ptr<View>> SharedData::views;
    std::vector<ContentRegistry::Tools::Entry> SharedData::toolsEntries;
    std::vector<ContentRegistry::DataInspector::Entry> SharedData::dataInspectorEntries;
    u32 SharedData::patternPaletteOffset;
    std::string SharedData::errorPopupMessage;
    std::list<ImHexApi::Bookmarks::Entry> SharedData::bookmarkEntries;

    std::map<std::string, std::string> SharedData::languageNames;
    std::map<std::string, std::vector<LanguageDefinition>> SharedData::languageDefinitions;
    std::map<std::string, std::string> SharedData::loadedLanguageStrings;

    std::vector<ContentRegistry::Interface::DrawCallback> SharedData::welcomeScreenEntries;
    std::vector<ContentRegistry::Interface::DrawCallback> SharedData::footerItems;

    std::vector<ContentRegistry::DataProcessorNode::Entry> SharedData::dataProcessorNodes;
    u32 SharedData::dataProcessorNodeIdCounter = 1;

    int SharedData::mainArgc;
    char **SharedData::mainArgv;

    ImVec2 SharedData::windowPos;
    ImVec2 SharedData::windowSize;

    std::map<std::string, std::any> SharedData::sharedVariables;
}