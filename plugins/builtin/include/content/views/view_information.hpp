#pragma once

#include <hex/ui/view.hpp>
#include <hex/api/task.hpp>

#include "content/helpers/diagrams.hpp"
#include <ui/widgets.hpp>

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
        bool m_dataValid = false;
        u32 m_blockSize = 0;
        double m_averageEntropy = -1.0;

        double m_highestBlockEntropy = -1.0;
        u64 m_highestBlockEntropyAddress = 0x00;
        double m_lowestBlockEntropy = -1.0;
        u64 m_lowestBlockEntropyAddress = 0x00;

        double m_plainTextCharacterPercentage = -1.0;

        TaskHolder m_analyzerTask;

        Region m_analysisRegion = { 0, 0 };
        Region m_analyzedRegion = { 0, 0 };

        std::string m_dataDescription;
        std::string m_dataMimeType;

        DiagramDigram m_digram;
        DiagramLayeredDistribution m_layeredDistribution;
        DiagramByteDistribution m_byteDistribution;
        DiagramByteTypesDistribution m_byteTypesDistribution;
        DiagramChunkBasedEntropyAnalysis m_chunkBasedEntropy;

        void analyze();

        int m_inputChunkSize    = 0;
        ui::RegionType m_selectionType  = ui::RegionType::EntireData;
    };

}
