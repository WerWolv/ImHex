#pragma once

#include <hex.hpp>

#include <list>
#include <optional>
#include <string>
#include <vector>
#include <variant>
#include <map>

#include <hex/helpers/concepts.hpp>
#include <hex/api/task.hpp>
#include <hex/api/keybinding.hpp>

#include <wolv/io/fs.hpp>

using ImGuiID = unsigned int;
struct ImVec2;

namespace hex {

    namespace prv {
        class Provider;
    }

    namespace ImHexApi {

        /* Functions to query information from the Hex Editor and interact with it */
        namespace HexEditor {

            using TooltipFunction = std::function<void(u64, const u8*, size_t)>;

            class Highlighting {
            public:
                Highlighting() = default;
                Highlighting(Region region, color_t color);

                [[nodiscard]] const Region &getRegion() const { return this->m_region; }
                [[nodiscard]] const color_t &getColor() const { return this->m_color; }

            private:
                Region m_region = {};
                color_t m_color = 0x00;
            };

            class Tooltip {
            public:
                Tooltip() = default;
                Tooltip(Region region, std::string value, color_t color);

                [[nodiscard]] const Region &getRegion() const { return this->m_region; }
                [[nodiscard]] const color_t &getColor() const { return this->m_color; }
                [[nodiscard]] const std::string &getValue() const { return this->m_value; }

            private:
                Region m_region = {};
                std::string m_value;
                color_t m_color = 0x00;
            };

            struct ProviderRegion : public Region {
                prv::Provider *provider;

                [[nodiscard]] prv::Provider *getProvider() const { return this->provider; }

                [[nodiscard]] Region getRegion() const { return { this->address, this->size }; }
            };

            namespace impl {

                using HighlightingFunction = std::function<std::optional<color_t>(u64, const u8*, size_t, bool)>;

                std::map<u32, Highlighting> &getBackgroundHighlights();
                std::map<u32, HighlightingFunction> &getBackgroundHighlightingFunctions();
                std::map<u32, Highlighting> &getForegroundHighlights();
                std::map<u32, HighlightingFunction> &getForegroundHighlightingFunctions();
                std::map<u32, Tooltip> &getTooltips();
                std::map<u32, TooltipFunction> &getTooltipFunctions();

                void setCurrentSelection(std::optional<ProviderRegion> region);
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
             * @brief Checks if there's a valid selection in the Hex Editor right now
             */
            bool isSelectionValid();

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

        }

        /* Functions to interact with Bookmarks */
        namespace Bookmarks {

            struct Entry {
                Region region;

                std::string name;
                std::string comment;
                u32 color;
                bool locked;
            };

            /**
             * @brief Adds a new bookmark
             * @param address The address of the bookmark
             * @param size The size of the bookmark
             * @param name The name of the bookmark
             * @param comment The comment of the bookmark
             * @param color The color of the bookmark or 0x00 for the default color
             */
            void add(u64 address, size_t size, const std::string &name, const std::string &comment, color_t color = 0x00000000);

        }

        /* Functions to interact with the loaded data provider */
        namespace Provider {

            namespace impl {

                void resetClosingProvider();
                prv::Provider* getClosingProvider();

            }

            /**
             * @brief Gets the currently selected data provider
             * @return The currently selected data provider
             */
            prv::Provider *get();

            /**
             * @brief Gets a list of all currently loaded data providers
             * @return The currently loaded data providers
             */
            const std::vector<prv::Provider *> &getProviders();

            /**
             * @brief Sets the currently selected data provider
             * @param index Index of the provider to select
             */
            void setCurrentProvider(u32 index);

            /**
             * @brief Checks whether the currently selected data provider is valid
             * @return Whether the currently selected data provider is valid
             */
            bool isValid();


            /**
             * @brief Marks the currently selected data provider as dirty
             */
            void markDirty();

            /**
             * @brief Marks the currently selected data provider as clean
             */
            void resetDirty();

            /**
             * @brief Checks whether the currently selected data provider is dirty
             * @return Whether the currently selected data provider is dirty
             */
            bool isDirty();


            /**
             * @brief Adds a newly created provider to the list of providers
             * @param provider The provider to add
             * @param skipLoadInterface Whether to skip loading the provider's loading interface
             */
            void add(prv::Provider *provider, bool skipLoadInterface = false);

            /**
             * @brief Creates a new provider and adds it to the list of providers
             * @tparam T The type of the provider to create
             * @param args Arguments to pass to the provider's constructor
             */
            template<std::derived_from<prv::Provider> T>
            void add(auto &&...args) {
                add(new T(std::forward<decltype(args)>(args)...));
            }

            /**
             * @brief Removes a provider from the list of providers
             * @param provider The provider to remove
             * @param noQuestions Whether to skip asking the user for confirmation
             */
            void remove(prv::Provider *provider, bool noQuestions = false);

            /**
             * @brief Creates a new provider using its unlocalized name
             * @param unlocalizedName The unlocalized name of the provider to create
             * @param skipLoadInterface Whether to skip loading the provider's loading interface
             */
            prv::Provider* createProvider(const std::string &unlocalizedName, bool skipLoadInterface = false);

        }

        /* Functions to interact with various ImHex system settings */
        namespace System {

            namespace impl {

                void setMainWindowPosition(u32 x, u32 y);
                void setMainWindowSize(u32 width, u32 height);
                void setMainDockSpaceId(ImGuiID id);

                void setGlobalScale(float scale);
                void setNativeScale(float scale);

                void setProgramArguments(int argc, char **argv, char **envp);

                void setBorderlessWindowMode(bool enabled);

                void setCustomFontPath(const std::fs::path &path);
                void setFontSize(float size);

                void setGPUVendor(const std::string &vendor);

                void setPortableVersion(bool enabled);

                void addInitArgument(const std::string &key, const std::string &value = { });
            }

            struct ProgramArguments {
                int argc;
                char **argv;
                char **envp;
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
             * @brief Gets the current program arguments
             * @return The current program arguments
             */
            const ProgramArguments &getProgramArguments();

            /**
             * @brief Gets a program argument
             * @param index The index of the argument to get
             * @return The argument at the given index
             */
            std::optional<std::u8string> getProgramArgument(int index);


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
             * @brief Checks if borderless window mode is enabled currently
             * @return Whether borderless window mode is enabled
             */
            bool isBorderlessWindowModeEnabled();

            /**
             * @brief Gets the init arguments passed to ImHex from the splash screen
             * @return Init arguments
             */
            std::map<std::string, std::string> &getInitArguments();

            constexpr static float DefaultFontSize = 13.0;

            /**
             * @brief Gets the current custom font path
             * @return The current custom font path
             */
            const std::filesystem::path &getCustomFontPath();

            /**
             * @brief Gets the current font size
             * @return The current font size
             */
            float getFontSize();


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
            const std::vector<std::filesystem::path> &getAdditionalFolderPaths();

            /**
             * @brief Sets the additional folder paths
             * @param paths The additional folder paths
             */
            void setAdditionalFolderPaths(const std::vector<std::filesystem::path> &paths);


            /**
             * @brief Gets the current GPU vendor
             * @return The current GPU vendor
             */
            const std::string &getGPUVendor();

            /**
             * @brief Checks if ImHex is running in portable mode
             * @return Whether ImHex is running in portable mode
             */
            bool isPortableVersion();
        }

    }

}
