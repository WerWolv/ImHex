#include <hex/api/imhex_api.hpp>
#include <hex/api/content_registry.hpp>

#include <hex/api/event.hpp>
#include <hex/providers/provider.hpp>

#include <utility>
#include <unistd.h>

#include <imgui.h>

#include <nlohmann/json.hpp>

namespace hex {

    namespace ImHexApi::Common {

        void closeImHex(bool noQuestions) {
            EventManager::post<RequestCloseImHex>(noQuestions);
        }

        void restartImHex() {
            EventManager::post<RequestCloseImHex>(false);
            std::atexit([] {
                auto &programArgs = ImHexApi::System::getProgramArguments();
                execve(programArgs.argv[0], programArgs.argv, programArgs.envp);
            });
        }

    }


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

        std::optional<Region> getSelection() {
            std::optional<Region> selection;
            EventManager::post<QuerySelection>(selection);

            return selection;
        }

        void setSelection(const Region &region) {
            EventManager::post<RequestSelectionChange>(region);
        }

        void setSelection(u64 address, size_t size) {
            setSelection({ address, size });
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

        static u32 s_currentProvider;
        static std::vector<prv::Provider *> s_providers;

        prv::Provider *get() {
            if (!ImHexApi::Provider::isValid())
                return nullptr;

            return s_providers[s_currentProvider];
        }

        const std::vector<prv::Provider *> &getProviders() {
            return s_providers;
        }

        void setCurrentProvider(u32 index) {
            if (Task::getRunningTaskCount() > 0)
                return;

            if (index < s_providers.size() && s_currentProvider != index) {
                auto oldProvider  = get();
                s_currentProvider = index;
                EventManager::post<EventProviderChanged>(oldProvider, get());
            }
        }

        bool isValid() {
            return !s_providers.empty() && s_currentProvider < s_providers.size();
        }

        void add(prv::Provider *provider) {
            if (Task::getRunningTaskCount() > 0)
                return;

            s_providers.push_back(provider);
            EventManager::post<EventProviderCreated>(provider);

            setCurrentProvider(s_providers.size() - 1);
        }

        void remove(prv::Provider *provider) {
            if (provider == nullptr)
                 return;

            if (Task::getRunningTaskCount() > 0)
                return;

            auto it = std::find(s_providers.begin(), s_providers.end(), provider);
            if (it == s_providers.end())
                return;

            EventManager::post<EventProviderDeleted>(provider);

            s_providers.erase(it);

            if (it - s_providers.begin() == s_currentProvider && !s_providers.empty())
                setCurrentProvider(0);

            delete provider;
        }

    }


    namespace ImHexApi::Tasks {

        Task createTask(const std::string &unlocalizedName, u64 maxValue) {
            return { unlocalizedName, maxValue };
        }

        void doLater(const std::function<void()> &function) {
            static std::mutex tasksMutex;
            std::scoped_lock lock(tasksMutex);

            getDeferredCalls().push_back(function);
        }

        std::vector<std::function<void()>> &getDeferredCalls() {
            static std::vector<std::function<void()>> deferredCalls;

            return deferredCalls;
        }

    }


    namespace ImHexApi::System {

        namespace impl {

            static ImVec2 s_mainWindowPos;
            static ImVec2 s_mainWindowSize;
            void setMainWindowPosition(u32 x, u32 y) {
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


            static ProgramArguments s_programArguments;
            void setProgramArguments(int argc, char **argv, char **envp) {
                s_programArguments.argc = argc;
                s_programArguments.argv = argv;
                s_programArguments.envp = envp;
            }

            static bool s_borderlessWindowMode;
            void setBorderlessWindowMode(bool enabled) {
                s_borderlessWindowMode = enabled;
            }

            static std::fs::path s_customFontPath;
            void setCustomFontPath(const std::fs::path &path) {
                s_customFontPath = path;
            }

            static float s_fontSize = 13.0;
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

        }


        const ProgramArguments &getProgramArguments() {
            return impl::s_programArguments;
        }


        static float s_targetFPS = 60.0F;

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

        const std::fs::path &getCustomFontPath() {
            return impl::s_customFontPath;
        }

        float getFontSize() {
            return impl::s_fontSize;
        }


        static Theme s_theme;
        static bool s_systemThemeDetection;

        void setTheme(Theme theme) {
            s_theme = theme;

            EventManager::post<EventSettingsChanged>();
        }

        Theme getTheme() {
            return s_theme;
        }


        void enableSystemThemeDetection(bool enabled) {
            s_systemThemeDetection = enabled;

            EventManager::post<EventSettingsChanged>();
        }

        bool usesSystemThemeDetection() {
            return s_systemThemeDetection;
        }


        static std::vector<std::fs::path> s_additionalFolderPaths;
        const std::vector<std::fs::path> &getAdditionalFolderPaths() {
            return s_additionalFolderPaths;
        }

        void setAdditionalFolderPaths(const std::vector<std::fs::path> &paths) {
            s_additionalFolderPaths = paths;
        }


        const std::string &getGPUVendor() {
            return impl::s_gpuVendor;
        }

        bool isPortableVersion() {
            return impl::s_portableVersion;
        }
    }

}
