namespace hex::plugin::builtin {

    void registerEventHandlers();
    void registerDataVisualizers();
    void registerMiniMapVisualizers();
    void registerDataInspectorEntries();
    void registerToolEntries();
    void registerPatternLanguageFunctions();
    void registerPatternLanguageTypes();
    void registerPatternLanguagePragmas();
    void registerPatternLanguageVisualizers();
    void registerCommandPaletteCommands();
    void registerSettings();
    void loadSettings();
    void registerDataProcessorNodes();
    void registerProviders();
    void registerDataFormatters();
    void registerMainMenuEntries();
    void createWelcomeScreen();
    void registerViews();
    void registerThemeHandlers();
    void registerStyleHandlers();
    void registerThemes();
    void registerBackgroundServices();
    void registerNetworkEndpoints();
    void registerMCPTools();
    void registerFileHandlers();
    void registerProjectHandlers();
    void registerAchievements();
    void registerReportGenerators();
    void registerTutorials();
    void registerDataInformationSections();
    void loadWorkspaces();

    void addWindowDecoration();
    void addFooterItems();
    void addTitleBarButtons();
    void addToolbarItems();
    void addGlobalUIItems();
    void addInitTasks();

    void handleBorderlessWindowMode();
    void setupOutOfBoxExperience();

    void extractBundledFiles();
}
