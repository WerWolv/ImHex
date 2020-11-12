#pragma once

#include "views/view.hpp"

#include <array>
#include <cstdio>
#include <string>
#include <vector>

namespace hex {

    namespace prv { class Provider; }

    class ViewInformation : public View {
    public:
        explicit ViewInformation(prv::Provider* &dataProvider);
        ~ViewInformation() override;

        void createView() override;
        void createMenu() override;

    private:
        prv::Provider* &m_dataProvider;
        bool m_windowOpen = true;

        float m_averageEntropy = 0;
        float m_highestBlockEntropy = 0;
        std::vector<float> m_blockEntropy;

        std::array<float, 256> m_valueCounts = { 0 };
        bool m_shouldInvalidate = true;

        std::string m_fileDescription;
        std::string m_mimeType;
    };

}