#pragma once

#include <hex/pattern_language/ast/ast_node.hpp>
#include <hex/pattern_language/ast/ast_node_attribute.hpp>
#include <hex/pattern_language/ast/ast_node_literal.hpp>
#include <hex/pattern_language/ast/ast_node_builtin_type.hpp>
#include <hex/pattern_language/ast/ast_node_while_statement.hpp>

#include <hex/pattern_language/patterns/pattern_padding.hpp>
#include <hex/pattern_language/patterns/pattern_character.hpp>
#include <hex/pattern_language/patterns/pattern_wide_character.hpp>
#include <hex/pattern_language/patterns/pattern_string.hpp>
#include <hex/pattern_language/patterns/pattern_wide_string.hpp>
#include <hex/pattern_language/patterns/pattern_array_dynamic.hpp>
#include <hex/pattern_language/patterns/pattern_array_static.hpp>

namespace hex::pl {

    class ASTNodeArrayVariableDecl : public ASTNode,
                                     public Attributable {
    public:
        ASTNodeArrayVariableDecl(std::string name, std::unique_ptr<ASTNode> &&type, std::unique_ptr<ASTNode> &&size, std::unique_ptr<ASTNode> &&placementOffset = {})
            : ASTNode(), m_name(std::move(name)), m_type(std::move(type)), m_size(std::move(size)), m_placementOffset(std::move(placementOffset)) { }

        ASTNodeArrayVariableDecl(const ASTNodeArrayVariableDecl &other) : ASTNode(other), Attributable(other) {
            this->m_name = other.m_name;
            this->m_type = other.m_type->clone();
            if (other.m_size != nullptr)
                this->m_size = other.m_size->clone();
            else
                this->m_size = nullptr;

            if (other.m_placementOffset != nullptr)
                this->m_placementOffset = other.m_placementOffset->clone();
            else
                this->m_placementOffset = nullptr;
        }

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeArrayVariableDecl(*this));
        }

        [[nodiscard]] std::vector<std::unique_ptr<Pattern>> createPatterns(Evaluator *evaluator) const override {
            auto startOffset = evaluator->dataOffset();

            if (this->m_placementOffset != nullptr) {
                auto evaluatedPlacement = this->m_placementOffset->evaluate(evaluator);
                auto offset             = dynamic_cast<ASTNodeLiteral *>(evaluatedPlacement.get());

                evaluator->dataOffset() = std::visit(overloaded {
                                                         [this](const std::string &) -> u64 { LogConsole::abortEvaluation("placement offset cannot be a string", this); },
                                                         [this](const std::shared_ptr<Pattern> &) -> u64 { LogConsole::abortEvaluation("placement offset cannot be a custom type", this); },
                                                         [](auto &&offset) -> u64 { return offset; } },
                    offset->getValue());
            }

            auto type = this->m_type->evaluate(evaluator);

            std::unique_ptr<Pattern> pattern;
            if (dynamic_cast<ASTNodeBuiltinType *>(type.get()))
                pattern = createStaticArray(evaluator);
            else if (auto attributable = dynamic_cast<Attributable *>(type.get())) {
                bool isStaticType = attributable->hasAttribute("static", false);

                if (isStaticType)
                    pattern = createStaticArray(evaluator);
                else
                    pattern = createDynamicArray(evaluator);
            } else {
                LogConsole::abortEvaluation("invalid type used in array", this);
            }

            applyVariableAttributes(evaluator, this, pattern.get());

            if (this->m_placementOffset != nullptr && !evaluator->isGlobalScope()) {
                evaluator->dataOffset() = startOffset;
            }

            return hex::moveToVector(std::move(pattern));
        }

    private:
        std::string m_name;
        std::unique_ptr<ASTNode> m_type;
        std::unique_ptr<ASTNode> m_size;
        std::unique_ptr<ASTNode> m_placementOffset;

        std::unique_ptr<Pattern> createStaticArray(Evaluator *evaluator) const {
            u64 startOffset = evaluator->dataOffset();

            auto templatePatterns = this->m_type->createPatterns(evaluator);
            auto &templatePattern = templatePatterns.front();

            evaluator->dataOffset() = startOffset;

            i128 entryCount = 0;

            if (this->m_size != nullptr) {
                auto sizeNode = this->m_size->evaluate(evaluator);

                if (auto literal = dynamic_cast<ASTNodeLiteral *>(sizeNode.get())) {
                    entryCount = std::visit(overloaded {
                                                [this](const std::string &) -> i128 { LogConsole::abortEvaluation("cannot use string to index array", this); },
                                                [this](const std::shared_ptr<Pattern> &) -> i128 { LogConsole::abortEvaluation("cannot use custom type to index array", this); },
                                                [](auto &&size) -> i128 { return size; } },
                        literal->getValue());
                } else if (auto whileStatement = dynamic_cast<ASTNodeWhileStatement *>(sizeNode.get())) {
                    while (whileStatement->evaluateCondition(evaluator)) {
                        entryCount++;
                        evaluator->dataOffset() += templatePattern->getSize();
                        evaluator->handleAbort();
                    }
                }

                if (entryCount < 0)
                    LogConsole::abortEvaluation("array cannot have a negative size", this);
            } else {
                std::vector<u8> buffer(templatePattern->getSize());
                while (true) {
                    if (evaluator->dataOffset() > evaluator->getProvider()->getActualSize() - buffer.size())
                        LogConsole::abortEvaluation("reached end of file before finding end of unsized array", this);

                    evaluator->getProvider()->read(evaluator->dataOffset(), buffer.data(), buffer.size());
                    evaluator->dataOffset() += buffer.size();

                    entryCount++;

                    bool reachedEnd = true;
                    for (u8 &byte : buffer) {
                        if (byte != 0x00) {
                            reachedEnd = false;
                            break;
                        }
                    }

                    if (reachedEnd) break;
                    evaluator->handleAbort();
                }
            }

            std::unique_ptr<Pattern> outputPattern;
            if (dynamic_cast<PatternPadding *>(templatePattern.get())) {
                outputPattern = std::unique_ptr<Pattern>(new PatternPadding(evaluator, startOffset, 0));
            } else if (dynamic_cast<PatternCharacter *>(templatePattern.get())) {
                outputPattern = std::unique_ptr<Pattern>(new PatternString(evaluator, startOffset, 0));
            } else if (dynamic_cast<PatternWideCharacter *>(templatePattern.get())) {
                outputPattern = std::unique_ptr<Pattern>(new PatternWideString(evaluator, startOffset, 0));
            } else {
                auto arrayPattern = std::make_unique<PatternArrayStatic>(evaluator, startOffset, 0);
                arrayPattern->setEntries(templatePattern->clone(), entryCount);
                outputPattern = std::move(arrayPattern);
            }

            outputPattern->setVariableName(this->m_name);
            outputPattern->setEndian(templatePattern->getEndian());
            outputPattern->setTypeName(templatePattern->getTypeName());
            outputPattern->setSize(templatePattern->getSize() * entryCount);

            evaluator->dataOffset() = startOffset + outputPattern->getSize();

            return outputPattern;
        }

        std::unique_ptr<Pattern> createDynamicArray(Evaluator *evaluator) const {
            auto arrayPattern = std::make_unique<PatternArrayDynamic>(evaluator, evaluator->dataOffset(), 0);
            arrayPattern->setVariableName(this->m_name);

            std::vector<std::shared_ptr<Pattern>> entries;

            size_t size    = 0;
            u64 entryIndex = 0;

            auto addEntries = [&](std::vector<std::unique_ptr<Pattern>> &&patterns) {
                for (auto &pattern : patterns) {
                    pattern->setVariableName(hex::format("[{}]", entryIndex));
                    pattern->setEndian(arrayPattern->getEndian());

                    size += pattern->getSize();
                    entryIndex++;

                    entries.push_back(std::move(pattern));

                    evaluator->handleAbort();
                }
            };

            auto discardEntries = [&](u32 count) {
                for (u32 i = 0; i < count; i++) {
                    entries.pop_back();
                    entryIndex--;
                }
            };

            if (this->m_size != nullptr) {
                auto sizeNode = this->m_size->evaluate(evaluator);

                if (auto literal = dynamic_cast<ASTNodeLiteral *>(sizeNode.get())) {
                    auto entryCount = std::visit(overloaded {
                                                     [this](const std::string &) -> u128 { LogConsole::abortEvaluation("cannot use string to index array", this); },
                                                     [this](const std::shared_ptr<Pattern> &) -> u128 { LogConsole::abortEvaluation("cannot use custom type to index array", this); },
                                                     [](auto &&size) -> u128 { return size; } },
                        literal->getValue());

                    auto limit = evaluator->getArrayLimit();
                    if (entryCount > limit)
                        LogConsole::abortEvaluation(hex::format("array grew past set limit of {}", limit), this);

                    for (u64 i = 0; i < entryCount; i++) {
                        evaluator->setCurrentControlFlowStatement(ControlFlowStatement::None);

                        auto patterns       = this->m_type->createPatterns(evaluator);
                        size_t patternCount = patterns.size();

                        if (!patterns.empty())
                            addEntries(std::move(patterns));

                        auto ctrlFlow = evaluator->getCurrentControlFlowStatement();
                        evaluator->setCurrentControlFlowStatement(ControlFlowStatement::None);
                        if (ctrlFlow == ControlFlowStatement::Break)
                            break;
                        else if (ctrlFlow == ControlFlowStatement::Continue) {

                            discardEntries(patternCount);
                            continue;
                        }
                    }
                } else if (auto whileStatement = dynamic_cast<ASTNodeWhileStatement *>(sizeNode.get())) {
                    while (whileStatement->evaluateCondition(evaluator)) {
                        auto limit = evaluator->getArrayLimit();
                        if (entryIndex > limit)
                            LogConsole::abortEvaluation(hex::format("array grew past set limit of {}", limit), this);

                        evaluator->setCurrentControlFlowStatement(ControlFlowStatement::None);

                        auto patterns       = this->m_type->createPatterns(evaluator);
                        size_t patternCount = patterns.size();

                        if (!patterns.empty())
                            addEntries(std::move(patterns));

                        auto ctrlFlow = evaluator->getCurrentControlFlowStatement();
                        evaluator->setCurrentControlFlowStatement(ControlFlowStatement::None);
                        if (ctrlFlow == ControlFlowStatement::Break)
                            break;
                        else if (ctrlFlow == ControlFlowStatement::Continue) {
                            discardEntries(patternCount);
                            continue;
                        }
                    }
                }
            } else {
                while (true) {
                    bool reachedEnd = true;
                    auto limit      = evaluator->getArrayLimit();
                    if (entryIndex > limit)
                        LogConsole::abortEvaluation(hex::format("array grew past set limit of {}", limit), this);

                    evaluator->setCurrentControlFlowStatement(ControlFlowStatement::None);

                    auto patterns = this->m_type->createPatterns(evaluator);

                    for (auto &pattern : patterns) {
                        std::vector<u8> buffer(pattern->getSize());

                        if (evaluator->dataOffset() > evaluator->getProvider()->getActualSize() - buffer.size()) {
                            LogConsole::abortEvaluation("reached end of file before finding end of unsized array", this);
                        }

                        const auto patternSize = pattern->getSize();
                        addEntries(hex::moveToVector(std::move(pattern)));

                        auto ctrlFlow = evaluator->getCurrentControlFlowStatement();
                        if (ctrlFlow == ControlFlowStatement::None)
                            break;

                        evaluator->getProvider()->read(evaluator->dataOffset() - patternSize, buffer.data(), buffer.size());
                        reachedEnd = true;
                        for (u8 &byte : buffer) {
                            if (byte != 0x00) {
                                reachedEnd = false;
                                break;
                            }
                        }

                        if (reachedEnd) break;
                    }

                    auto ctrlFlow = evaluator->getCurrentControlFlowStatement();
                    evaluator->setCurrentControlFlowStatement(ControlFlowStatement::None);
                    if (ctrlFlow == ControlFlowStatement::Break)
                        break;
                    else if (ctrlFlow == ControlFlowStatement::Continue) {
                        discardEntries(1);
                        continue;
                    }

                    if (reachedEnd) break;
                }
            }

            arrayPattern->setEntries(std::move(entries));

            if (auto &arrayEntries = arrayPattern->getEntries(); !entries.empty())
                arrayPattern->setTypeName(arrayEntries.front()->getTypeName());

            arrayPattern->setSize(size);

            return std::move(arrayPattern);
        }
    };

}