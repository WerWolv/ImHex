#pragma once

#include <hex.hpp>

#include <imgui.h>
#include <hex/views/view.hpp>

#include <array>
#include <string>
#include <vector>

namespace hex::plugin::builtin {

    namespace prv { class Provider; }

    class ViewDiff : public View {
    public:
        ViewDiff();
        ~ViewDiff() override;

        void drawContent() override;
        void drawMenu() override;

    private:
        void drawDiffLine(const std::array<int, 2> &providerIds, u64 row) const;

        int m_providerA = -1, m_providerB = -1;

        bool m_greyedOutZeros = true;
        bool m_upperCaseHex = true;
        int m_columnCount = 16;
    };

}