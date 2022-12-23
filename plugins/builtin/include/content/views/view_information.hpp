#pragma once

#include <hex/ui/view.hpp>
#include <hex/api/task.hpp>

#include <array>
#include <atomic>
#include <cstdio>
#include <string>
#include <vector>

namespace hex::plugin::builtin {

    class ViewInformation : public View {
    public:
        explicit ViewInformation();
        ~ViewInformation() override;

        void drawContent() override;

    private:
        bool m_dataValid            = false;
        u32 m_blockSize             = 0;
        float m_averageEntropy      = 0;
        float m_highestBlockEntropy = 0;
        std::vector<float> m_blockEntropy;
        u64 m_blockEntropyProcessedCount = 0;

        double m_entropyHandlePosition;

        std::array<ImU64, 256> m_valueCounts = { 0 };
        TaskHolder m_analyzerTask;

        Region m_analyzedRegion = { 0, 0 };

        std::string m_dataDescription;
        std::string m_dataMimeType;

        void analyze();
    };

}