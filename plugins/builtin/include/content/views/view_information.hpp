#pragma once

#include <hex/api/content_registry/data_information.hpp>
#include <hex/api/task_manager.hpp>
#include <hex/ui/view.hpp>
#include <ui/widgets.hpp>

namespace hex::plugin::builtin {

    class ViewInformation : public View::Scrolling {
    public:
        explicit ViewInformation();
        ~ViewInformation() override = default;

        void drawContent() override;

        void drawHelpText() override;

    private:
        void analyze();

        struct AnalysisData {
            bool valid = false;

            TaskHolder task;
            const prv::Provider *analyzedProvider = nullptr;
            Region analysisRegion = { 0, 0 };

            ui::RegionType selectionType  = ui::RegionType::EntireData;

            std::list<std::unique_ptr<ContentRegistry::DataInformation::InformationSection>> informationSections;
        };

        PerProvider<AnalysisData> m_analysisData;
        PerProvider<bool> m_settingsCollapsed;
    };

}
