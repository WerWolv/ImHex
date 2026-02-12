#include <hex/api/imhex_api/bookmarks.hpp>
#include <hex/api/imhex_api/hex_editor.hpp>
#include <hex/api/imhex_api/fonts.hpp>
#include <hex/api/imhex_api/messaging.hpp>
#include <hex/api/imhex_api/provider.hpp>
#include <hex/api/imhex_api/system.hpp>

#include <hex/api/events/events_provider.hpp>
#include <hex/api/events/events_lifecycle.hpp>
#include <hex/api/events/events_gui.hpp>
#include <hex/api/events/requests_interaction.hpp>
#include <hex/api/events/requests_lifecycle.hpp>
#include <hex/api/events/requests_provider.hpp>
#include <hex/api/events/requests_gui.hpp>
#include <hex/api/events/events_interaction.hpp>

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
#include <hex/api_urls.hpp>
#include <hex/helpers/http_requests.hpp>

#include <hex/helpers/utils_macos.hpp>
#include <nlohmann/json.hpp>

#if defined(OS_WINDOWS)
    #include <windows.h>
    #include <DSRole.h>
#else
    #include <sys/utsname.h>
    #include <unistd.h>
#endif

#if defined(OS_WEB)
    #include <emscripten.h>
#elif defined(OS_MACOS)
    extern "C" {
        void macosRegisterFont(const unsigned char *data, size_t size);
    }
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
                if (provider == nullptr)
                    return;
                
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
            setSelection({ { .address=address, .size=size }, provider == nullptr ? Provider::get() : provider });
        }

        void addVirtualFile(const std::string &path, std::vector<u8> data, Region region) {
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
            return add(Region { .address=address, .size=size }, name, comment, color);
        }

        void remove(u64 id) {
            RequestRemoveBookmark::post(id);
        }

    }


    namespace ImHexApi::Provider {

        static i64 s_currentProvider = -1;
        static AutoReset<std::vector<std::shared_ptr<prv::Provider>>> s_providers;
        static AutoReset<std::map<prv::Provider*, std::shared_ptr<prv::Provider>>> s_providersToRemove;

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
            }
            EventProviderDirtied::post(provider);
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

        void add(std::shared_ptr<prv::Provider> &&provider, bool skipLoadInterface, bool select) {
            std::scoped_lock lock(impl::s_providerMutex);

            if (TaskManager::getRunningTaskCount() > 0)
                return;

            if (skipLoadInterface)
                provider->skipLoadInterface();

            EventProviderCreated::post(provider);
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

            EventProviderClosed::post(providerToRemove);
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

        std::shared_ptr<prv::Provider> createProvider(const UnlocalizedString &unlocalizedName, bool skipLoadInterface, bool select) {
            std::shared_ptr<prv::Provider> result = nullptr;
            RequestCreateProvider::post(unlocalizedName, skipLoadInterface, select, &result);

            return result;
        }

        void openProvider(std::shared_ptr<prv::Provider> provider) {
            RequestOpenProvider::post(provider);
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

            static bool s_mainWindowFocused = false;
            void setMainWindowFocusState(bool focused) {
                s_mainWindowFocused = focused;
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

            static SemanticVersion s_openGLVersion;
            void setGLVersion(SemanticVersion version) {
                s_openGLVersion = version;
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

            static auto& getAutoResetObjects() {
                static std::set<hex::impl::AutoResetBase*> autoResetObjects;

                return autoResetObjects;
            }

            void addAutoResetObject(hex::impl::AutoResetBase *object) {
                getAutoResetObjects().insert(object);
            }

            void removeAutoResetObject(hex::impl::AutoResetBase *object) {
                getAutoResetObjects().erase(object);
            }

            void cleanup() {
                for (const auto &object : getAutoResetObjects())
                    object->reset();
            }

            static bool s_frameRateUnlockRequested = false;
            bool frameRateUnlockRequested() {
                return s_frameRateUnlockRequested;
            }

            void resetFrameRateUnlockRequested() {
                s_frameRateUnlockRequested = false;
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
                const auto sessionType = hex::getEnvironmentVariable("XDG_SESSION_TYPE");
                if (!sessionType.has_value() || sessionType == "x11")
                    return 1.0F;
                else {
                    int windowW, windowH;
                    int displayW, displayH;
                    glfwGetWindowSize(getMainWindowHandle(), &windowW, &windowH);
                    glfwGetFramebufferSize(getMainWindowHandle(), &displayW, &displayH);

                    return (windowW > 0) ? float(displayW) / windowW : 1.0f;
                }
            #elif defined(OS_WEB)
                return emscripten_get_device_pixel_ratio();
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

        bool isMainWindowFocused() {
            return impl::s_mainWindowFocused;
        }

        bool isBorderlessWindowModeEnabled() {
            return impl::s_borderlessWindowMode;
        }

        bool isMultiWindowModeEnabled() {
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

        const SemanticVersion& getGLVersion() {
            return impl::s_openGLVersion;
        }

        bool isCorporateEnvironment() {
            #if defined(OS_WINDOWS)
                {
                    DSROLE_PRIMARY_DOMAIN_INFO_BASIC * info;
                    if ((DsRoleGetPrimaryDomainInformation(NULL, DsRolePrimaryDomainInfoBasic, (PBYTE *)&info) == ERROR_SUCCESS) && (info != nullptr))
                    {
                        bool result = std::wstring(info->DomainNameFlat) != L"WORKGROUP";
                        DsRoleFreeMemory(info);

                        return result;
                    } else {
                        DWORD size = 128;
                        char buffer[128];
                        ::GetComputerNameExA(ComputerNameDnsDomain, buffer, &size);
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

                return fmt::format("{}.{}.{}", info.dwMajorVersion, info.dwMinorVersion, info.dwBuildNumber);
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

            return { { .name=name, .version=version } };
        }

        const SemanticVersion& getImHexVersion() {
            #if defined(IMHEX_VERSION)
                static auto version = SemanticVersion(IMHEX_VERSION);
                return version;
            #else
                static auto version = SemanticVersion();
                return version;
            #endif
        }

        std::string getCommitHash(bool longHash) {
            #if defined(GIT_COMMIT_HASH_LONG)
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
            #if defined(GIT_BRANCH)
                return GIT_BRANCH;
            #else
                return "Unknown";
            #endif
        }

        std::optional<std::chrono::system_clock::time_point> getBuildTime() {
            #if defined(IMHEX_BUILD_DATE)
                return hex::parseTime("%Y-%m-%dT%H:%M:%SZ", IMHEX_BUILD_DATE);
            #else
                return std::nullopt;
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
            const static bool isNightly = getImHexVersion().nightly();

            return isNightly;
        }

        std::optional<std::string> checkForUpdate() {
            #if defined(OS_WEB)
                return std::nullopt;
            #else
                if (ImHexApi::System::isNightlyBuild()) {
                    HttpRequest request("GET", GitHubApiURL + std::string("/releases/tags/nightly"));
                    request.setTimeout(10000);

                    // Query the GitHub API for the latest release version
                    auto response = request.execute().get();
                    if (response.getStatusCode() != 200)
                        return std::nullopt;

                    nlohmann::json releases;
                    try {
                        releases = nlohmann::json::parse(response.getData());
                    } catch (const std::exception &) {
                        return std::nullopt;
                    }

                    // Check if the response is valid
                    if (!releases.contains("assets") || !releases["assets"].is_array())
                        return std::nullopt;

                    const auto firstAsset = releases["assets"].front();
                    if (!firstAsset.is_object() || !firstAsset.contains("updated_at"))
                        return std::nullopt;

                    const auto nightlyUpdateTime = hex::parseTime("%Y-%m-%dT%H:%M:%SZ", firstAsset["updated_at"].get<std::string>());
                    const auto imhexBuildTime = ImHexApi::System::getBuildTime();

                    // Give a bit of time leniency for the update time check
                    // We're comparing here the binary build time to the release upload time. If we were to strictly compare
                    // upload time to be greater than current build time, the check would always be true since the CI
                    // takes a few minutes after the build to actually upload the artifact.
                    // TODO: Is there maybe a better way to handle this without downloading the artifact just to check the build time?
                    if (nightlyUpdateTime.has_value() && imhexBuildTime.has_value() && *nightlyUpdateTime > *imhexBuildTime + std::chrono::hours(1)) {
                        return "Nightly";
                    }
                } else {
                    HttpRequest request("GET", GitHubApiURL + std::string("/releases/latest"));

                    // Query the GitHub API for the latest release version
                    auto response = request.execute().get();
                    if (response.getStatusCode() != 200)
                        return std::nullopt;

                    nlohmann::json releases;
                    try {
                        releases = nlohmann::json::parse(response.getData());
                    } catch (const std::exception &) {
                        return std::nullopt;
                    }

                    // Check if the response is valid
                    if (!releases.contains("tag_name") || !releases["tag_name"].is_string())
                        return std::nullopt;

                    // Convert the current version string to a format that can be compared to the latest release
                    auto currVersion   = "v" + ImHexApi::System::getImHexVersion().get(false);

                    // Get the latest release version string
                    auto latestVersion = releases["tag_name"].get<std::string>();

                    // Check if the latest release is different from the current version
                    if (latestVersion != currVersion)
                        return latestVersion;
                }

                return std::nullopt;
            #endif
        }

        bool updateImHex(UpdateType updateType) {
            #if defined(OS_WEB)
                switch (updateType) {
                    case UpdateType::Stable:
                        EM_ASM({ window.location.href = window.location.origin; });
                        break;
                    case UpdateType::Nightly:
                        EM_ASM({ window.location.href = window.location.origin + "/nightly"; });
                        break;
                }

                return true;
            #else
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
                        updateTypeString = "stable";
                        break;
                    case UpdateType::Nightly:
                        updateTypeString = "nightly";
                        break;
                }

                EventImHexClosing::subscribe([executablePath, updateTypeString] {
                    hex::startProgram({ wolv::util::toUTF8String(executablePath), updateTypeString });
                });

                ImHexApi::System::closeImHex();

                return true;
            #endif
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

        void unlockFrameRate() {
            impl::s_frameRateUnlockRequested = true;
        }

        void setPostProcessingShader(const std::string &vertexShader, const std::string &fragmentShader) {
            RequestSetPostProcessingShader::post(vertexShader, fragmentShader);
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

            static AutoReset<std::vector<MergeFont>> s_fonts;
            const std::vector<MergeFont>& getMergeFonts() {
                return *s_fonts;
            }

            static AutoReset<std::map<UnlocalizedString, FontDefinition>> s_fontDefinitions;
            std::map<UnlocalizedString, FontDefinition>& getFontDefinitions() {
                return *s_fontDefinitions;
            }

            static AutoReset<const Font*> s_defaultFont;

        }

        Font::Font(UnlocalizedString fontName) : m_fontName(std::move(fontName)) {
            if (impl::s_defaultFont == nullptr)
                impl::s_defaultFont = this;
        }

        void Font::push(float size) const {
            push(size, getFont(m_fontName).regular);
        }

        void Font::pushBold(float size) const {
            push(size, getFont(m_fontName).bold);
        }

        void Font::pushItalic(float size) const {
            push(size, getFont(m_fontName).italic);
        }

        void Font::push(float size, ImFont* font) const {
            if (font != nullptr) {
                if (size <= 0.0F) {
                    size = font->LegacySize;

                    if (font->Sources[0]->PixelSnapH)
                        size *= System::getGlobalScale();
                    else
                        size *= std::floor(System::getGlobalScale());
                } else {
                    size *= ImGui::GetCurrentContext()->FontSizeBase;
                }
            }

            // If no font has been loaded, revert back to the default font to
            // prevent an assertion failure in ImGui
            const auto *ctx = ImGui::GetCurrentContext();
            if (font == nullptr && ctx->Font == nullptr) [[unlikely]] {
                font = ImGui::GetDefaultFont();
            }

            ImGui::PushFont(font, size);
        }


        void Font::pop() const {
            ImGui::PopFont();
        }

        Font::operator ImFont*() const {
            return getFont(m_fontName).regular;
        }

        void registerMergeFont(const std::string &name, const std::span<const u8> &data, Offset offset, std::optional<float> fontSizeMultiplier) {
            impl::s_fonts->emplace_back(
                name,
                data,
                offset,
                fontSizeMultiplier
            );

            #if defined(OS_MACOS)
                macosRegisterFont(data.data(), data.size_bytes());
            #endif
        }

        void registerFont(const Font& font) {
            (*impl::s_fontDefinitions)[font.getUnlocalizedName()] = {};
        }

        FontDefinition getFont(const UnlocalizedString &fontName) {
            auto it = impl::s_fontDefinitions->find(fontName);
            
            if (it == impl::s_fontDefinitions->end()) {
                const auto defaultFont = ImGui::GetDefaultFont();
                return { .regular=defaultFont, .bold=defaultFont, .italic=defaultFont };
            } else
                return it->second;
        }

        void setDefaultFont(const Font& font) {
            impl::s_defaultFont = &font;
        }

        const Font& getDefaultFont() {
            if (*impl::s_defaultFont == nullptr) {
                static Font emptyFont("");
                return emptyFont;
            }
            return **impl::s_defaultFont;
        }

        float getDpi() {
            auto dpi = ImHexApi::System::getNativeScale() * 96.0F;
            return dpi ? dpi : 96.0F;
        }

        float pixelsToPoints(float pixels) {
            return pixels * (72.0 / getDpi());
        }

        float pointsToPixels(float points) {
            return points / (72.0 / getDpi());
        }

    }

}
