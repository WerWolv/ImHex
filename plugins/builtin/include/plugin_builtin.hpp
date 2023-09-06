extern "C" [[gnu::visibility("default")]] const char *getPluginName();
extern "C" [[gnu::visibility("default")]] const char *getPluginAuthor();
extern "C" [[gnu::visibility("default")]] const char *getPluginDescription();
extern "C" [[gnu::visibility("default")]] const char *getCompatibleVersion();
extern "C" [[gnu::visibility("default")]] const char *initializePlugin();
extern "C" [[gnu::visibility("default")]] const char *setImGuiContext(ImGuiContext *ctx);
extern "C" [[gnu::visibility("default")]] bool isBuiltinPlugin();
