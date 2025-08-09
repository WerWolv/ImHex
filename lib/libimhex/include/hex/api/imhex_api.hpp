#pragma once

#include <hex.hpp>
#include <hex/api/localization_manager.hpp>
#include <hex/helpers/semantic_version.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/helpers/fs.hpp>

#include <chrono>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>
#include "imgui_internal.h"

#if !defined(HEX_MODULE_EXPORT)
    using ImGuiID = unsigned int;
    struct ImVec2;
    struct ImFontAtlas;
    struct ImFont;
#endif
struct GLFWwindow;

EXPORT_MODULE namespace hex {

    #if !defined(HEX_MODULE_EXPORT)
        namespace impl {
            class AutoResetBase;
        }

        namespace prv {
            class Provider;
        }
    #endif

    namespace ImHexApi {

        /* Functions to query information from the Hex Editor and interact with it */
        namespace HexEditor {

            using TooltipFunction = std::function<void(u64, const u8*, size_t)>;

            class Highlighting {
            public:
                Highlighting() = default;
                Highlighting(Region region, color_t color);

                [[nodiscard]] const Region& getRegion() const { return m_region; }
                [[nodiscard]] const color_t& getColor() const { return m_color; }

            private:
                Region m_region = {};
                color_t m_color = 0x00;
            };

            class Tooltip {
            public:
                Tooltip() = default;
                Tooltip(Region region, std::string value, color_t color);

                [[nodiscard]] const Region& getRegion() const { return m_region; }
                [[nodiscard]] const color_t& getColor() const { return m_color; }
                [[nodiscard]] const std::string& getValue() const { return m_value; }

            private:
                Region m_region = {};
                std::string m_value;
                color_t m_color = 0x00;
            };

            struct ProviderRegion : Region {
                prv::Provider *provider;

                [[nodiscard]] prv::Provider *getProvider() const { return this->provider; }

                [[nodiscard]] Region getRegion() const { return { this->address, this->size }; }
            };

            namespace impl {

                using HighlightingFunction = std::function<std::optional<color_t>(u64, const u8*, size_t, bool)>;
                using HoveringFunction = std::function<std::set<Region>(const prv::Provider *, u64, size_t)>;

                const std::map<u32, Highlighting>& getBackgroundHighlights();
                const std::map<u32, HighlightingFunction>& getBackgroundHighlightingFunctions();
                const std::map<u32, Highlighting>& getForegroundHighlights();
                const std::map<u32, HighlightingFunction>& getForegroundHighlightingFunctions();
                const std::map<u32, HoveringFunction>& getHoveringFunctions();
                const std::map<u32, Tooltip>& getTooltips();
                const std::map<u32, TooltipFunction>& getTooltipFunctions();

                void setCurrentSelection(const std::optional<ProviderRegion> &region);
                void setHoveredRegion(const prv::Provider *provider, const Region &region);
            }

            /**
             * @brief Adds a background color highlighting to the Hex Editor
             * @param region The region to highlight
             * @param color The color to use for the highlighting
             * @return Unique ID used to remove the highlighting again later
             */
            u32 addBackgroundHighlight(const Region &region, color_t color);

            /**
             * @brief Removes a background color highlighting from the Hex Editor
             * @param id The ID of the highlighting to remove
             */
            void removeBackgroundHighlight(u32 id);


            /**
             * @brief Adds a foreground color highlighting to the Hex Editor
             * @param region The region to highlight
             * @param color The color to use for the highlighting
             * @return Unique ID used to remove the highlighting again later
             */
            u32 addForegroundHighlight(const Region &region, color_t color);

            /**
             * @brief Removes a foreground color highlighting from the Hex Editor
             * @param id The ID of the highlighting to remove
             */
            void removeForegroundHighlight(u32 id);

            /**
             * @brief Adds a hover tooltip to the Hex Editor
             * @param region The region to add the tooltip to
             * @param value Text to display in the tooltip
             * @param color The color of the tooltip
             * @return Unique ID used to remove the tooltip again later
             */
            u32 addTooltip(Region region, std::string value, color_t color);

            /**
             * @brief Removes a hover tooltip from the Hex Editor
             * @param id The ID of the tooltip to remove
             */
            void removeTooltip(u32 id);


            /**
             * @brief Adds a background color highlighting to the Hex Editor using a callback function
             * @param function Function that draws the highlighting based on the hovered region
             * @return Unique ID used to remove the highlighting again later
             */
            u32 addTooltipProvider(TooltipFunction function);

            /**
             * @brief Removes a background color highlighting from the Hex Editor
             * @param id The ID of the highlighting to remove
             */
            void removeTooltipProvider(u32 id);


            /**
             * @brief Adds a background color highlighting to the Hex Editor using a callback function
             * @param function Function that draws the highlighting based on the hovered region
             * @return Unique ID used to remove the highlighting again later
             */
            u32 addBackgroundHighlightingProvider(const impl::HighlightingFunction &function);

            /**
             * @brief Removes a background color highlighting from the Hex Editor
             * @param id The ID of the highlighting to remove
             */
            void removeBackgroundHighlightingProvider(u32 id);


            /**
             * @brief Adds a foreground color highlighting to the Hex Editor using a callback function
             * @param function Function that draws the highlighting based on the hovered region
             * @return Unique ID used to remove the highlighting again later
             */
            u32 addForegroundHighlightingProvider(const impl::HighlightingFunction &function);

            /**
             * @brief Removes a foreground color highlighting from the Hex Editor
             * @param id The ID of the highlighting to remove
             */
            void removeForegroundHighlightingProvider(u32 id);

            /**
             * @brief Adds a hovering provider to the Hex Editor using a callback function
             * @param function Function that draws the highlighting based on the hovered region
             * @return Unique ID used to remove the highlighting again later
             */
            u32 addHoverHighlightProvider(const impl::HoveringFunction &function);

            /**
             * @brief Removes a hovering color highlighting from the Hex Editor
             * @param id The ID of the highlighting to remove
             */
            void removeHoverHighlightProvider(u32 id);

            /**
             * @brief Checks if there's a valid selection in the Hex Editor right now
             */
            bool isSelectionValid();

            /**
             * @brief Clears the current selection in the Hex Editor
             */
            void clearSelection();

            /**
             * @brief Gets the current selection in the Hex Editor
             * @return The current selection
             */
            std::optional<ProviderRegion> getSelection();

            /**
             * @brief Sets the current selection in the Hex Editor
             * @param region The region to select
             * @param provider The provider to select the region in
             */
            void setSelection(const Region &region, prv::Provider *provider = nullptr);

            /**
             * @brief Sets the current selection in the Hex Editor
             * @param region The region to select
             */
            void setSelection(const ProviderRegion &region);

            /**
             * @brief Sets the current selection in the Hex Editor
             * @param address The address to select
             * @param size The size of the selection
             * @param provider The provider to select the region in
             */
            void setSelection(u64 address, size_t size, prv::Provider *provider = nullptr);

            /**
             * @brief Adds a virtual file to the list in the Hex Editor
             * @param path The path of the file
             * @param data The data of the file
             * @param region The location of the file in the Hex Editor if available
             */
            void addVirtualFile(const std::fs::path &path, std::vector<u8> data, Region region = Region::Invalid());

            /**
             * @brief Gets the currently hovered cell region in the Hex Editor
             * @return
             */
            const std::optional<Region>& getHoveredRegion(const prv::Provider *provider);

        }

        /* Functions to interact with Bookmarks */
        namespace Bookmarks {

            struct Entry {
                Region region;

                std::string name;
                std::string comment;
                u32 color;
                bool locked;
                u64 id;
            };

            /**
             * @brief Adds a new bookmark
             * @param address The address of the bookmark
             * @param size The size of the bookmark
             * @param name The name of the bookmark
             * @param comment The comment of the bookmark
             * @param color The color of the bookmark or 0x00 for the default color
             * @return Bookmark ID
             */
            u64 add(u64 address, size_t size, const std::string &name, const std::string &comment, color_t color = 0x00000000);

            /**
            * @brief Adds a new bookmark
            * @param region The region of the bookmark
            * @param name The name of the bookmark
            * @param comment The comment of the bookmark
            * @param color The color of the bookmark or 0x00 for the default color
            * @return Bookmark ID
            */
            u64 add(Region region, const std::string &name, const std::string &comment, color_t color = 0x00000000);

            /**
            * @brief Removes a bookmark
            * @param id The ID of the bookmark to remove
            */
            void remove(u64 id);

        }

        /**
         * Helper methods about the providers
         * @note the "current provider" or "currently selected provider" refers to the currently selected provider in the UI;
         * the provider the user is actually editing.
         */
        namespace Provider {

            namespace impl {

                void resetClosingProvider();
                std::set<prv::Provider*> getClosingProviders();

            }

            /**
             * @brief Gets the currently selected data provider
             * @return The currently selected data provider, or nullptr is there is none
             */
            prv::Provider *get();

            /**
             * @brief Gets a list of all currently loaded data providers
             * @return The currently loaded data providers
             */
            std::vector<prv::Provider*> getProviders();

            /**
             * @brief Sets the currently selected data provider
             * @param index Index of the provider to select
             */
            void setCurrentProvider(i64 index);

            /**
             * @brief Sets the currently selected data provider
             * @param provider The provider to select
             */
            void setCurrentProvider(NonNull<prv::Provider*> provider);

            /**
             * @brief Gets the index of the currently selected data provider
             * @return Index of the selected provider
             */
            i64 getCurrentProviderIndex();

            /**
             * @brief Checks whether the currently selected data provider is valid
             * @return Whether the currently selected data provider is valid
             */
            bool isValid();


            /**
             * @brief Marks the **currently selected** data provider as dirty
             */
            void markDirty();

            /**
             * @brief Marks **all data providers** as clean
             */
            void resetDirty();

            /**
             * @brief Checks whether **any of the data providers** is dirty
             * @return Whether any data provider is dirty
             */
            bool isDirty();


            /**
             * @brief Adds a newly created provider to the list of providers, and mark it as the selected one.
             * @param provider The provider to add
             * @param skipLoadInterface Whether to skip the provider's loading interface (see property documentation)
             * @param select Whether to select the provider after adding it
             */
            void add(std::unique_ptr<prv::Provider> &&provider, bool skipLoadInterface = false, bool select = true);

            /**
             * @brief Creates a new provider and adds it to the list of providers
             * @tparam T The type of the provider to create
             * @param args Arguments to pass to the provider's constructor
             */
            template<std::derived_from<prv::Provider> T>
            void add(auto &&...args) {
                add(std::make_unique<T>(std::forward<decltype(args)>(args)...));
            }

            /**
             * @brief Removes a provider from the list of providers
             * @param provider The provider to remove
             * @param noQuestions Whether to skip asking the user for confirmation
             */
            void remove(prv::Provider *provider, bool noQuestions = false);

            /**
             * @brief Creates a new provider using its unlocalized name and add it to the list of providers
             * @param unlocalizedName The unlocalized name of the provider to create
             * @param skipLoadInterface Whether to skip the provider's loading interface (see property documentation)
             * @param select Whether to select the provider after adding it
             */
            prv::Provider* createProvider(
                const UnlocalizedString &unlocalizedName,
                bool skipLoadInterface = false,
                bool select = true
            );

        }

        /* Functions to interact with various ImHex system settings */
        namespace System {

            struct ProgramArguments {
                int argc;
                char **argv;
                char **envp;
            };

            struct InitialWindowProperties {
                i32 x, y;
                u32 width, height;
                bool maximized;
            };

            enum class TaskProgressState {
                Reset,
                Progress,
                Flash
            };

            enum class TaskProgressType {
                Normal,
                Warning,
                Error
            };

            namespace impl {

                void setMainInstanceStatus(bool status);

                void setMainWindowPosition(i32 x, i32 y);
                void setMainWindowSize(u32 width, u32 height);
                void setMainDockSpaceId(ImGuiID id);
                void setMainWindowHandle(GLFWwindow *window);

                void setGlobalScale(float scale);
                void setNativeScale(float scale);

                void setBorderlessWindowMode(bool enabled);
                void setMultiWindowMode(bool enabled);
                void setInitialWindowProperties(InitialWindowProperties properties);

                void setGPUVendor(const std::string &vendor);
                void setGLRenderer(const std::string &renderer);

                void addInitArgument(const std::string &key, const std::string &value = { });

                void setLastFrameTime(double time);

                bool isWindowResizable();

                void addAutoResetObject(hex::impl::AutoResetBase *object);
                void removeAutoResetObject(hex::impl::AutoResetBase *object);

                void cleanup();

                bool frameRateUnlockRequested();
                void resetFrameRateUnlockRequested();

            }

            /**
             * @brief Closes ImHex
             * @param noQuestions Whether to skip asking the user for confirmation
             */
            void closeImHex(bool noQuestions = false);

            /**
             * @brief Restarts ImHex
             */
            void restartImHex();

            /**
             * @brief Sets the progress bar in the task bar
             * @param state The state of the progress bar
             * @param type The type of the progress bar progress
             * @param progress The progress of the progress bar
             */
            void setTaskBarProgress(TaskProgressState state, TaskProgressType type, u32 progress);


            /**
             * @brief Gets the current target FPS
             * @return The current target FPS
             */
            float getTargetFPS();

            /**
             * @brief Sets the target FPS
             * @param fps The target FPS
             */
            void setTargetFPS(float fps);


            /**
             * @brief Gets the current global scale
             * @return The current global scale
             */
            float getGlobalScale();

            /**
             * @brief Gets the current native scale
             * @return The current native scale
             */
            float getNativeScale();

            float getBackingScaleFactor();

            /**
             * @brief Gets the current main window position
             * @return Position of the main window
             */
            ImVec2 getMainWindowPosition();

            /**
             * @brief Gets the current main window size
             * @return Size of the main window
             */
            ImVec2 getMainWindowSize();

            /**
             * @brief Gets the current main dock space ID
             * @return ID of the main dock space
             */
            ImGuiID getMainDockSpaceId();

            /**
             * @brief Gets the main window's GLFW window handle
             * @return GLFW window handle
             */
            GLFWwindow* getMainWindowHandle();

            /**
             * @brief Checks if borderless window mode is enabled currently
             * @return Whether borderless window mode is enabled
             */
            bool isBorderlessWindowModeEnabled();

            /**
             * @brief Checks if multi-window mode is enabled currently
             * @return Whether multi-window mode is enabled
             */
            bool isMutliWindowModeEnabled();

            /**
             * @brief Gets the init arguments passed to ImHex from the splash screen
             * @return Init arguments
             */
            const std::map<std::string, std::string>& getInitArguments();

            /**
             * @brief Gets a init arguments passed to ImHex from the splash screen
             * @param key The key of the init argument
             * @return Init argument
            */
            std::string getInitArgument(const std::string &key);

            /**
             * @brief Sets if ImHex should follow the system theme
             * @param enabled Whether to follow the system theme
             */
            void enableSystemThemeDetection(bool enabled);

            /**
             * @brief Checks if ImHex follows the system theme
             * @return Whether ImHex follows the system theme
             */
            bool usesSystemThemeDetection();


            /**
             * @brief Gets the currently set additional folder paths
             * @return The currently set additional folder paths
             */
            const std::vector<std::filesystem::path>& getAdditionalFolderPaths();

            /**
             * @brief Sets the additional folder paths
             * @param paths The additional folder paths
             */
            void setAdditionalFolderPaths(const std::vector<std::filesystem::path> &paths);


            /**
             * @brief Gets the current GPU vendor
             * @return The current GPU vendor
             */
            const std::string& getGPUVendor();

            /**
             * @brief Gets the current GPU vendor
             * @return The current GPU vendor
             */
            const std::string& getGLRenderer();

            /**
             * @brief Checks if ImHex is being run in a "Corporate Environment"
             * This function simply checks for common telltale signs such as if the machine is joined a
             * domain. It's not super accurate, but it's still useful for statistics
             * @return True if it is
             */
            bool isCorporateEnvironment();

            /**
             * @brief Checks if ImHex is running in portable mode
             * @return Whether ImHex is running in portable mode
             */
            bool isPortableVersion();

            /**
             * @brief Gets the current Operating System name
             * @return Operating System name
             */
            std::string getOSName();

            /**
             * @brief Gets the current Operating System version
             * @return Operating System version
             */
            std::string getOSVersion();

            /**
             * @brief Gets the current CPU architecture
             * @return CPU architecture
             */
            std::string getArchitecture();


            struct LinuxDistro {
                std::string name;
                std::string version;
            };
            /**
             * @brief Gets information related to the Linux distribution, if running on Linux
             */
            std::optional<LinuxDistro> getLinuxDistro();

            /**
             * @brief Gets the current ImHex version
             * @return ImHex version
             */
            SemanticVersion getImHexVersion();

            /**
             * @brief Gets the current git commit hash
             * @param longHash Whether to return the full hash or the shortened version
             * @return Git commit hash
             */
            std::string getCommitHash(bool longHash = false);

            /**
             * @brief Gets the current git commit branch
             * @return Git commit branch
             */
            std::string getCommitBranch();

            /**
             * @brief Gets the time ImHex was built
             * @return The time ImHex was built
             */
            std::optional<std::chrono::system_clock::time_point> getBuildTime();

            /**
             * @brief Checks if ImHex was built in debug mode
             * @return True if ImHex was built in debug mode, false otherwise
             */
            bool isDebugBuild();

            /**
             * @brief Checks if this version of ImHex is a nightly build
             * @return True if this version is a nightly, false if it's a release
             */
            bool isNightlyBuild();

            enum class UpdateType {
                Stable,
                Nightly
            };

            /**
             * @brief Triggers the update process
             * @param updateType The update channel
             * @return If the update process was successfully started
             */
            bool updateImHex(UpdateType updateType);

            /**
             * @brief Add a new startup task that will be run while ImHex's splash screen is shown
             * @param name Name to be shown in the UI
             * @param async Whether to run the task asynchronously
             * @param function The function to run
             */
            void addStartupTask(const std::string &name, bool async, const std::function<bool()> &function);

            /**
             * @brief Gets the time the previous frame took
             * @return Previous frame time
             */
            double getLastFrameTime();

            /**
             * @brief Sets the window resizable
             * @param resizable Whether the window should be resizable
             */
            void setWindowResizable(bool resizable);

            /**
             * @brief Checks if this window is the main instance of ImHex
             * @return True if this is the main instance, false if another instance is already running
             */
            bool isMainInstance();

            /**
             * @brief Gets the initial window properties
             * @return Initial window properties
             */
            std::optional<InitialWindowProperties> getInitialWindowProperties();

            /**
             * @brief Gets the module handle of libimhex
             * @return Module handle
             */
            void* getLibImHexModuleHandle();

            /**
             * Adds a new migration routine that will be executed when upgrading from a lower version than specified in migrationVersion
             * @param migrationVersion Upgrade point version
             * @param function Function to run
             */
            void addMigrationRoutine(SemanticVersion migrationVersion, std::function<void()> function);

            /**
             * @brief Unlocks the frame rate temporarily, allowing animations to run smoothly
             */
            void unlockFrameRate();
        }

        /**
         * @brief Cross-instance messaging system
         * This allows you to send messages to the "main" instance of ImHex running, from any other instance
         */
        namespace Messaging {
            
            namespace impl {

                using MessagingHandler = std::function<void(const std::vector<u8> &)>;

                const std::map<std::string, MessagingHandler>& getHandlers();
                void runHandler(const std::string &eventName, const std::vector<u8> &args);

            }

            /**
             * @brief Register the handler for this specific event name
             */
            void registerHandler(const std::string &eventName, const impl::MessagingHandler &handler);
        }

        namespace Fonts {

            struct Offset { float x, y; };

            struct MergeFont {
                std::string name;
                std::vector<u8> fontData;
                Offset offset;
                std::optional<float> fontSizeMultiplier;
            };

            class Font {
            public:
                explicit Font(UnlocalizedString fontName);

                void push(float size = 0.0F) const;
                void pushBold(float size = 0.0F) const;
                void pushItalic(float size = 0.0F) const;

                void pop() const;

                [[nodiscard]] operator ImFont*() const;
                [[nodiscard]] const UnlocalizedString& getUnlocalizedName() const { return m_fontName; }
                
            private:
                void push(float size, ImFont *font) const;

            private:
                UnlocalizedString m_fontName;
            };

            struct FontDefinition {
                ImFont* regular;
                ImFont* bold;
                ImFont* italic;
            };

            namespace impl {

                const std::vector<MergeFont>& getMergeFonts();
                std::map<UnlocalizedString, FontDefinition>& getFontDefinitions();

            }

            void registerMergeFont(const std::fs::path &path, Offset offset = {}, std::optional<float> fontSizeMultiplier = std::nullopt);
            void registerMergeFont(const std::string &name, const std::span<const u8> &data, Offset offset = {}, std::optional<float> fontSizeMultiplier = std::nullopt);

            void registerFont(const Font& font);
            FontDefinition getFont(const UnlocalizedString &fontName);

            void setDefaultFont(const Font& font);
            const Font& getDefaultFont();

            float getDpi();
            float pixelsToPoints(float pixels);
            float pointsToPixels(float points);

        }

    }

}
