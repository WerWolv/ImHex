#pragma once

#include <bit>
#include <map>
#include <optional>
#include <vector>

#include <hex/pattern_language/log_console.hpp>
#include <hex/api/content_registry.hpp>

namespace hex::prv { class Provider; }

namespace hex::pl {

    class PatternData;
    class ASTNode;

    class Evaluator {
    public:
        Evaluator() = default;

        std::optional<std::vector<PatternData*>> evaluate(const std::vector<ASTNode*> &ast);

        [[nodiscard]]
        LogConsole& getConsole() {
            return this->m_console;
        }

        struct Scope { PatternData *parent; std::vector<PatternData*>* scope; };
        void pushScope(PatternData *parent, std::vector<PatternData*> &scope) { this->m_scopes.push_back({ parent, &scope }); }
        void popScope() { this->m_scopes.pop_back(); }
        const Scope& getScope(s32 index) {
            static Scope empty;

            if (index > 0 || -index >= this->m_scopes.size()) return empty;
            return this->m_scopes[this->m_scopes.size() - 1 + index];
        }

        const Scope& getGlobalScope() {
            return this->m_scopes.front();
        }

        void setProvider(prv::Provider *provider) {
            this->m_provider = provider;
        }

        [[nodiscard]]
        prv::Provider *getProvider() const {
            return this->m_provider;
        }

        void setDefaultEndian(std::endian endian) {
            this->m_defaultEndian = endian;
        }

        [[nodiscard]]
        std::endian getDefaultEndian() const {
            return this->m_defaultEndian;
        }

        u64& dataOffset() { return this->m_currOffset; }

        bool addCustomFunction(const std::string &name, u32 numParams, const ContentRegistry::PatternLanguageFunctions::Callback &function) {
            const auto [iter, inserted] = this->m_customFunctions.insert({ name, { numParams, function } });

            return inserted;
        }

        [[nodiscard]]
        const std::map<std::string, ContentRegistry::PatternLanguageFunctions::Function>& getCustomFunctions() const {
            return this->m_customFunctions;
        }

        [[nodiscard]]
        std::vector<u8>& getStack() {
            return this->m_stack;
        }

        void createVariable(const std::string &name, ASTNode *type);

        void setVariable(const std::string &name, const Token::Literal& value);

    private:
        u64 m_currOffset;
        prv::Provider *m_provider = nullptr;
        LogConsole m_console;

        std::endian m_defaultEndian = std::endian::native;

        std::vector<Scope> m_scopes;
        std::map<std::string, ContentRegistry::PatternLanguageFunctions::Function> m_customFunctions;
        std::vector<ASTNode*> m_customFunctionDefinitions;
        std::vector<u8> m_stack;
    };

}