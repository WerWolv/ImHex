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
        u32 m_selectedRule = 0;
        TaskHolder m_matcherTask;

        std::vector<std::string> m_consoleMessages;

        void applyRules();
        void clearResult();
    };

}