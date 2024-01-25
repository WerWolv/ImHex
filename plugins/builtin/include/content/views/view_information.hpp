#pragma once

#include <hex/ui/view.hpp>
#include <hex/api/task_manager.hpp>

#include "content/helpers/diagrams.hpp"
#include <ui/widgets.hpp>

#include <string>

namespace hex::plugin::builtin {

    class ViewInformation : public View::Window {
    public:
        explicit ViewInformation();
        ~ViewInformation() override;

        void drawContent() override;

    private:
        struct AnalysisData {
            AnalysisData() = default;
            AnalysisData(const AnalysisData&) = default;

            TaskHolder analyzerTask;

            bool dataValid = false;
            u32 blockSize = 0;
            double averageEntropy = -1.0;

            double highestBlockEntropy = -1.0;
            u64 highestBlockEntropyAddress = 0x00;
            double lowestBlockEntropy = -1.0;
            u64 lowestBlockEntropyAddress = 0x00;

            double plainTextCharacterPercentage = -1.0;

            Region analysisRegion = { 0, 0 };
            Region analyzedRegion = { 0, 0 };
            prv::Provider *analyzedProvider = nullptr;

            std::string dataDescription;
            std::string dataMimeType;
            std::string dataAppleCreatorType;
            std::string dataExtensions;

            std::shared_ptr<DiagramDigram> digram;
            std::shared_ptr<DiagramLayeredDistribution> layeredDistribution;
            std::shared_ptr<DiagramByteDistribution> byteDistribution;
            std::shared_ptr<DiagramByteTypesDistribution> byteTypesDistribution;
            std::shared_ptr<DiagramChunkBasedEntropyAnalysis> chunkBasedEntropy;

            u32 inputChunkSize    = 0;
            ui::RegionType selectionType  = ui::RegionType::EntireData;
        };

        PerProvider<AnalysisData> m_analysis;


        void analyze();

    };

}
