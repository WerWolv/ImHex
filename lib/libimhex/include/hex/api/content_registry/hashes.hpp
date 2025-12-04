#pragma once

#include <hex.hpp>

#include <hex/api/localization_manager.hpp>

#include <vector>
#include <string>
#include <memory>
#include <functional>

#include <nlohmann/json_fwd.hpp>

EXPORT_MODULE namespace hex {

    #if !defined(HEX_MODULE_EXPORT)
        namespace prv { class Provider; }
    #endif

    /* Hash Registry. Allows adding new hashes to the Hash view */
    namespace ContentRegistry::Hashes {

        class Hash {
        public:
            explicit Hash(UnlocalizedString unlocalizedName) : m_unlocalizedName(std::move(unlocalizedName)) {}
            virtual ~Hash() = default;

            class Function {
            public:
                using Callback = std::function<std::vector<u8>(const Region&, prv::Provider *)>;

                Function(Hash *type, std::string name, Callback callback)
                    : m_type(type), m_name(std::move(name)), m_callback(std::move(callback)) {

                }

                [[nodiscard]] Hash *getType() { return m_type; }
                [[nodiscard]] const Hash *getType() const { return m_type; }
                [[nodiscard]] const std::string& getName() const { return m_name; }

                std::vector<u8> get(const Region& region, prv::Provider *provider) const {
                    return m_callback(region, provider);
                }

            private:
                Hash *m_type;
                std::string m_name;
                Callback m_callback;
            };

            virtual void draw() { }
            [[nodiscard]] virtual Function create(std::string name) = 0;

            [[nodiscard]] virtual nlohmann::json store() const = 0;
            virtual void load(const nlohmann::json &json) = 0;

            [[nodiscard]] const UnlocalizedString& getUnlocalizedName() const {
                return m_unlocalizedName;
            }

        protected:
            [[nodiscard]] Function create(const std::string &name, const Function::Callback &callback) {
                return { this, name, callback };
            }

        private:
            UnlocalizedString m_unlocalizedName;
        };

        namespace impl {

            const std::vector<std::unique_ptr<Hash>>& getHashes();

            void add(std::unique_ptr<Hash> &&hash);

        }


        /**
         * @brief Adds a new hash
         * @tparam T The hash type that extends hex::Hash
         * @param args The arguments to pass to the constructor of the hash
         */
        template<typename T, typename ... Args>
        void add(Args && ... args) {
            impl::add(std::make_unique<T>(std::forward<Args>(args)...));
        }

    }

}