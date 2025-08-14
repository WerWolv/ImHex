#pragma once

#include <hex.hpp>

#include <hex/api/localization_manager.hpp>

#include <map>
#include <string>

EXPORT_MODULE namespace hex {

    /* Experiments Registry. Allows adding new experiments */
    namespace ContentRegistry::Experiments {

        namespace impl {

            struct Experiment {
                UnlocalizedString unlocalizedName, unlocalizedDescription;
                bool enabled;
            };

            const std::map<std::string, Experiment>& getExperiments();
        }

        void addExperiment(
            const std::string &experimentName,
            const UnlocalizedString &unlocalizedName,
            const UnlocalizedString &unlocalizedDescription = ""
        );

        void enableExperiement(const std::string &experimentName, bool enabled);

        [[nodiscard]] bool isExperimentEnabled(const std::string &experimentName);
    }

}