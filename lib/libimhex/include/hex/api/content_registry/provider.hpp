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

            void addProviderName(const UnlocalizedString &unlocalizedName, const char *icon);

            using ProviderCreationFunction = std::function<std::shared_ptr<prv::Provider>()>;
            void add(const std::string &typeName, ProviderCreationFunction creationFunction);

            struct Entry {
                UnlocalizedString unlocalizedName;
                const char *icon;
            };

            const std::vector<Entry>& getEntries();

        }

        /**
         * @brief Adds a new provider to the list of providers
         * @tparam T The provider type that extends hex::prv::Provider
         * @param addToList Whether to display the provider in the Other Providers list in the welcome screen and File menu
         */
        template<std::derived_from<prv::Provider> T>
        void add(bool addToList = true) {
            const T provider;
            auto typeName = provider.getTypeName();

            impl::add(typeName, []() -> std::unique_ptr<prv::Provider> {
                return std::make_unique<T>();
            });

            if (addToList)
                impl::addProviderName(typeName, provider.getIcon());
        }

    }

}