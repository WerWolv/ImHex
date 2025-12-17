#include <hex/api/content_registry/reports.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/helpers/utils.hpp>

#include <hex/providers/provider.hpp>

namespace hex::plugin::builtin {

    namespace {

    }

    static void registerReportGenerators() {
        // Generate provider data description report
        ContentRegistry::Reports::addReportProvider([](const prv::Provider *provider) -> std::string {
            std::string result;

            if (auto *dataDescriptionProvider = dynamic_cast<const prv::IProviderDataDescription*>(provider); dataDescriptionProvider != nullptr) {
                auto descriptions = dataDescriptionProvider->getDataDescription();
                if (!descriptions.empty()) {
                    result += "## Data description\n\n";
                    result += "| Type | Value |\n";
                    result += "| ---- | ----- |\n";

                    for (const auto &[type, value] : dataDescriptionProvider->getDataDescription())
                        result += fmt::format("| {} | {} |\n", type, value);
                }
            }


            return result;
        });

        // Generate provider overlays report
        ContentRegistry::Reports::addReportProvider([](prv::Provider *provider) -> std::string {
            std::string result;

            const auto &overlays = provider->getOverlays();
            if (overlays.empty())
                return "";

            result += "## Overlays\n\n";

            for (const auto &overlay : overlays) {
                result += fmt::format("### Overlay 0x{:04X} - 0x{:04X}", overlay->getAddress(), overlay->getAddress() + overlay->getSize() - 1);
                result += "\n\n";

                result += "```\n";
                result += hex::generateHexView(overlay->getAddress(), overlay->getSize(), provider);
                result += "\n```\n\n";
            }

            return result;
        });

    }

}