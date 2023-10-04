#include <hex/api/imhex_api.hpp>
#include <hex/api/content_registry.hpp>

#include <hex/api/event.hpp>
#include <hex/api/task.hpp>
#include <hex/providers/provider.hpp>
#include <hex/helpers/fmt.hpp>

#include <wolv/io/file.hpp>

#include <utility>
#include <unistd.h>

#include <imgui.h>

#include <nlohmann/json.hpp>

#if defined(OS_WINDOWS)
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <shellapi.h>
#else
    #include <sys/utsname.h>
#endif

namespace hex {


    namespace ImHexApi::HexEditor {

        Highlighting::Highlighting(Region region, color_t color)
            : m_region(region), m_color(color) {
        }

        Tooltip::Tooltip(Region region, std::string value, color_t color) : m_region(region), m_value(std::move(value)), m_color(color) {

        }

        namespace impl {

            static std::map<u32, Highlighting> s_backgroundHighlights;
            std::map<u32, Highlighting> &getBackgroundHighlights() {
                return s_backgroundHighlights;
            }

            static std::map<u32, HighlightingFunction> s_backgroundHighlightingFunctions;
            std::map<u32, HighlightingFunction> &getBackgroundHighlightingFunctions() {
                return s_backgroundHighlightingFunctions;
            }

            static std::map<u32, Highlighting> s_foregroundHighlights;
            std::map<u32, Highlighting> &getForegroundHighlights() {
                return s_foregroundHighlights;
            }

            static std::map<u32, HighlightingFunction> s_foregroundHighlightingFunctions;
            std::map<u32, HighlightingFunction> &getForegroundHighlightingFunctions() {
                return s_foregroundHighlightingFunctions;
            }

            static std::map<u32, Tooltip> s_tooltips;
            std::map<u32, Tooltip> &getTooltips() {
                return s_tooltips;
            }

            static std::map<u32, TooltipFunction> s_tooltipFunctions;
            std::map<u32, TooltipFunction> &getTooltipFunctions() {
                return s_tooltipFunctions;
            }

            static std::optional<ProviderRegion> s_currentSelection;
            void setCurrentSelection(std::optional<ProviderRegion> region) {
                s_currentSelection = region;
            }

        }

        u32 addBackgroundHighlight(const Region &region, color_t color) {
            static u32 id = 0;

            id++;

            impl::getBackgroundHighlights().insert({
                id, Highlighting {region, color}
            });

            EventManager::post<EventHighlightingChanged>();

            return id;
        }

        void removeBackgroundHighlight(u32 id) {
            impl::getBackgroundHighlights().erase(id);

            EventManager::post<EventHighlightingChanged>();
        }

        u32 addBackgroundHighlightingProvider(const impl::HighlightingFunction &function) {
            static u32 id = 0;

            id++;

            impl::getBackgroundHighlightingFunctions().insert({ id, function });

            EventManager::post<EventHighlightingChanged>();

            return id;
        }

        void removeBackgroundHighlightingProvider(u32 id) {
            impl::getBackgroundHighlightingFunctions().erase(id);

            EventManager::post<EventHighlightingChanged>();
        }

        u32 addForegroundHighlight(const Region &region, color_t color) {
            static u32 id = 0;

            id++;

            impl::getForegroundHighlights().insert({
                id, Highlighting {region, color}
            });

            EventManager::post<EventHighlightingChanged>();

            return id;
        }

        void removeForegroundHighlight(u32 id) {
            impl::getForegroundHighlights().erase(id);

            EventManager::post<EventHighlightingChanged>();
        }

        u32 addForegroundHighlightingProvider(const impl::HighlightingFunction &function) {
            static u32 id = 0;

            id++;

            impl::getForegroundHighlightingFunctions().insert({ id, function });

            EventManager::post<EventHighlightingChanged>();

            return id;
        }

        void removeForegroundHighlightingProvider(u32 id) {
            impl::getForegroundHighlightingFunctions().erase(id);

            EventManager::post<EventHighlightingChanged>();
        }

        static u32 tooltipId = 0;
        u32 addTooltip(Region region, std::string value, color_t color) {
            tooltipId++;
            impl::getTooltips().insert({ tooltipId, { region, std::move(value), color } });

            return tooltipId;
        }

        void removeTooltip(u32 id) {
            impl::getTooltips().erase(id);
        }

        static u32 tooltipFunctionId;
        u32 addTooltipProvider(TooltipFunction function) {
            tooltipFunctionId++;
            impl::getTooltipFunctions().insert({ tooltipFunctionId, std::move(function) });

            return tooltipFunctionId;
        }

        void removeTooltipProvider(u32 id) {
            impl::getTooltipFunctions().erase(id);
        }

        bool isSelectionValid() {
            return getSelection().has_value();
        }

        std::optional<ProviderRegion> getSelection() {
            return impl::s_currentSelection;
        }

        void clearSelection() {
            impl::s_currentSelection.reset();
        }

        void setSelection(const Region &region, prv::Provider *provider) {
            setSelection(ProviderRegion { region, provider == nullptr ? Provider::get() : provider });
        }

        void setSelection(const ProviderRegion &region) {
            EventManager::post<RequestSelectionChange>(region);
        }

        void setSelection(u64 address, size_t size, prv::Provider *provider) {
            setSelection({ { address, size }, provider == nullptr ? Provider::get() : provider });
        }

    }


    namespace ImHexApi::Bookmarks {

        void add(Region region, const std::string &name, const std::string &comment, u32 color) {
            EventManager::post<RequestAddBookmark>(region, name, comment, color);
        }

        void add(u64 address, size_t size, const std::string &name, const std::string &comment, u32 color) {
            add(Region { address, size }, name, comment, color);
        }

    }


    namespace ImHexApi::Provider {

        static i64 s_currentProvider = -1;
        static std::vector<prv::Provider *> s_providers;

        namespace impl {

            static prv::Provider *s_closingProvider = nullptr;
            void resetClosingProvider() {
                s_closingProvider = nullptr;
            }

            prv::Provider* getClosingProvider() {
                return s_closingProvider;
            }

        }

        prv::Provider *get() {
            if (!ImHexApi::Provider::isValid())
                return nullptr;

            return s_providers[s_currentProvider];
        }

        const std::vector<prv::Provider *> &getProviders() {
            return s_providers;
        }

        void setCurrentProvider(u32 index) {
            if (TaskManager::getRunningTaskCount() > 0)
                return;

            if (index < s_providers.size() && s_currentProvider != index) {
                auto oldProvider  = get();
                s_currentProvider = index;
                EventManager::post<EventProviderChanged>(oldProvider, get());
            }
        }

        i64 getCurrentProviderIndex() {
            return s_currentProvider;
        }

        bool isValid() {
            return !s_providers.empty() && s_currentProvider >= 0 && s_currentProvider < i64(s_providers.size());
        }

        void markDirty() {
            get()->markDirty();
        }

        void resetDirty() {
            for (auto &provider : s_providers)
                provider->markDirty(false);
        }

        bool isDirty() {
            return std::ranges::any_of(s_providers, [](const auto &provider) {
                return provider->isDirty();
            });
        }

        void add(prv::Provider *provider, bool skipLoadInterface, bool select) {
            if (TaskManager::getRunningTaskCount() > 0)
                return;

            if (skipLoadInterface)
                provider->skipLoadInterface();

            s_providers.push_back(provider);
            EventManager::post<EventProviderCreated>(provider);

            if (select || s_providers.size() == 1)
                setCurrentProvider(s_providers.size() - 1);
        }

        void remove(prv::Provider *provider, bool noQuestions) {
            if (provider == nullptr)
                 return;

            if (TaskManager::getRunningTaskCount() > 0)
                return;

            if (!noQuestions) {
                impl::s_closingProvider = provider;

                bool shouldClose = true;
                EventManager::post<EventProviderClosing>(provider, &shouldClose);
                if (!shouldClose)
                    return;
            }

            auto it = std::find(s_providers.begin(), s_providers.end(), provider);
            if (it == s_providers.end())
                return;

            if (!s_providers.empty()) {
                if (it == s_providers.begin()) {
                    // If the first provider is being closed, select the one that's the first one now
                    setCurrentProvider(0);
                }
                else if (std::distance(s_providers.begin(), it) == s_currentProvider) {
                    // If the current provider is being closed, select the one that's before it
                    setCurrentProvider(s_currentProvider - 1);
                }
                else {
                    // If any other provider is being closed, find the current provider in the list again and select it again
                    auto currentProvider = get();
                    auto currentIt = std::find(s_providers.begin(), s_providers.end(), currentProvider);

                    if (currentIt != s_providers.end()) {
                        auto newIndex = std::distance(s_providers.begin(), currentIt);

                        if (s_currentProvider == newIndex)
                            newIndex -= 1;

                        setCurrentProvider(newIndex);
                    } else {
                        // If the current provider is not in the list anymore, select the first one
                        setCurrentProvider(0);
                    }
                }
            }

            s_providers.erase(it);
            if (s_currentProvider >= i64(s_providers.size()))
                setCurrentProvider(0);

            if (s_providers.empty())
                EventManager::post<EventProviderChanged>(provider, nullptr);

            provider->close();
            EventManager::post<EventProviderClosed>(provider);

            TaskManager::runWhenTasksFinished([provider] {
                EventManager::post<EventProviderDeleted>(provider);
                delete provider;
            });
        }

        prv::Provider* createProvider(const std::string &unlocalizedName, bool skipLoadInterface, bool select) {
            prv::Provider* result = nullptr;
            EventManager::post<RequestCreateProvider>(unlocalizedName, skipLoadInterface, select, &result);

            return result;
        }

    }

    namespace ImHexApi::System {


        namespace impl {

            // Default to true means we forward to ourselves by default
            static bool s_isMainInstance = true;

            void setMainInstanceStatus(bool status) {
                s_isMainInstance = status;
            }

            static ImVec2 s_mainWindowPos;
            static ImVec2 s_mainWindowSize;
            void setMainWindowPosition(i32 x, i32 y) {
                s_mainWindowPos = ImVec2(x, y);
            }

            void setMainWindowSize(u32 width, u32 height) {
                s_mainWindowSize = ImVec2(width, height);
            }

            static ImGuiID s_mainDockSpaceId;
            void setMainDockSpaceId(ImGuiID id) {
                s_mainDockSpaceId = id;
            }


            static float s_globalScale = 1.0;
            void setGlobalScale(float scale) {
                s_globalScale = scale;
            }

            static float s_nativeScale = 1.0;
            void setNativeScale(float scale) {
                s_nativeScale = scale;
            }


            static bool s_borderlessWindowMode;
            void setBorderlessWindowMode(bool enabled) {
                s_borderlessWindowMode = enabled;
            }

            static std::fs::path s_customFontPath;
            void setCustomFontPath(const std::fs::path &path) {
                s_customFontPath = path;
            }

            static float s_fontSize = DefaultFontSize;
            void setFontSize(float size) {
                s_fontSize = size;
            }

            static std::string s_gpuVendor;
            void setGPUVendor(const std::string &vendor) {
                s_gpuVendor = vendor;
            }

            static bool s_portableVersion = false;
            void setPortableVersion(bool enabled) {
                s_portableVersion = enabled;
            }

            void addInitArgument(const std::string &key, const std::string &value) {
                static std::mutex initArgumentsMutex;
                std::scoped_lock lock(initArgumentsMutex);

                getInitArguments()[key] = value;
            }

        }

        bool isMainInstance() {
            return impl::s_isMainInstance;
        }

        void closeImHex(bool noQuestions) {
            EventManager::post<RequestCloseImHex>(noQuestions);
        }

        void restartImHex() {
            EventManager::post<RequestRestartImHex>();
            EventManager::post<RequestCloseImHex>(false);
        }

        void setTaskBarProgress(TaskProgressState state, TaskProgressType type, u32 progress) {
            EventManager::post<EventSetTaskBarIconState>(u32(state), u32(type), progress);
        }


        static float s_targetFPS = 14.0F;

        float getTargetFPS() {
            return s_targetFPS;
        }

        void setTargetFPS(float fps) {
            s_targetFPS = fps;
        }

        float getGlobalScale() {
            return impl::s_globalScale;
        }

        float getNativeScale() {
            return impl::s_nativeScale;
        }


        ImVec2 getMainWindowPosition() {
            return impl::s_mainWindowPos;
        }

        ImVec2 getMainWindowSize() {
            return impl::s_mainWindowSize;
        }


        ImGuiID getMainDockSpaceId() {
            return impl::s_mainDockSpaceId;
        }

        bool isBorderlessWindowModeEnabled() {
            return impl::s_borderlessWindowMode;
        }

        std::map<std::string, std::string> &getInitArguments() {
            static std::map<std::string, std::string> initArgs;

            return initArgs;
        }

        std::fs::path &getCustomFontPath() {
            return impl::s_customFontPath;
        }

        float getFontSize() {
            return impl::s_fontSize;
        }


        static bool s_systemThemeDetection;


        void enableSystemThemeDetection(bool enabled) {
            s_systemThemeDetection = enabled;

            EventManager::post<EventSettingsChanged>();
            EventManager::post<EventOSThemeChanged>();
        }

        bool usesSystemThemeDetection() {
            return s_systemThemeDetection;
        }


        std::vector<std::fs::path> &getAdditionalFolderPaths() {
            static std::vector<std::fs::path> additionalFolderPaths;
            return additionalFolderPaths;
        }

        void setAdditionalFolderPaths(const std::vector<std::fs::path> &paths) {
            getAdditionalFolderPaths() = paths;
        }


        const std::string &getGPUVendor() {
            return impl::s_gpuVendor;
        }

        bool isPortableVersion() {
            return impl::s_portableVersion;
        }

        std::string getOSName() {
            #if defined(OS_WINDOWS)
                return "Windows";
            #elif defined(OS_LINUX)
                return "Linux";
            #elif defined(OS_MACOS)
                return "macOS";
            #elif defined(OS_WEB)
                return "Web";
            #else
                return "Unknown";
            #endif
        }

        std::string getOSVersion() {
            #if defined(OS_WINDOWS)
                OSVERSIONINFOA info;
                info.dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);
                ::GetVersionExA(&info);

                return hex::format("{}.{}.{}", info.dwMajorVersion, info.dwMinorVersion, info.dwBuildNumber);
            #elif defined(OS_LINUX) || defined(OS_MACOS) || defined(OS_WEB)
                struct utsname details;

                if (uname(&details) != 0) {
                    return "Unknown";
                }

                return std::string(details.release) + " " + std::string(details.version);
            #else
                return "Unknown";
            #endif
        }

        std::string getArchitecture() {
            #if defined(OS_WINDOWS)
                SYSTEM_INFO info;
                ::GetNativeSystemInfo(&info);

                switch (info.wProcessorArchitecture) {
                    case PROCESSOR_ARCHITECTURE_AMD64:
                        return "x86_64";
                    case PROCESSOR_ARCHITECTURE_ARM:
                        return "ARM";
                    case PROCESSOR_ARCHITECTURE_ARM64:
                        return "ARM64";
                    case PROCESSOR_ARCHITECTURE_IA64:
                        return "IA64";
                    case PROCESSOR_ARCHITECTURE_INTEL:
                        return "x86";
                    default:
                        return "Unknown";
                }
            #elif defined(OS_LINUX) || defined(OS_MACOS) || defined(OS_WEB)
                struct utsname details;

                if (uname(&details) != 0) {
                    return "Unknown";
                }

                return std::string(details.machine);
            #else
                return "Unknown";
            #endif
        }

        std::string getImHexVersion() {
            #if defined IMHEX_VERSION
                return IMHEX_VERSION;
            #else
                return "Unknown";
            #endif
        }

        std::string getCommitHash(bool longHash) {
            if (longHash) {
                #if defined GIT_COMMIT_HASH_LONG
                    return GIT_COMMIT_HASH_LONG;
                #else
                    return "Unknown";
                #endif
            }
            else {
                #if defined GIT_COMMIT_HASH_SHORT
                    return GIT_COMMIT_HASH_SHORT;
                #else
                    return "Unknown";
                #endif
            }
        }

        std::string getCommitBranch() {
            #if defined GIT_BRANCH
                return GIT_BRANCH;
            #else
                return "Unknown";
            #endif
        }

    }

    namespace ImHexApi::Messaging {

        namespace impl {

            std::map<std::string, MessagingHandler> &getHandlers() {
                static std::map<std::string, MessagingHandler> handlers;

                return handlers;
            }

            void runHandler(const std::string &eventName, const std::vector<u8> &args) {
                const auto& handlers = impl::getHandlers();
                auto matchHandler = handlers.find(eventName);
                
                if (matchHandler == handlers.end()) {
                    log::error("Forward event handler {} not found", eventName);
                } else {
                    matchHandler->second(args);
                }

            }

        }

        void registerHandler(const std::string &eventName, const impl::MessagingHandler &handler) {
            log::debug("Registered new forward event handler: {}", eventName);

            impl::getHandlers().insert({ eventName, handler });
        }

    }

}
