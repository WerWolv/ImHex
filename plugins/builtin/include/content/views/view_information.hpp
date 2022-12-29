#pragma once

#include <hex/ui/view.hpp>
#include <hex/api/task.hpp>

#include "content/helpers/diagrams.hpp"

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
        bool m_dataValid             = false;
        u32 m_blockSize              = 0;
        double m_averageEntropy      = -1.0;
        double m_highestBlockEntropy = -1.0;
        double m_plainTextCharacterPercentage = -1.0;
        std::vector<double> m_blockEntropy;
        std::array<std::vector<float>, 12> m_blockTypeDistributions;
        std::atomic<u64> m_processedBlockCount   = 0;

        double m_diagramHandlePosition = 0.0;

        std::array<ImU64, 256> m_valueCounts = { 0 };
        TaskHolder m_analyzerTask;

        Region m_analyzedRegion = { 0, 0 };

        std::string m_dataDescription;
        std::string m_dataMimeType;

        DiagramDigram m_digram;
        DiagramLayeredDistribution m_layeredDistribution;

        void analyze();
    };

}