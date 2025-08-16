#pragma once

#include <hex.hpp>

#include <hex/api/localization_manager.hpp>
#include <wolv/container/interval_tree.hpp>

#include <vector>
#include <memory>


EXPORT_MODULE namespace hex {

    #if !defined(HEX_MODULE_EXPORT)
        namespace prv { class Provider; }
    #endif

    /* Diffing Registry. Allows adding new diffing algorithms */
    namespace ContentRegistry::Diffing {

        enum class DifferenceType : u8 {
            Match       = 0,
            Insertion   = 1,
            Deletion    = 2,
            Mismatch    = 3
        };

        using DiffTree = wolv::container::IntervalTree<DifferenceType>;

        class Algorithm {
        public:
            explicit Algorithm(UnlocalizedString unlocalizedName, UnlocalizedString unlocalizedDescription)
                : m_unlocalizedName(std::move(unlocalizedName)),
                  m_unlocalizedDescription(std::move(unlocalizedDescription)) { }

            virtual ~Algorithm() = default;

            virtual std::vector<DiffTree> analyze(prv::Provider *providerA, prv::Provider *providerB) const = 0;
            virtual void drawSettings() { }

            const UnlocalizedString& getUnlocalizedName() const { return m_unlocalizedName; }
            const UnlocalizedString& getUnlocalizedDescription() const { return m_unlocalizedDescription; }

        private:
            UnlocalizedString m_unlocalizedName, m_unlocalizedDescription;
        };

        namespace impl {

            const std::vector<std::unique_ptr<Algorithm>>& getAlgorithms();

            void addAlgorithm(std::unique_ptr<Algorithm> &&hash);

        }

        /**
         * @brief Adds a new hash
         * @tparam T The hash type that extends hex::Hash
         * @param args The arguments to pass to the constructor of the hash
         */
        template<typename T, typename ... Args>
        void addAlgorithm(Args && ... args) {
            impl::addAlgorithm(std::make_unique<T>(std::forward<Args>(args)...));
        }

    }

}