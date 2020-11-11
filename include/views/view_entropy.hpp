#pragma once

#include "views/view.hpp"

#include <cstdio>

namespace hex {

    namespace prv { class Provider; }

    class ViewEntropy : public View {
    public:
        explicit ViewEntropy(prv::Provider* &dataProvider);
        ~ViewEntropy() override;

        void createView() override;
        void createMenu() override;

    private:
        prv::Provider* &m_dataProvider;
        bool m_windowOpen = true;

        double m_entropy = 0;
        float m_valueCounts[256] = { 0 };
        bool m_shouldInvalidate = true;
    };

}