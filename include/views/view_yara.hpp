#pragma once

#include <hex.hpp>

#include <imgui.h>
#include <hex/views/view.hpp>

namespace hex {

    class ViewYara : public View {
    public:
        ViewYara();
        ~ViewYara() override;

        void drawContent() override;
        void drawMenu() override;

    private:
        struct YaraMatch {
            std::string identifier;
            s64 address;
            s32 size;
            bool wholeDataMatch;
        };

        std::vector<std::string> m_rules;
        std::vector<YaraMatch> m_matches;
        u32 m_selectedRule = 0;
        bool m_matching = false;
        std::vector<char> m_errorMessage;

        void reloadRules();
        void applyRules();
    };

}