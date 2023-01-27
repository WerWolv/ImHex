#pragma once

#include <hex.hpp>

#include <imgui.h>
#include <hex/ui/view.hpp>

#include <hex/api/task.hpp>

namespace hex::plugin::builtin {

    class ViewYara : public View {
    public:
        ViewYara();
        ~ViewYara() override;

        void drawContent() override;

    private:
        struct YaraMatch {
            std::string identifier;
            std::string variable;
            u64 address;
            size_t size;
            bool wholeDataMatch;

            u32 highlightId;
            u32 tooltipId;
        };

        std::vector<std::pair<std::fs::path, std::fs::path>> m_rules;
        std::vector<YaraMatch> m_matches;
        u32 m_selectedRule = 0;
        TaskHolder m_matcherTask;

        std::vector<std::string> m_consoleMessages;

        void applyRules();
        void clearResult();
    };

}