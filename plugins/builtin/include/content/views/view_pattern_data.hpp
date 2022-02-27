#pragma once

#include <hex.hpp>

#include <imgui.h>
#include <hex/ui/view.hpp>

#include <vector>
#include <tuple>
#include <cstdio>

namespace hex::plugin::builtin {

    class ViewPatternData : public View {
    public:
        ViewPatternData();
        ~ViewPatternData() override;

        void drawContent() override;

    private:
        std::map<prv::Provider *, std::vector<std::shared_ptr<pl::Pattern>>> m_sortedPatterns;
    };

}