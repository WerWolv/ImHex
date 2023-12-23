#pragma once

#include <hex.hpp>

#include <hex/ui/view.hpp>

#include <hex/api/task_manager.hpp>

namespace hex::plugin::yara {

    class ViewYara : public View::Window {
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

            mutable u32 highlightId;
            mutable u32 tooltipId;
        };

    private:
        PerProvider<std::vector<std::pair<std::fs::path, std::fs::path>>> m_rules;
        PerProvider<std::vector<YaraMatch>> m_matches;
        PerProvider<std::vector<YaraMatch*>> m_sortedMatches;

        u32 m_selectedRule = 0;
        TaskHolder m_matcherTask;

        std::vector<std::string> m_consoleMessages;

        void applyRules();
        void clearResult();
    };

}