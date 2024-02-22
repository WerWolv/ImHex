#pragma once

#include <hex.hpp>

#include <hex/ui/view.hpp>

#include <hex/api/task_manager.hpp>

#include <content/yara_rule.hpp>

namespace hex::plugin::yara {

    class ViewYara : public View::Window {
    public:
        ViewYara();
        ~ViewYara() override;

        void drawContent() override;

    private:
        PerProvider<std::vector<std::pair<std::fs::path, std::fs::path>>> m_rulePaths;
        PerProvider<std::vector<YaraRule::Rule>> m_matchedRules;
        PerProvider<std::vector<std::string>> m_consoleMessages;
        PerProvider<u32> m_selectedRule;

        PerProvider<std::vector<u32>> m_tooltipIds, m_highlightingIds;

        TaskHolder m_matcherTask;

        void applyRules();
        void clearResult();
    };

}