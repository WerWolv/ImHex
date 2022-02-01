#pragma once

#include <hex.hpp>

#include <list>
#include <vector>
#include <string>

#include <hex/helpers/concepts.hpp>
#include <hex/api/task.hpp>
#include <hex/api/keybinding.hpp>

#include <imgui.h>

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

            class Highlighting {
            public:
                Highlighting() = default;
                Highlighting(Region region, color_t color, const std::string &tooltip = "");

                [[nodiscard]] const Region &getRegion() const { return this->m_region; }
                [[nodiscard]] const color_t &getColor() const { return this->m_color; }
                [[nodiscard]] const std::string &getTooltip() const { return this->m_tooltip; }

            private:
                Region m_region;
                color_t m_color;
                std::string m_tooltip;
            };

            u32 addHighlight(const Region &region, color_t color, std::string tooltip = "");
            void removeHighlight(u32 id);
            std::map<u32, Highlighting> &getHighlights();

        }

        namespace Bookmarks {

            struct Entry {
                Region region;

                std::string name;
                std::string comment;
                u32 color;
                bool locked;

                u32 highlightId;
            };

            void add(Region region, const std::string &name, const std::string &comment, color_t color = 0x00000000);
            void add(u64 address, size_t size, const std::string &name, const std::string &comment, color_t color = 0x00000000);

        }

        namespace Provider {

            prv::Provider *get();
            const std::vector<prv::Provider *> &getProviders();

            void setCurrentProvider(u32 index);

            bool isValid();

            void add(prv::Provider *provider);

            template<hex::derived_from<prv::Provider> T>
            void add(auto &&...args) {
                add(new T(std::forward<decltype(args)>(args)...));
            }

            void remove(prv::Provider *provider);

        }

        namespace Tasks {

            Task createTask(const std::string &unlocalizedName, u64 maxValue);

            void doLater(const std::function<void()> &function);
            std::vector<std::function<void()>>& getDeferredCalls();

        }

        namespace System {

            namespace impl {

                void setMainWindowPosition(u32 x, u32 y);
                void setMainWindowSize(u32 width, u32 height);
                void setMainDockSpaceId(ImGuiID id);

                void setGlobalScale(float scale);

                void setProgramArguments(int argc, char **argv, char **envp);
            }

            struct ProgramArguments {
                int argc;
                char **argv;
                char **envp;
            };

            const ProgramArguments& getProgramArguments();

            float getTargetFPS();
            void setTargetFPS(float fps);

            float getGlobalScale();

            ImVec2 getMainWindowPosition();
            ImVec2 getMainWindowSize();
            ImGuiID getMainDockSpaceId();

            std::map<std::string, std::string>& getInitArguments();

        }

    }

}
