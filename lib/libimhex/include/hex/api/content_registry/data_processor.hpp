#pragma once

#include <hex.hpp>

#include <hex/api/localization_manager.hpp>

#include <functional>
#include <memory>
#include <vector>

EXPORT_MODULE namespace hex {

    #if !defined(HEX_MODULE_EXPORT)
        namespace dp { class Node; }
    #endif

    /* Data Processor Node Registry. Allows adding new processor nodes to be used in the data processor */
    namespace ContentRegistry::DataProcessor {

        namespace impl {

            using CreatorFunction = std::function<std::unique_ptr<dp::Node>()>;

            struct Entry {
                UnlocalizedString unlocalizedCategory;
                UnlocalizedString unlocalizedName;
                CreatorFunction creatorFunction;
            };

            void add(const Entry &entry);

            const std::vector<Entry>& getEntries();
        }


        /**
         * @brief Adds a new node to the data processor
         * @tparam T The custom node class that extends dp::Node
         * @tparam Args Arguments types
         * @param unlocalizedCategory The unlocalized category name of the node
         * @param unlocalizedName The unlocalized name of the node
         * @param args Arguments passed to the constructor of the node
         */
        template<std::derived_from<dp::Node> T, typename... Args>
        void add(const UnlocalizedString &unlocalizedCategory, const UnlocalizedString &unlocalizedName, Args &&...args) {
            add(impl::Entry {
                unlocalizedCategory,
                unlocalizedName,
                [unlocalizedName, ...args = std::forward<Args>(args)]() mutable {
                    auto node = std::make_unique<T>(std::forward<Args>(args)...);
                    node->setUnlocalizedName(unlocalizedName);
                    return node;
                }
            });
        }

        /**
         * @brief Adds a separator to the data processor right click menu
         */
        void addSeparator();

    }

}