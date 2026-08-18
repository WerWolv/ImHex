#pragma once

#include <wolv/types/static_string.hpp>
#include <hex/helpers/binary_pattern.hpp>

namespace hex::prv {

    class Provider;

    class PatternMatcherBase {
    public:
        virtual ~PatternMatcherBase() = default;

        [[nodiscard]] virtual std::string_view getPragma() const = 0;
        [[nodiscard]] virtual bool match(const std::string &parameter) = 0;
    };

    template<wolv::type::StaticString Pragma>
    class PatternMatcher : public PatternMatcherBase {
    public:
        PatternMatcher(Provider *provider) : m_provider(provider) { }
        [[nodiscard]] std::string_view getPragma() const override {
            return Pragma.get();
        }

        [[nodiscard]] Provider* getProvider() const {
            return m_provider;
        }

    private:
        Provider *m_provider;
    };

    class ProviderMatchStrategiesBase {
    public:
        virtual ~ProviderMatchStrategiesBase() = default;
        virtual std::vector<std::shared_ptr<PatternMatcherBase>> createMatchers(Provider *provider) = 0;
    };

    template<typename ... Matchers>
    class ProviderMatchStrategies : public ProviderMatchStrategiesBase {
    public:
        virtual ~ProviderMatchStrategies() = default;

        [[nodiscard]] std::vector<std::shared_ptr<PatternMatcherBase>> createMatchers(Provider *provider) override {
            std::vector<std::shared_ptr<PatternMatcherBase>> result;

            (
                (result.emplace_back(std::make_shared<Matchers>(provider))),
                ...
            );

            return result;
        }
    };

}
