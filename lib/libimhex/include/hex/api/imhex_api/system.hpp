#pragma once

#include <hex.hpp>

#include <hex/helpers/semantic_version.hpp>
#include <hex/helpers/fs.hpp>
#include <hex/helpers/keys.hpp>

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <map>
#include <utility>

#if !defined(HEX_MODULE_EXPORT)
    using ImGuiID = unsigned int;
    struct ImVec2;
    struct ImFontAtlas;
#endif

EXPORT_MODULE namespace hex {

    #if !defined(HEX_MODULE_EXPORT)
        namespace impl {
            class AutoResetBase;
        }
    #endif

    /* Functions to interact with various ImHex system settings */
    namespace ImHexApi::System {

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

        enum class NativeWindowType {
            Unknown,
            Win32,
            Cocoa,
            X11,
            Wayland
        };

        struct NativeWindow {
            NativeWindowType type = NativeWindowType::Unknown;
            void *handle = nullptr;
            void *display = nullptr;
        };

        class WindowBackend {
        public:
            struct Config {
                std::string title;
                i32 width = 0;
                i32 height = 0;
                i32 glMajor = 3;
                i32 glMinor = 1;
                bool coreProfile = false;
                bool forwardCompatible = true;
                bool resizable = true;
                bool decorated = true;
                bool transparent = false;
                bool visible = false;
                bool maximized = false;
                bool highPixelDensity = true;
                bool scaleToMonitor = true;
                std::string applicationId = "imhex";
                std::string webCanvasSelector = "#canvas";
                i32 swapInterval = 0;
            };

            struct Monitor {
                i32 x = 0;
                i32 y = 0;
                i32 width = 0;
                i32 height = 0;
                float refreshRate = 60.0F;
            };

            struct Callbacks {
                std::function<void(i32, i32)> moved;
                std::function<void(i32, i32)> resized;
                std::function<void(i32, i32)> framebufferResized;
                std::function<void(bool)> focused;
                std::function<void(Keys)> keyPressed;
                std::function<void()> inputActivity;
                std::function<void()> closeRequested;
                std::function<void(const std::fs::path&)> fileDropped;
                std::function<void()> refreshRequested;
            };

            virtual ~WindowBackend() = default;

            virtual bool create(const Config &config, Callbacks callbacks) = 0;
            virtual void destroy() = 0;
            virtual void pollEvents() = 0;
            virtual void waitEvents() = 0;
            virtual bool waitEvents(double timeout) = 0;
            virtual void wakeEventLoop() = 0;

            virtual bool initializeImGui() = 0;
            virtual void shutdownImGui() = 0;
            virtual void newImGuiFrame() = 0;
            virtual void renderImGuiPlatformWindows() = 0;

            virtual void show() = 0;
            virtual void hide() = 0;
            virtual bool shouldClose() const = 0;
            virtual void setShouldClose(bool close) = 0;
            virtual void setPosition(i32 x, i32 y) = 0;
            virtual void setSize(i32 width, i32 height) = 0;
            virtual void setSizeLimits(i32 minWidth, i32 minHeight, std::optional<i32> maxWidth, std::optional<i32> maxHeight) = 0;
            virtual void setResizable(bool enabled) = 0;
            virtual void setTitle(const std::string &title) = 0;
            virtual void setAlwaysOnTop(bool enabled) = 0;
            virtual bool isAlwaysOnTop() const = 0;
            virtual void setFullscreen(bool enabled) = 0;
            virtual bool isFullscreen() const = 0;
            virtual void minimize() = 0;
            virtual void maximize() = 0;
            virtual void restore() = 0;
            virtual bool isMaximized() const = 0;
            virtual bool isMinimized() const = 0;
            virtual bool isVisible() const = 0;
            virtual bool isFocused() const = 0;
            virtual void focus() = 0;
            virtual void requestAttention() = 0;
            virtual void setOpacity(float opacity) = 0;

            virtual InitialWindowProperties getWindowProperties() const = 0;
            virtual std::pair<i32, i32> getFramebufferSize() const = 0;
            virtual float getContentScale() const = 0;
            virtual float getBackingScaleFactor() const = 0;
            virtual std::optional<Monitor> getPrimaryMonitor() const = 0;
            virtual NativeWindow getNativeWindow() const = 0;
            virtual bool isWayland() const = 0;
            virtual const char* getName() const = 0;

            virtual void makeContextCurrent() = 0;
            virtual void swapBuffers() = 0;
        };

        namespace impl {

            void setMainInstanceStatus(bool status);

            void setMainWindowPosition(i32 x, i32 y);
            void setMainWindowSize(u32 width, u32 height);
            void setMainDockSpaceId(ImGuiID id);
            void setMainWindowFocusState(bool focused);
            void setWindowBackend(WindowBackend *backend);
            WindowBackend* getWindowBackend();
            NativeWindow getNativeWindow();

            void setGlobalScale(float scale);
            void setNativeScale(float scale);

            void setBorderlessWindowMode(bool enabled);
            void setMultiWindowMode(bool enabled);
            void setInitialWindowProperties(InitialWindowProperties properties);

            void setGPUVendor(const std::string &vendor);
            void setGLRenderer(const std::string &renderer);
            void setGLVersion(SemanticVersion version);

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
        InitialWindowProperties getMainWindowProperties();

        /**
         * @brief Gets the current main dock space ID
         * @return ID of the main dock space
         */
        ImGuiID getMainDockSpaceId();

        /**
         * @brief Checks if the main window is currently focused
         * @return Whether the main window is focused
         */
        bool isMainWindowFocused();

        /**
         * @brief Checks if borderless window mode is enabled currently
         * @return Whether borderless window mode is enabled
         */
        bool isBorderlessWindowModeEnabled();

        /**
         * @brief Checks if multi-window mode is enabled currently
         * @return Whether multi-window mode is enabled
         */
        bool isMultiWindowModeEnabled();

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
        const std::vector<std::fs::path>& getAdditionalFolderPaths();

        /**
         * @brief Sets the additional folder paths
         * @param paths The additional folder paths
         */
        void setAdditionalFolderPaths(const std::vector<std::fs::path> &paths);


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
         * @brief Gets the current OpenGL version
         * @return The current OpenGL version
         */
        const SemanticVersion& getGLVersion();

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
        const SemanticVersion& getImHexVersion();

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

        /**
         * @brief Checks if there's an update available for the current version of ImHex
         * @return Optional string returning the version string of the new version, or std::nullopt if no update is available
         */
        std::optional<std::string> checkForUpdate();

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
        void resizeMainWindow(i32 width, i32 height);

        void setMainWindowTitle(const std::string &title);
        void setMainWindowAlwaysOnTop(bool enabled);
        bool isMainWindowAlwaysOnTop();
        void setMainWindowFullscreen(bool enabled);
        bool isMainWindowFullscreen();
        void minimizeMainWindow();
        void maximizeMainWindow();
        void restoreMainWindow();
        bool isMainWindowMaximized();
        void focusMainWindow();
        void requestMainWindowAttention();
        void cancelMainWindowClose();
        void handleMainWindowTitlebarDoubleClick();
        void setMainWindowMovable(bool movable);

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

        /**
         * @brief Sets the current post-processing shader to use
         * @param vertexShader The vertex shader to use
         * @param fragmentShader The fragment shader to use
         */
        void setPostProcessingShader(const std::string &vertexShader, const std::string &fragmentShader);

    }

}
