#pragma once

#include <hex.hpp>

#include <imgui.h>
#include <hex/ui/view.hpp>

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
        };

        std::vector<std::pair<std::string, std::string>> m_rules;
        std::vector<YaraMatch> m_matches;
        u32 m_selectedRule = 0;
        bool m_matching    = false;
        std::vector<char> m_errorMessage;

        void reloadRules();
        void applyRules();
    };

}