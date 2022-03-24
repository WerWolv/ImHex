#pragma once

#include <hex/pattern_language/ast/ast_node.hpp>
#include <hex/pattern_language/ast/ast_node_attribute.hpp>

namespace hex::pl {

    class ASTNodeTypeDecl : public ASTNode,
                            public Attributable {
    public:
        ASTNodeTypeDecl(std::string name) : m_forwardDeclared(true), m_name(std::move(name)) { }

        ASTNodeTypeDecl(std::string name, std::shared_ptr<ASTNode> type, std::optional<std::endian> endian = std::nullopt)
            : ASTNode(), m_name(std::move(name)), m_type(std::move(type)), m_endian(endian) { }

        ASTNodeTypeDecl(const ASTNodeTypeDecl &other) : ASTNode(other), Attributable(other) {
            this->m_name            = other.m_name;
            this->m_type            = other.m_type;
            this->m_endian          = other.m_endian;
            this->m_forwardDeclared = other.m_forwardDeclared;
        }

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeTypeDecl(*this));
        }

        void setName(const std::string &name) { this->m_name = name; }
        [[nodiscard]] const std::string &getName() const { return this->m_name; }
        [[nodiscard]] const std::shared_ptr<ASTNode> &getType() const {
            if (this->isForwardDeclared())
                LogConsole::abortEvaluation(hex::format("cannot use incomplete type '{}'", this->m_name), this);
            
            return this->m_type;
        }
        [[nodiscard]] std::optional<std::endian> getEndian() const { return this->m_endian; }

        [[nodiscard]] std::unique_ptr<ASTNode> evaluate(Evaluator *evaluator) const override {
            auto type = this->getType()->evaluate(evaluator);

            if (auto attributable = dynamic_cast<Attributable *>(type.get())) {
                for (auto &attribute : this->getAttributes()) {
                    auto copy = attribute->clone();
                    if (auto node = dynamic_cast<ASTNodeAttribute *>(copy.get())) {
                        attributable->addAttribute(std::unique_ptr<ASTNodeAttribute>(node));
                        (void)copy.release();
                    }
                }
            }

            return type;
        }

        [[nodiscard]] std::vector<std::unique_ptr<Pattern>> createPatterns(Evaluator *evaluator) const override {
            auto patterns = this->getType()->createPatterns(evaluator);

            for (auto &pattern : patterns) {
                if (pattern == nullptr)
                    continue;

                if (!this->m_name.empty())
                    pattern->setTypeName(this->m_name);

                if (this->m_endian.has_value())
                    pattern->setEndian(this->m_endian.value());

                applyTypeAttributes(evaluator, this, pattern.get());
            }

            return patterns;
        }

        void addAttribute(std::unique_ptr<ASTNodeAttribute> &&attribute) override {
            if (auto attributable = dynamic_cast<Attributable *>(this->getType().get()); attributable != nullptr) {
                attributable->addAttribute(std::unique_ptr<ASTNodeAttribute>(static_cast<ASTNodeAttribute *>(attribute->clone().release())));
            }

            Attributable::addAttribute(std::move(attribute));
        }

        [[nodiscard]]
        bool isForwardDeclared() const {
            return this->m_forwardDeclared;
        }

        void setType(std::shared_ptr<ASTNode> type) {
            this->m_forwardDeclared = false;
            this->m_type = type;
        }

    private:
        bool m_forwardDeclared = false;
        std::string m_name;
        std::shared_ptr<ASTNode> m_type;
        std::optional<std::endian> m_endian;
    };

}