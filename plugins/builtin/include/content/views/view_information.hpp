#pragma once

#include <hex/api/content_registry/data_information.hpp>
#include <hex/api/task_manager.hpp>
#include <hex/providers/provider_data.hpp>
#include <hex/providers/file_backed_provider_data.hpp>
#include <hex/ui/view.hpp>
#include <ui/widgets.hpp>

#include <nlohmann/json.hpp>

namespace hex::plugin::builtin {

    class ViewInformation : public View::Scrolling {
    public:
        explicit ViewInformation();
        ~ViewInformation() override;

        void drawContent() override;

        void drawHelpText() override;

    private:
        void analyze();

        struct InformationConfig {
            nlohmann::json sections = nlohmann::json::object();
        };

        static FileBackedProviderData<InformationConfig>::SerializedData encodeConfig(const InformationConfig &config);
        static std::optional<InformationConfig> decodeConfig(std::span<const u8> data);

        void applyConfig(prv::Provider *provider);
        void synchronizeConfig(prv::Provider *provider);

        struct AnalysisData {
            bool valid = false;

            TaskHolder task;
            const prv::Provider *analyzedProvider = nullptr;
            Region analysisRegion = { 0, 0 };

            ui::RegionType selectionType  = ui::RegionType::EntireData;

            std::list<std::unique_ptr<ContentRegistry::DataInformation::InformationSection>> informationSections;
        };

        PerProvider<AnalysisData> m_analysisData;
        FileBackedProviderData<InformationConfig> m_informationConfig;
        PerProvider<bool> m_settingsCollapsed;
    };

}
