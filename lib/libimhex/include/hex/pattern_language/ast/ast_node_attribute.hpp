#pragma once

#include <hex/pattern_language/ast/ast_node.hpp>

#include <hex/pattern_language/patterns/pattern_pointer.hpp>
#include <hex/pattern_language/patterns/pattern_array_dynamic.hpp>

namespace hex::pl {

    class ASTNodeAttribute : public ASTNode {
    public:
        explicit ASTNodeAttribute(std::string attribute, std::optional<std::string> value = std::nullopt)
            : ASTNode(), m_attribute(std::move(attribute)), m_value(std::move(value)) { }

        ~ASTNodeAttribute() override = default;

        ASTNodeAttribute(const ASTNodeAttribute &other) : ASTNode(other) {
            this->m_attribute = other.m_attribute;
            this->m_value     = other.m_value;
        }

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeAttribute(*this));
        }

        [[nodiscard]] const std::string &getAttribute() const {
            return this->m_attribute;
        }

        [[nodiscard]] const std::optional<std::string> &getValue() const {
            return this->m_value;
        }

    private:
        std::string m_attribute;
        std::optional<std::string> m_value;
    };


    class Attributable {
    protected:
        Attributable() = default;

        Attributable(const Attributable &other) {
            for (auto &attribute : other.m_attributes) {
                auto copy = attribute->clone();
                if (auto node = dynamic_cast<ASTNodeAttribute *>(copy.get())) {
                    this->m_attributes.push_back(std::unique_ptr<ASTNodeAttribute>(node));
                    (void)copy.release();
                }
            }
        }

    public:
        virtual void addAttribute(std::unique_ptr<ASTNodeAttribute> &&attribute) {
            this->m_attributes.push_back(std::move(attribute));
        }

        [[nodiscard]] const auto &getAttributes() const {
            return this->m_attributes;
        }

        [[nodiscard]] bool hasAttribute(const std::string &key, bool needsParameter) const {
            return std::any_of(this->m_attributes.begin(), this->m_attributes.end(), [&](const std::unique_ptr<ASTNodeAttribute> &attribute) {
                if (attribute->getAttribute() == key) {
                    if (needsParameter && !attribute->getValue().has_value())
                        LogConsole::abortEvaluation(hex::format("attribute '{}' expected a parameter"), attribute);
                    else if (!needsParameter && attribute->getValue().has_value())
                        LogConsole::abortEvaluation(hex::format("attribute '{}' did not expect a parameter "), attribute);
                    else
                        return true;
                }

                return false;
            });
        }

        [[nodiscard]] std::optional<std::string> getAttributeValue(const std::string &key) const {
            auto attribute = std::find_if(this->m_attributes.begin(), this->m_attributes.end(), [&](const std::unique_ptr<ASTNodeAttribute> &attribute) {
                return attribute->getAttribute() == key;
            });

            if (attribute != this->m_attributes.end())
                return (*attribute)->getValue();
            else
                return std::nullopt;
        }

    private:
        std::vector<std::unique_ptr<ASTNodeAttribute>> m_attributes;
    };


    inline void applyTypeAttributes(Evaluator *evaluator, const ASTNode *node, Pattern *pattern) {
        auto attributable = dynamic_cast<const Attributable *>(node);
        if (attributable == nullptr)
            LogConsole::abortEvaluation("attribute cannot be applied here", node);

        if (attributable->hasAttribute("inline", false)) {
            auto inlinable = dynamic_cast<Inlinable *>(pattern);

            if (inlinable == nullptr)
                LogConsole::abortEvaluation("inline attribute can only be applied to nested types", node);
            else
                inlinable->setInlined(true);
        }

        if (auto value = attributable->getAttributeValue("format"); value) {
            auto functions = evaluator->getCustomFunctions();
            if (!functions.contains(*value))
                LogConsole::abortEvaluation(hex::format("cannot find formatter function '{}'", *value), node);

            const auto &function = functions[*value];
            if (function.parameterCount != 1)
                LogConsole::abortEvaluation("formatter function needs exactly one parameter", node);

            pattern->setFormatterFunction(function);
        }

        if (auto value = attributable->getAttributeValue("format_entries"); value) {
            auto functions = evaluator->getCustomFunctions();
            if (!functions.contains(*value))
                LogConsole::abortEvaluation(hex::format("cannot find formatter function '{}'", *value), node);

            const auto &function = functions[*value];
            if (function.parameterCount != 1)
                LogConsole::abortEvaluation("formatter function needs exactly one parameter", node);

            auto array = dynamic_cast<PatternArrayDynamic *>(pattern);
            if (array == nullptr)
                LogConsole::abortEvaluation("inline_array attribute can only be applied to array types", node);

            for (const auto &entry : array->getEntries()) {
                entry->setFormatterFunction(function);
            }
        }

        if (auto value = attributable->getAttributeValue("transform"); value) {
            auto functions = evaluator->getCustomFunctions();
            if (!functions.contains(*value))
                LogConsole::abortEvaluation(hex::format("cannot find transform function '{}'", *value), node);

            const auto &function = functions[*value];
            if (function.parameterCount != 1)
                LogConsole::abortEvaluation("transform function needs exactly one parameter", node);

            pattern->setTransformFunction(function);
        }

        if (auto value = attributable->getAttributeValue("pointer_base"); value) {
            auto functions = evaluator->getCustomFunctions();
            if (!functions.contains(*value))
                LogConsole::abortEvaluation(hex::format("cannot find pointer base function '{}'", *value), node);

            const auto &function = functions[*value];
            if (function.parameterCount != 1)
                LogConsole::abortEvaluation("pointer base function needs exactly one parameter", node);

            if (auto pointerPattern = dynamic_cast<PatternPointer *>(pattern)) {
                u128 pointerValue = pointerPattern->getPointedAtAddress();

                auto result = function.func(evaluator, { pointerValue });

                if (!result.has_value())
                    LogConsole::abortEvaluation("pointer base function did not return a value", node);

                pointerPattern->setPointedAtAddress(Token::literalToUnsigned(result.value()) + pointerValue);
            } else {
                LogConsole::abortEvaluation("pointer_base attribute may only be applied to a pointer");
            }
        }

        if (attributable->hasAttribute("hidden", false)) {
            pattern->setHidden(true);
        }

        if (!pattern->hasOverriddenColor()) {
            if (auto colorValue = attributable->getAttributeValue("color"); colorValue) {
                u32 color = strtoul(colorValue->c_str(), nullptr, 16);
                pattern->setColor(hex::changeEndianess(color, std::endian::big) >> 8);
            } else if (auto singleColor = attributable->hasAttribute("single_color", false); singleColor) {
                pattern->setColor(ContentRegistry::PatternLanguage::getNextColor());
            }
        }
    }

    inline void applyVariableAttributes(Evaluator *evaluator, const ASTNode *node, Pattern *pattern) {
        auto attributable = dynamic_cast<const Attributable *>(node);
        if (attributable == nullptr)
            LogConsole::abortEvaluation("attribute cannot be applied here", node);

        auto endOffset          = evaluator->dataOffset();
        evaluator->dataOffset() = pattern->getOffset();
        ON_SCOPE_EXIT { evaluator->dataOffset() = endOffset; };

        applyTypeAttributes(evaluator, node, pattern);

        if (auto colorValue = attributable->getAttributeValue("color"); colorValue) {
            u32 color = strtoul(colorValue->c_str(), nullptr, 16);
            pattern->setColor(hex::changeEndianess(color, std::endian::big) >> 8);
        } else if (auto singleColor = attributable->hasAttribute("single_color", false); singleColor) {
            pattern->setColor(ContentRegistry::PatternLanguage::getNextColor());
        }

        if (auto value = attributable->getAttributeValue("name"); value) {
            pattern->setDisplayName(*value);
        }

        if (auto value = attributable->getAttributeValue("comment"); value) {
            pattern->setComment(*value);
        }

        if (attributable->hasAttribute("no_unique_address", false)) {
            endOffset -= pattern->getSize();
        }
    }

}