#include <hex/api/imhex_api.hpp>

#include <hex/api/event_manager.hpp>
#include <hex/api/task_manager.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/helpers/auto_reset.hpp>
#include <hex/providers/provider_data.hpp>
#include <hex/providers/provider.hpp>

#include <wolv/utils/string.hpp>

#include <utility>
#include <numeric>

#include <imgui.h>
#include <imgui_internal.h>
#include <set>
#include <algorithm>
#include <GLFW/glfw3.h>

#include <hex/helpers/utils_macos.hpp>

#if defined(OS_WINDOWS)
    #include <windows.h>
    #include <DSRole.h>
#else
    #include <sys/utsname.h>
    #include <unistd.h>
#endif

#if defined(OS_WEB)
    #include <emscripten.h>
#endif

namespace hex {


    namespace ImHexApi::HexEditor {

        Highlighting::Highlighting(Region region, color_t color)
            : m_region(region), m_color(color) {
        }

        Tooltip::Tooltip(Region region, std::string value, color_t color) : m_region(region), m_value(std::move(value)), m_color(color) {

        }

        namespace impl {

            static AutoReset<std::map<u32, Highlighting>> s_backgroundHighlights;
            const std::map<u32, Highlighting>& getBackgroundHighlights() {
                return *s_backgroundHighlights;
            }

            static AutoReset<std::map<u32, HighlightingFunction>> s_backgroundHighlightingFunctions;
            const std::map<u32, HighlightingFunction>& getBackgroundHighlightingFunctions() {
                return *s_backgroundHighlightingFunctions;
            }

            static AutoReset<std::map<u32, Highlighting>> s_foregroundHighlights;
            const std::map<u32, Highlighting>& getForegroundHighlights() {
                return *s_foregroundHighlights;
            }

            static AutoReset<std::map<u32, HighlightingFunction>> s_foregroundHighlightingFunctions;
            const std::map<u32, HighlightingFunction>& getForegroundHighlightingFunctions() {
                return *s_foregroundHighlightingFunctions;
            }

            static AutoReset<std::map<u32, Tooltip>> s_tooltips;
            const std::map<u32, Tooltip>& getTooltips() {
                return *s_tooltips;
            }

            static AutoReset<std::map<u32, TooltipFunction>> s_tooltipFunctions;
            const std::map<u32, TooltipFunction>& getTooltipFunctions() {
                return *s_tooltipFunctions;
            }

            static AutoReset<std::map<u32, HoveringFunction>> s_hoveringFunctions;
            const std::map<u32, HoveringFunction>& getHoveringFunctions() {
                return *s_hoveringFunctions;
            }

            static AutoReset<std::optional<ProviderRegion>> s_currentSelection;
            void setCurrentSelection(const std::optional<ProviderRegion> &region) {
                if (region == Region::Invalid()) {
                    clearSelection();
                } else {
                    *s_currentSelection = region;
                }
            }

            static PerProvider<std::optional<Region>> s_hoveredRegion;
            void setHoveredRegion(const prv::Provider *provider, const Region &region) {
                if (region == Region::Invalid())
                    s_hoveredRegion.get(provider).reset();
                else
                    s_hoveredRegion.get(provider) = region;
            }

        }

        u32 addBackgroundHighlight(const Region &region, color_t color) {
            static u32 id = 0;

            id++;

            impl::s_backgroundHighlights->insert({
                id, Highlighting { region, color }
            });

            TaskManager::doLaterOnce([]{ EventHighlightingChanged::post(); });

            return id;
        }

        void removeBackgroundHighlight(u32 id) {
            impl::s_backgroundHighlights->erase(id);

            TaskManager::doLaterOnce([]{ EventHighlightingChanged::post(); });
        }

        u32 addBackgroundHighlightingProvider(const impl::HighlightingFunction &function) {
            static u32 id = 0;

            id++;

            impl::s_backgroundHighlightingFunctions->insert({ id, function });

            TaskManager::doLaterOnce([]{ EventHighlightingChanged::post(); });

            return id;
        }

        void removeBackgroundHighlightingProvider(u32 id) {
            impl::s_backgroundHighlightingFunctions->erase(id);

            TaskManager::doLaterOnce([]{ EventHighlightingChanged::post(); });
        }

        u32 addForegroundHighlight(const Region &region, color_t color) {
            static u32 id = 0;

            id++;

            impl::s_foregroundHighlights->insert({
                id, Highlighting { region, color }
            });

            TaskManager::doLaterOnce([]{ EventHighlightingChanged::post(); });

            return id;
        }

        void removeForegroundHighlight(u32 id) {
            impl::s_foregroundHighlights->erase(id);

            TaskManager::doLaterOnce([]{ EventHighlightingChanged::post(); });
        }

        u32 addForegroundHighlightingProvider(const impl::HighlightingFunction &function) {
            static u32 id = 0;

            id++;

            impl::s_foregroundHighlightingFunctions->insert({ id, function });

            TaskManager::doLaterOnce([]{ EventHighlightingChanged::post(); });

            return id;
        }

        void removeForegroundHighlightingProvider(u32 id) {
            impl::s_foregroundHighlightingFunctions->erase(id);

            TaskManager::doLaterOnce([]{ EventHighlightingChanged::post(); });
        }

        u32 addHoverHighlightProvider(const impl::HoveringFunction &function) {
            static u32 id = 0;

            id++;

            impl::s_hoveringFunctions->insert({ id, function });

            return id;
        }

        void removeHoverHighlightProvider(u32 id) {
            impl::s_hoveringFunctions->erase(id);
        }

        static u32 s_tooltipId = 0;
        u32 addTooltip(Region region, std::string value, color_t color) {
            s_tooltipId++;
            impl::s_tooltips->insert({ s_tooltipId, { region, std::move(value), color } });

            return s_tooltipId;
        }

        void removeTooltip(u32 id) {
            impl::s_tooltips->erase(id);
        }

        static u32 s_tooltipFunctionId;
        u32 addTooltipProvider(TooltipFunction function) {
            s_tooltipFunctionId++;
            impl::s_tooltipFunctions->insert({ s_tooltipFunctionId, std::move(function) });

            return s_tooltipFunctionId;
        }

        void removeTooltipProvider(u32 id) {
            impl::s_tooltipFunctions->erase(id);
        }

        bool isSelectionValid() {
            auto selection = getSelection();
            return selection.has_value() && selection->provider != nullptr;
        }

        std::optional<ProviderRegion> getSelection() {
            return impl::s_currentSelection;
        }

        void clearSelection() {
            impl::s_currentSelection->reset();
        }

        void setSelection(const Region &region, prv::Provider *provider) {
            setSelection(ProviderRegion { region, provider == nullptr ? Provider::get() : provider });
        }

        void setSelection(const ProviderRegion &region) {
            RequestHexEditorSelectionChange::post(region);
        }

        void setSelection(u64 address, size_t size, prv::Provider *provider) {
            setSelection({ { address, size }, provider == nullptr ? Provider::get() : provider });
        }

        void addVirtualFile(const std::fs::path &path, std::vector<u8> data, Region region) {
            RequestAddVirtualFile::post(path, std::move(data), region);
        }

        const std::optional<Region>& getHoveredRegion(const prv::Provider *provider) {
            return impl::s_hoveredRegion.get(provider);
        }

    }


    namespace ImHexApi::Bookmarks {

        u64 add(Region region, const std::string &name, const std::string &comment, u32 color) {
            u64 id = 0;
            RequestAddBookmark::post(region, name, comment, color, &id);

            return id;
        }

        u64 add(u64 address, size_t size, const std::string &name, const std::string &comment, u32 color) {
            return add(Region { address, size }, name, comment, color);
        }

        void remove(u64 id) {
            RequestRemoveBookmark::post(id);
        }

    }


    namespace ImHexApi::Provider {

        static i64 s_currentProvider = -1;
        static AutoReset<std::vector<std::unique_ptr<prv::Provider>>> s_providers;
        static AutoReset<std::map<prv::Provider*, std::unique_ptr<prv::Provider>>> s_providersToRemove;

        namespace impl {

            static std::set<prv::Provider*> s_closingProviders;
            void resetClosingProvider() {
                s_closingProviders.clear();
            }

            std::set<prv::Provider*> getClosingProviders() {
                return s_closingProviders;
            }

            static std::recursive_mutex s_providerMutex;
        }

        prv::Provider *get() {
            if (!ImHexApi::Provider::isValid())
                return nullptr;

            return (*s_providers)[s_currentProvider].get();
        }

        std::vector<prv::Provider*> getProviders() {
            std::vector<prv::Provider*> result;
            result.reserve(s_providers->size());
            for (const auto &provider : *s_providers)
                result.push_back(provider.get());

            return result;
        }

        void setCurrentProvider(i64 index) {
            std::scoped_lock lock(impl::s_providerMutex);

            if (TaskManager::getRunningTaskCount() > 0)
                return;

            if (std::cmp_less(index, s_providers->size()) && s_currentProvider != index) {
                auto oldProvider  = get();
                s_currentProvider = index;
                EventProviderChanged::post(oldProvider, get());
            }

            RequestUpdateWindowTitle::post();
        }

        void setCurrentProvider(NonNull<prv::Provider*> provider) {
            std::scoped_lock lock(impl::s_providerMutex);

            if (TaskManager::getRunningTaskCount() > 0)
                return;

            const auto providers = getProviders();
            auto it = std::ranges::find(providers, provider.get());

            auto index = std::distance(providers.begin(), it);
            setCurrentProvider(index);
        }

        i64 getCurrentProviderIndex() {
            return s_currentProvider;
        }

        bool isValid() {
            return !s_providers->empty() && s_currentProvider >= 0 && s_currentProvider < i64(s_providers->size());
        }

        void markDirty() {
            const auto provider = get();
            if (!provider->isDirty()) {
                provider->markDirty();
                EventProviderDirtied::post(provider);
            }
        }

        void resetDirty() {
            for (const auto &provider : *s_providers)
                provider->markDirty(false);
        }

        bool isDirty() {
            return std::ranges::any_of(*s_providers, [](const auto &provider) {
                return provider->isDirty();
            });
        }

        void add(std::unique_ptr<prv::Provider> &&provider, bool skipLoadInterface, bool select) {
            std::scoped_lock lock(impl::s_providerMutex);

            if (TaskManager::getRunningTaskCount() > 0)
                return;

            if (skipLoadInterface)
                provider->skipLoadInterface();

            EventProviderCreated::post(provider.get());
            s_providers->emplace_back(std::move(provider));

            if (select || s_providers->size() == 1)
                setCurrentProvider(s_providers->size() - 1);
        }

        void remove(prv::Provider *provider, bool noQuestions) {
            std::scoped_lock lock(impl::s_providerMutex);

            if (provider == nullptr)
                 return;

            if (TaskManager::getRunningTaskCount() > 0)
                return;

            if (!noQuestions) {
                impl::s_closingProviders.insert(provider);

                bool shouldClose = true;
                EventProviderClosing::post(provider, &shouldClose);
                if (!shouldClose)
                    return;
            }

            const auto it = std::ranges::find_if(*s_providers, [provider](const auto &p) {
                return p.get() == provider;
            });

            if (it == s_providers->end())
                return;

            if (!s_providers->empty()) {
                if (it == s_providers->begin()) {
                    // If the first provider is being closed, select the one that's the first one now
                    setCurrentProvider(0);

                    if (s_providers->size() > 1)
                        EventProviderChanged::post(s_providers->at(0).get(), s_providers->at(1).get());
                }
                else if (std::distance(s_providers->begin(), it) == s_currentProvider) {
                    // If the current provider is being closed, select the one that's before it
                    setCurrentProvider(s_currentProvider - 1);
                }
                else {
                    // If any other provider is being closed, find the current provider in the list again and select it again
                    const auto currentProvider = get();
                    const auto currentIt = std::ranges::find_if(*s_providers, [currentProvider](const auto &p) {
                        return p.get() == currentProvider;
                    });

                    if (currentIt != s_providers->end()) {
                        auto newIndex = std::distance(s_providers->begin(), currentIt);

                        if (s_currentProvider == newIndex && newIndex != 0)
                            newIndex -= 1;

                        setCurrentProvider(newIndex);
                    } else {
                        // If the current provider is not in the list anymore, select the first one
                        setCurrentProvider(0);
                    }
                }
            }

            static std::mutex eraseMutex;

            // Move provider over to a list of providers to delete
            eraseMutex.lock();
            auto providerToRemove = it->get();
            (*s_providersToRemove)[providerToRemove] = std::move(*it);
            eraseMutex.unlock();

            // Remove left over references from the main provider list
            s_providers->erase(it);
            impl::s_closingProviders.erase(provider);

            if (s_currentProvider >= i64(s_providers->size()) && !s_providers->empty())
                setCurrentProvider(s_providers->size() - 1);

            if (s_providers->empty())
                EventProviderChanged::post(provider, nullptr);

            EventProviderClosed::post(it->get());
            RequestUpdateWindowTitle::post();

            // Do the destruction of the provider in the background once all tasks have finished
            TaskManager::runWhenTasksFinished([providerToRemove] {
                EventProviderDeleted::post(providerToRemove);
                TaskManager::createBackgroundTask("Closing Provider", [providerToRemove](Task &) {
                    eraseMutex.lock();
                    auto provider = std::move((*s_providersToRemove)[providerToRemove]);
                    s_providersToRemove->erase(providerToRemove);
                    eraseMutex.unlock();

                    provider->close();
                });
            });
        }

        prv::Provider* createProvider(const UnlocalizedString &unlocalizedName, bool skipLoadInterface, bool select) {
            prv::Provider* result = nullptr;
            RequestCreateProvider::post(unlocalizedName, skipLoadInterface, select, &result);

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
                s_mainWindowPos = ImVec2(float(x), float(y));
            }

            void setMainWindowSize(u32 width, u32 height) {
                s_mainWindowSize = ImVec2(float(width), float(height));
            }

            static ImGuiID s_mainDockSpaceId;
            void setMainDockSpaceId(ImGuiID id) {
                s_mainDockSpaceId = id;
            }

            static GLFWwindow *s_mainWindowHandle;
            void setMainWindowHandle(GLFWwindow *window) {
                s_mainWindowHandle = window;
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

            static bool s_multiWindowMode = false;
            void setMultiWindowMode(bool enabled) {
                s_multiWindowMode = enabled;
            }

            static std::optional<InitialWindowProperties> s_initialWindowProperties;
            void setInitialWindowProperties(InitialWindowProperties properties) {
                s_initialWindowProperties = properties;
            }


            static AutoReset<std::string> s_gpuVendor;
            void setGPUVendor(const std::string &vendor) {
                s_gpuVendor = vendor;
            }

            static AutoReset<std::string> s_glRenderer;
            void setGLRenderer(const std::string &renderer) {
                s_glRenderer = renderer;
            }

            static AutoReset<std::map<std::string, std::string>> s_initArguments;
            void addInitArgument(const std::string &key, const std::string &value) {
                static std::mutex initArgumentsMutex;
                std::scoped_lock lock(initArgumentsMutex);

                (*s_initArguments)[key] = value;
            }

            static double s_lastFrameTime;
            void setLastFrameTime(double time) {
                s_lastFrameTime = time;
            }

            static bool s_windowResizable = true;
            bool isWindowResizable() {
                return s_windowResizable;
            }

            static std::vector<hex::impl::AutoResetBase*> s_autoResetObjects;
            void addAutoResetObject(hex::impl::AutoResetBase *object) {
                s_autoResetObjects.emplace_back(object);
            }

            void cleanup() {
                for (const auto &object : s_autoResetObjects)
                    object->reset();
            }


        }

        bool isMainInstance() {
            return impl::s_isMainInstance;
        }

        void closeImHex(bool noQuestions) {
            RequestCloseImHex::post(noQuestions);
        }

        void restartImHex() {
            RequestRestartImHex::post();
            RequestCloseImHex::post(false);
        }

        void setTaskBarProgress(TaskProgressState state, TaskProgressType type, u32 progress) {
            EventSetTaskBarIconState::post(u32(state), u32(type), progress);
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

        float getBackingScaleFactor() {
            #if defined(OS_WINDOWS)
                return 1.0F;
            #elif defined(OS_MACOS)
                return ::getBackingScaleFactor();
            #elif defined(OS_LINUX)
                if (std::string_view(::getenv("XDG_SESSION_TYPE")) == "x11")
                    return 1.0F;
                else {
                    float xScale = 0, yScale = 0;
                    glfwGetMonitorContentScale(glfwGetPrimaryMonitor(), &xScale, &yScale);

                    return std::midpoint(xScale, yScale);
                }
            #elif defined(OS_WEB)
                return 1.0F;
                /*
                return EM_ASM_INT({
                    return window.devicePixelRatio;
                });
                */
            #else
                return 1.0F;
            #endif
        }


        ImVec2 getMainWindowPosition() {
            if ((ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) != ImGuiConfigFlags_None)
                return impl::s_mainWindowPos;
            else
                return { 0, 0 };
        }

        ImVec2 getMainWindowSize() {
            return impl::s_mainWindowSize;
        }


        ImGuiID getMainDockSpaceId() {
            return impl::s_mainDockSpaceId;
        }

        GLFWwindow* getMainWindowHandle() {
            return impl::s_mainWindowHandle;
        }

        bool isBorderlessWindowModeEnabled() {
            return impl::s_borderlessWindowMode;
        }

        bool isMutliWindowModeEnabled() {
            return impl::s_multiWindowMode;
        }

        std::optional<InitialWindowProperties> getInitialWindowProperties() {
            return impl::s_initialWindowProperties;
        }

        void* getLibImHexModuleHandle() {
            return hex::getContainingModule(reinterpret_cast<void*>(&getLibImHexModuleHandle));
        }

        void addMigrationRoutine(SemanticVersion migrationVersion, std::function<void()> function) {
            EventImHexUpdated::subscribe([migrationVersion, function](const SemanticVersion &oldVersion, const SemanticVersion &newVersion) {
                if (oldVersion < migrationVersion && newVersion >= migrationVersion) {
                    function();
                }
            });
        }


        const std::map<std::string, std::string>& getInitArguments() {
            return *impl::s_initArguments;
        }

        std::string getInitArgument(const std::string &key) {
            if (impl::s_initArguments->contains(key))
                return impl::s_initArguments->at(key);
            else
                return "";
        }



        static bool s_systemThemeDetection;
        void enableSystemThemeDetection(bool enabled) {
            s_systemThemeDetection = enabled;

            EventOSThemeChanged::post();
        }

        bool usesSystemThemeDetection() {
            return s_systemThemeDetection;
        }


        static AutoReset<std::vector<std::fs::path>> s_additionalFolderPaths;
        const std::vector<std::fs::path>& getAdditionalFolderPaths() {
            return *s_additionalFolderPaths;
        }

        void setAdditionalFolderPaths(const std::vector<std::fs::path> &paths) {
            s_additionalFolderPaths = paths;
        }


        const std::string &getGPUVendor() {
            return impl::s_gpuVendor;
        }

        const std::string &getGLRenderer() {
            return impl::s_glRenderer;
        }

        bool isCorporateEnvironment() {
            #if defined(OS_WINDOWS)
                {
                    DSROLE_PRIMARY_DOMAIN_INFO_BASIC * info;
                    if ((DsRoleGetPrimaryDomainInformation(NULL, DsRolePrimaryDomainInfoBasic, (PBYTE *)&info) == ERROR_SUCCESS) && (info != nullptr))
                    {
                        bool result = std::wstring(info->DomainNameFlat).empty();
                        DsRoleFreeMemory(info);

                        return result;
                    } else {
                        DWORD size = 1024;
                        ::GetComputerNameExA(ComputerNameDnsDomain, nullptr, &size);
                        return size > 0;
                    }
                }
            #else
                return false;
            #endif
        }

        bool isPortableVersion() {
            static std::optional<bool> portable;
            if (portable.has_value())
                return portable.value();

            if (const auto executablePath = wolv::io::fs::getExecutablePath(); executablePath.has_value()) {
                const auto flagFile = executablePath->parent_path() / "PORTABLE";

                portable = wolv::io::fs::exists(flagFile) && wolv::io::fs::isRegularFile(flagFile);
            } else {
                portable = false;
            }

            return portable.value();
        }

        std::string getOSName() {
            #if defined(OS_WINDOWS)
                return "Windows";
            #elif defined(OS_LINUX)
                #if defined(OS_FREEBSD)
                    return "FreeBSD";
                #else
                    return "Linux";
                #endif
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
                struct utsname details = { };

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
                struct utsname details = { };

                if (uname(&details) != 0) {
                    return "Unknown";
                }

                return { details.machine };
            #else
                return "Unknown";
            #endif
        }

        std::optional<LinuxDistro> getLinuxDistro() {
            wolv::io::File file("/etc/os-release", wolv::io::File::Mode::Read);
            std::string name;
            std::string version;

            auto fileContent = file.readString();
            for (const auto &line : wolv::util::splitString(fileContent, "\n")) {
                if (line.find("PRETTY_NAME=") != std::string::npos) {
                    name = line.substr(line.find("=") + 1);
                    std::erase(name, '\"');
                } else if (line.find("VERSION_ID=") != std::string::npos) {
                    version = line.substr(line.find("=") + 1);
                    std::erase(version, '\"');
                }
            }

            return { { name, version } };
        }

        SemanticVersion getImHexVersion() {
            #if defined IMHEX_VERSION
                return SemanticVersion(IMHEX_VERSION);
            #else
                return {};
            #endif
        }

        std::string getCommitHash(bool longHash) {
            #if defined GIT_COMMIT_HASH_LONG
                if (longHash) {
                    return GIT_COMMIT_HASH_LONG;
                } else {
                    return std::string(GIT_COMMIT_HASH_LONG).substr(0, 7);
                }
            #else
                std::ignore = longHash;
                return "Unknown";
            #endif
        }

        std::string getCommitBranch() {
            #if defined GIT_BRANCH
                return GIT_BRANCH;
            #else
                return "Unknown";
            #endif
        }

        bool isDebugBuild() {
            #if defined DEBUG
                return true;
            #else
                return false;
            #endif
        }

        bool isNightlyBuild() {
            return getImHexVersion().nightly();
        }

        bool updateImHex(UpdateType updateType) {
            // Get the path of the updater executable
            std::fs::path executablePath;

            for (const auto &entry : std::fs::directory_iterator(wolv::io::fs::getExecutablePath()->parent_path())) {
                if (entry.path().filename().string().starts_with("imhex-updater")) {
                    executablePath = entry.path();
                    break;
                }
            }

            if (executablePath.empty() || !wolv::io::fs::exists(executablePath))
                return false;

            std::string updateTypeString;
            switch (updateType) {
                case UpdateType::Stable:
                    updateTypeString = "latest";
                    break;
                case UpdateType::Nightly:
                    updateTypeString = "nightly";
                    break;
            }

            EventImHexClosing::subscribe([executablePath, updateTypeString] {
                hex::executeCommand(
                        hex::format("\"{}\" \"{}\"",
                                    wolv::util::toUTF8String(executablePath),
                                    updateTypeString
                                    )
                                );
            });

            ImHexApi::System::closeImHex();

            return true;
        }

        void addStartupTask(const std::string &name, bool async, const std::function<bool()> &function) {
            RequestAddInitTask::post(name, async, function);
        }

        double getLastFrameTime() {
            return impl::s_lastFrameTime;
        }

        void setWindowResizable(bool resizable) {
            glfwSetWindowAttrib(impl::s_mainWindowHandle, GLFW_RESIZABLE, int(resizable));
            impl::s_windowResizable = resizable;
        }



    }

    namespace ImHexApi::Messaging {

        namespace impl {

            static AutoReset<std::map<std::string, MessagingHandler>> s_handlers;
            const std::map<std::string, MessagingHandler>& getHandlers() {
                return *s_handlers;
            }

            void runHandler(const std::string &eventName, const std::vector<u8> &args) {
                const auto& handlers = getHandlers();
                const auto matchHandler = handlers.find(eventName);
                
                if (matchHandler == handlers.end()) {
                    log::error("Forward event handler {} not found", eventName);
                } else {
                    matchHandler->second(args);
                }

            }

        }

        void registerHandler(const std::string &eventName, const impl::MessagingHandler &handler) {
            log::debug("Registered new forward event handler: {}", eventName);

            impl::s_handlers->insert({ eventName, handler });
        }

    }

    namespace ImHexApi::Fonts {

        namespace impl {

            static AutoReset<std::vector<Font>> s_fonts;
            const std::vector<Font>& getFonts() {
                return *s_fonts;
            }

            static AutoReset<std::fs::path> s_customFontPath;
            void setCustomFontPath(const std::fs::path &path) {
                s_customFontPath = path;
            }

            static float s_fontSize = DefaultFontSize;
            void setFontSize(float size) {
                s_fontSize = size;
            }

            static AutoReset<ImFontAtlas*> s_fontAtlas;
            void setFontAtlas(ImFontAtlas* fontAtlas) {
                s_fontAtlas = fontAtlas;
            }

            static ImFont *s_boldFont = nullptr;
            static ImFont *s_italicFont = nullptr;
            void setFonts(ImFont *bold, ImFont *italic) {
                s_boldFont   = bold;
                s_italicFont = italic;
            }


        }

        GlyphRange glyph(const char *glyph) {
            u32 codepoint;
            ImTextCharFromUtf8(&codepoint, glyph, nullptr);

            return {
                .begin = u16(codepoint),
                .end   = u16(codepoint)
            };
        }
        GlyphRange glyph(u32 codepoint) {
            return {
                .begin = u16(codepoint),
                .end   = u16(codepoint)
            };
        }
        GlyphRange range(const char *glyphBegin, const char *glyphEnd) {
            u32 codepointBegin, codepointEnd;
            ImTextCharFromUtf8(&codepointBegin, glyphBegin, nullptr);
            ImTextCharFromUtf8(&codepointEnd, glyphEnd, nullptr);

            return {
                .begin = u16(codepointBegin),
                .end   = u16(codepointEnd)
            };
        }

        GlyphRange range(u32 codepointBegin, u32 codepointEnd) {
            return {
                .begin = u16(codepointBegin),
                .end   = u16(codepointEnd)
            };
        }

        void loadFont(const std::fs::path &path, const std::vector<GlyphRange> &glyphRanges, Offset offset, u32 flags, std::optional<u32> defaultSize) {
            wolv::io::File fontFile(path, wolv::io::File::Mode::Read);
            if (!fontFile.isValid()) {
                log::error("Failed to load font from file '{}'", wolv::util::toUTF8String(path));
                return;
            }

            impl::s_fonts->emplace_back(Font {
                wolv::util::toUTF8String(path.filename()),
                fontFile.readVector(),
                glyphRanges,
                offset,
                flags,
                defaultSize
            });
        }

        void loadFont(const std::string &name, const std::span<const u8> &data, const std::vector<GlyphRange> &glyphRanges, Offset offset, u32 flags, std::optional<u32> defaultSize) {
            impl::s_fonts->emplace_back(Font {
                name,
                { data.begin(), data.end() },
                glyphRanges,
                offset,
                flags,
                defaultSize
            });
        }

        const std::fs::path& getCustomFontPath() {
            return impl::s_customFontPath;
        }

        float getFontSize() {
            return impl::s_fontSize;
        }

        ImFontAtlas* getFontAtlas() {
            return impl::s_fontAtlas;
        }

        ImFont* Bold() {
            return impl::s_boldFont;
        }

        ImFont* Italic() {
            return impl::s_italicFont;
        }


    }

}
