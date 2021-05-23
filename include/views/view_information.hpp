#pragma once

#include <hex/views/view.hpp>

#include <array>
#include <cstdio>
#include <string>
#include <vector>

namespace hex {

    namespace prv { class Provider; }

    class ViewInformation : public View {
    public:
        explicit ViewInformation();
        ~ViewInformation() override;

        void drawContent() override;
        void drawMenu() override;

    private:
        bool m_dataValid = false;
        u32 m_blockSize = 0;
        float m_averageEntropy = 0;
        float m_highestBlockEntropy = 0;
        std::vector<float> m_blockEntropy;

        double m_entropyHandlePosition;

        std::array<ImU64, 256> m_valueCounts = { 0 };
        bool m_analyzing = false;

        std::pair<u64, u64> m_analyzedRegion = { 0, 0 };

        std::string m_fileDescription;
        std::string m_mimeType;

        void analyze();
    };

}