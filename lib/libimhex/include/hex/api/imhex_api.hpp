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
#include <hex/helpers/fs.hpp>

using ImGuiID = unsigned int;
struct ImVec2;

namespace hex {

    namespace prv {
        class Provider;
    }

    namespace ImHexApi {

        namespace Common {

            void closeImHex(bool noQuestions = false);
            void restartImHex();

        }

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

            namespace impl {

                using HighlightingFunction = std::function<std::optional<color_t>(u64, const u8*, size_t)>;

                std::map<u32, Highlighting> &getBackgroundHighlights();
                std::map<u32, HighlightingFunction> &getBackgroundHighlightingFunctions();
                std::map<u32, Highlighting> &getForegroundHighlights();
                std::map<u32, HighlightingFunction> &getForegroundHighlightingFunctions();
                std::map<u32, Tooltip> &getTooltips();
                std::map<u32, TooltipFunction> &getTooltipFunctions();

            }

            u32 addBackgroundHighlight(const Region &region, color_t color);
            void removeBackgroundHighlight(u32 id);

            u32 addForegroundHighlight(const Region &region, color_t color);
            void removeForegroundHighlight(u32 id);

            u32 addTooltip(Region region, std::string value, color_t color);
            void removeTooltip(u32 id);

            u32 addTooltipProvider(TooltipFunction function);
            void removeTooltipProvider(u32 id);

            u32 addBackgroundHighlightingProvider(const impl::HighlightingFunction &function);
            void removeBackgroundHighlightingProvider(u32 id);

            u32 addForegroundHighlightingProvider(const impl::HighlightingFunction &function);
            void removeForegroundHighlightingProvider(u32 id);

            bool isSelectionValid();
            std::optional<Region> getSelection();
            void setSelection(const Region &region);
            void setSelection(u64 address, size_t size);

        }

        namespace Bookmarks {

            struct Entry {
                Region region;

                std::string name;
                std::string comment;
                u32 color;
                bool locked;
            };

            void add(u64 address, size_t size, const std::string &name, const std::string &comment, color_t color = 0x00000000);

        }

        namespace Provider {

            namespace impl {

                void resetClosingProvider();
                prv::Provider* getClosingProvider();

            }

            prv::Provider *get();
            const std::vector<prv::Provider *> &getProviders();

            void setCurrentProvider(u32 index);

            bool isValid();

            void markDirty();
            void resetDirty();
            bool isDirty();

            void add(prv::Provider *provider, bool skipLoadInterface = false);

            template<std::derived_from<prv::Provider> T>
            void add(auto &&...args) {
                add(new T(std::forward<decltype(args)>(args)...));
            }

            void remove(prv::Provider *provider, bool noQuestions = false);

            prv::Provider* createProvider(const std::string &unlocalizedName, bool skipLoadInterface = false);

        }

        namespace Tasks {

            Task createTask(const std::string &unlocalizedName, u64 maxValue);

            void doLater(const std::function<void()> &function);
            std::vector<std::function<void()>> &getDeferredCalls();

        }

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
            }

            struct ProgramArguments {
                int argc;
                char **argv;
                char **envp;
            };

            enum class Theme {
                Dark    = 1,
                Light   = 2,
                Classic = 3
            };

            const ProgramArguments &getProgramArguments();

            float getTargetFPS();
            void setTargetFPS(float fps);

            float getGlobalScale();
            float getNativeScale();

            ImVec2 getMainWindowPosition();
            ImVec2 getMainWindowSize();
            ImGuiID getMainDockSpaceId();

            bool isBorderlessWindowModeEnabled();

            std::map<std::string, std::string> &getInitArguments();

            constexpr static float DefaultFontSize = 13.0;
            const std::fs::path &getCustomFontPath();
            float getFontSize();

            void setTheme(Theme theme);
            Theme getTheme();

            void enableSystemThemeDetection(bool enabled);
            bool usesSystemThemeDetection();

            const std::vector<std::fs::path> &getAdditionalFolderPaths();
            void setAdditionalFolderPaths(const std::vector<std::fs::path> &paths);

            const std::string &getGPUVendor();

            bool isPortableVersion();
        }

    }

}
