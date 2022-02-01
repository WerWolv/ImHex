#pragma once

#include <hex.hpp>
#include <hex/ui/view.hpp>

#include <imgui.h>

#include <vector>
#include <tuple>
#include <cstdio>

namespace hex::plugin::builtin {

    class ViewCommandPalette : public View {
    public:
        ViewCommandPalette();
        ~ViewCommandPalette() override;

        void drawContent() override;

        [[nodiscard]] bool isAvailable() const override { return true; }
        [[nodiscard]] bool shouldProcess() const override { return true; }

        [[nodiscard]] bool hasViewMenuItemEntry() const override { return false; }
        [[nodiscard]] ImVec2 getMinSize() const override { return { 400, 100 }; }
        [[nodiscard]] ImVec2 getMaxSize() const override { return { 400, 100 }; }

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
        bool m_justOpened         = false;
        bool m_focusInputTextBox  = false;

        std::vector<char> m_commandBuffer;
        std::vector<CommandResult> m_lastResults;
        std::string m_exactResult;

        void focusInputTextBox() {
            this->m_focusInputTextBox = true;
        }

        std::vector<CommandResult> getCommandResults(const std::string &command);
    };

}