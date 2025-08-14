#pragma once

#include <hex.hpp>

#include <functional>
#include <string>
#include <vector>

EXPORT_MODULE namespace hex {

    #if !defined(HEX_MODULE_EXPORT)
        namespace prv { class Provider; }
    #endif

    /* Reports Registry. Allows adding new sections to exported reports */
    namespace ContentRegistry::Reports {

        namespace impl {

            using Callback = std::function<std::string(prv::Provider*)>;

            struct ReportGenerator {
                Callback callback;
            };

            const std::vector<ReportGenerator>& getGenerators();

        }

        void addReportProvider(impl::Callback callback);

    }

}