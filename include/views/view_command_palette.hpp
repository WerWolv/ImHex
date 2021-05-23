#pragma once

#include <hex.hpp>
#include <hex/views/view.hpp>

#include <imgui.h>
#include <hex/lang/pattern_data.hpp>

#include <vector>
#include <tuple>
#include <cstdio>

namespace hex {

    namespace prv { class Provider; }

    class ViewCommandPalette : public View {
    public:
        ViewCommandPalette();
        ~ViewCommandPalette() override;

        void drawContent() override;
        void drawMenu() override;
        bool isAvailable() override { return true; }
        bool shouldProcess() override { return true; }

        bool handleShortcut(bool keys[512], bool ctrl, bool shift, bool alt) override;

        bool hasViewMenuItemEntry() override { return false; }
        ImVec2 getMinSize() override { return ImVec2(400, 100); }
        ImVec2 getMaxSize() override { return ImVec2(400, 100); }

    private:
        enum class MatchType {
            NoMatch,
            InfoMatch,
            PartialMatch,
            PerfectMatch
        };

        struct CommandResult {
            std::string displayResult;
            std::string matchedCommand;
            std::function<void(std::string)> executeCallback;
        };

        bool m_commandPaletteOpen = false;
        bool m_justOpened = false;
        bool m_focusInputTextBox = false;

        std::vector<char> m_commandBuffer;
        std::vector<CommandResult> m_lastResults;
        std::string m_exactResult;

        void focusInputTextBox() {
            this->m_focusInputTextBox = true;
        }

        std::vector<CommandResult> getCommandResults(std::string_view command);
    };

}