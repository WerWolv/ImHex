#pragma once

#include <hex.hpp>

#include <hex/api/localization_manager.hpp>
#include <hex/providers/provider.hpp>

#include <functional>
#include <memory>
#include <string>
#include <vector>

EXPORT_MODULE namespace hex {

    /* Provider Registry. Allows adding new data providers to be created from the UI */
    namespace ContentRegistry::Provider {

        namespace impl {

            void addProviderMetadata(const UnlocalizedString &unlocalizedName, const char *icon, std::vector<fs::ItemFilter> validFileExtensions, bool hidden);

            using ProviderCreationFunction = std::function<std::shared_ptr<prv::Provider>()>;
            void add(const std::string &typeName, ProviderCreationFunction creationFunction);

            struct Entry {
                UnlocalizedString unlocalizedName;
                const char *icon;
                std::vector<fs::ItemFilter> validFileExtensions;
                bool hidden;
            };

            const std::vector<Entry>& getEntries();

        }

        /**
         * @brief Adds a new provider to the list of providers
         * @tparam T The provider type that extends hex::prv::Provider
         * @param hidden Whether to hide the provider in the Other Providers list in the welcome screen and File menu
         */
        template<std::derived_from<prv::Provider> T>
        void add(bool hidden = false) {
            const T provider;
            const auto typeName = provider.getTypeName();

            impl::add(typeName, []() -> std::unique_ptr<prv::Provider> {
                return std::make_unique<T>();
            });

            std::vector<fs::ItemFilter> validFileExtensions = {};

            if constexpr (std::derived_from<T, prv::IProviderFilePicker>) {
                validFileExtensions = static_cast<const prv::IProviderFilePicker&>(provider).getValidExtensions();
            }

            impl::addProviderMetadata(typeName, provider.getIcon(), std::move(validFileExtensions), hidden);
        }

    }

}