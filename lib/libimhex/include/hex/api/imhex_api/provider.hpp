#pragma once

#include <hex.hpp>

#include <hex/api/localization_manager.hpp>
#include <hex/providers/provider.hpp>

#include <set>
#include <vector>
#include <memory>
#include <concepts>

EXPORT_MODULE namespace hex {

    /**
     * Helper methods about the providers
     * @note the "current provider" or "currently selected provider" refers to the currently selected provider in the UI;
     * the provider the user is actually editing.
     */
    namespace ImHexApi::Provider {

        namespace impl {

            void resetClosingProvider();
            std::set<prv::Provider*> getClosingProviders();

        }

        /**
         * @brief Gets the currently selected data provider
         * @return The currently selected data provider, or nullptr is there is none
         */
        prv::Provider *get();

        /**
         * @brief Gets a list of all currently loaded data providers
         * @return The currently loaded data providers
         */
        std::vector<prv::Provider*> getProviders();

        /**
         * @brief Sets the currently selected data provider
         * @param index Index of the provider to select
         */
        void setCurrentProvider(i64 index);

        /**
         * @brief Sets the currently selected data provider
         * @param provider The provider to select
         */
        void setCurrentProvider(NonNull<prv::Provider*> provider);

        /**
         * @brief Gets the index of the currently selected data provider
         * @return Index of the selected provider
         */
        i64 getCurrentProviderIndex();

        /**
         * @brief Checks whether the currently selected data provider is valid
         * @return Whether the currently selected data provider is valid
         */
        bool isValid();


        /**
         * @brief Marks the **currently selected** data provider as dirty
         */
        void markDirty();

        /**
         * @brief Marks **all data providers** as clean
         */
        void resetDirty();

        /**
         * @brief Checks whether **any of the data providers** is dirty
         * @return Whether any data provider is dirty
         */
        bool isDirty();


        /**
         * @brief Adds a newly created provider to the list of providers, and mark it as the selected one.
         * @param provider The provider to add
         * @param skipLoadInterface Whether to skip the provider's loading interface (see property documentation)
         * @param select Whether to select the provider after adding it
         */
        void add(std::shared_ptr<prv::Provider> &&provider, bool skipLoadInterface = false, bool select = true);

        /**
         * @brief Creates a new provider and adds it to the list of providers
         * @tparam T The type of the provider to create
         * @param args Arguments to pass to the provider's constructor
         */
        template<std::derived_from<prv::Provider> T>
        void add(auto &&...args) {
            add(std::make_unique<T>(std::forward<decltype(args)>(args)...));
        }

        /**
         * @brief Removes a provider from the list of providers
         * @param provider The provider to remove
         * @param noQuestions Whether to skip asking the user for confirmation
         */
        void remove(prv::Provider *provider, bool noQuestions = false);

        /**
         * @brief Creates a new provider using its unlocalized name and add it to the list of providers
         * @param unlocalizedName The unlocalized name of the provider to create
         * @param skipLoadInterface Whether to skip the provider's loading interface (see property documentation)
         * @param select Whether to select the provider after adding it
         */
        std::shared_ptr<prv::Provider> createProvider(
            const UnlocalizedString &unlocalizedName,
            bool skipLoadInterface = false,
            bool select = true
        );

        /**
         * @brief Opens a provider, making its data available to ImHex and handling any error that may occur
         * @param provider The provider to open
         */
        void openProvider(std::shared_ptr<prv::Provider> provider);

    }

}