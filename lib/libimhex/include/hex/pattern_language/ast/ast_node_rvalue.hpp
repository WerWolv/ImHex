#pragma once

#include <hex/pattern_language/ast/ast_node.hpp>
#include <hex/pattern_language/ast/ast_node_literal.hpp>
#include <hex/pattern_language/ast/ast_node_parameter_pack.hpp>

namespace hex::pl {

    class ASTNodeRValue : public ASTNode {
    public:
        using PathSegment = std::variant<std::string, std::unique_ptr<ASTNode>>;
        using Path        = std::vector<PathSegment>;

        explicit ASTNodeRValue(Path &&path) : ASTNode(), m_path(std::move(path)) { }

        ASTNodeRValue(const ASTNodeRValue &other) : ASTNode(other) {
            for (auto &part : other.m_path) {
                if (auto stringPart = std::get_if<std::string>(&part); stringPart != nullptr)
                    this->m_path.push_back(*stringPart);
                else if (auto nodePart = std::get_if<std::unique_ptr<ASTNode>>(&part); nodePart != nullptr)
                    this->m_path.push_back((*nodePart)->clone());
            }
        }

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeRValue(*this));
        }

        [[nodiscard]] const Path &getPath() const {
            return this->m_path;
        }

        [[nodiscard]] std::unique_ptr<ASTNode> evaluate(Evaluator *evaluator) const override {
            if (this->getPath().size() == 1) {
                if (auto name = std::get_if<std::string>(&this->getPath().front()); name != nullptr) {
                    if (*name == "$") return std::unique_ptr<ASTNode>(new ASTNodeLiteral(u128(evaluator->dataOffset())));

                    auto &parameterPack = evaluator->getScope(0).parameterPack;
                    if (parameterPack && *name == parameterPack->name)
                        return std::unique_ptr<ASTNode>(new ASTNodeParameterPack(parameterPack->values));
                }
            }

            auto patterns = this->createPatterns(evaluator);
            auto &pattern = patterns.front();

            Token::Literal literal;
            if (dynamic_cast<PatternDataUnsigned *>(pattern.get()) || dynamic_cast<PatternDataEnum *>(pattern.get())) {
                u128 value = 0;
                readVariable(evaluator, value, pattern.get());
                literal = value;
            } else if (dynamic_cast<PatternDataSigned *>(pattern.get())) {
                i128 value = 0;
                readVariable(evaluator, value, pattern.get());
                value   = hex::signExtend(pattern->getSize() * 8, value);
                literal = value;
            } else if (dynamic_cast<PatternDataFloat *>(pattern.get())) {
                if (pattern->getSize() == sizeof(u16)) {
                    u16 value = 0;
                    readVariable(evaluator, value, pattern.get());
                    literal = double(float16ToFloat32(value));
                } else if (pattern->getSize() == sizeof(float)) {
                    float value = 0;
                    readVariable(evaluator, value, pattern.get());
                    literal = double(value);
                } else if (pattern->getSize() == sizeof(double)) {
                    double value = 0;
                    readVariable(evaluator, value, pattern.get());
                    literal = value;
                } else LogConsole::abortEvaluation("invalid floating point type access", this);
            } else if (dynamic_cast<PatternDataCharacter *>(pattern.get())) {
                char value = 0;
                readVariable(evaluator, value, pattern.get());
                literal = value;
            } else if (dynamic_cast<PatternDataBoolean *>(pattern.get())) {
                bool value = false;
                readVariable(evaluator, value, pattern.get());
                literal = value;
            } else if (dynamic_cast<PatternDataString *>(pattern.get())) {
                std::string value;

                if (pattern->isLocal()) {
                    auto &variableValue = evaluator->getStack()[pattern->getOffset()];

                    std::visit(overloaded {
                                   [&](char assignmentValue) { if (assignmentValue != 0x00) value = std::string({ assignmentValue }); },
                                   [&](std::string &assignmentValue) { value = assignmentValue; },
                                   [&, this](PatternData *const &assignmentValue) {
                                       if (!dynamic_cast<PatternDataString *>(assignmentValue) && !dynamic_cast<PatternDataCharacter *>(assignmentValue))
                                           LogConsole::abortEvaluation(hex::format("cannot assign '{}' to string", pattern->getTypeName()), this);

                                       readVariable(evaluator, value, assignmentValue);
                                   },
                                   [&, this](auto &&assignmentValue) { LogConsole::abortEvaluation(hex::format("cannot assign '{}' to string", pattern->getTypeName()), this); } },
                        variableValue);
                } else {
                    value.resize(pattern->getSize());
                    evaluator->getProvider()->read(pattern->getOffset(), value.data(), value.size());
                    value.erase(std::find(value.begin(), value.end(), '\0'), value.end());
                }

                literal = value;
            } else if (auto bitfieldFieldPattern = dynamic_cast<PatternDataBitfieldField *>(pattern.get())) {
                u64 value = 0;
                readVariable(evaluator, value, pattern.get());
                literal = u128(hex::extract(bitfieldFieldPattern->getBitOffset() + (bitfieldFieldPattern->getBitSize() - 1), bitfieldFieldPattern->getBitOffset(), value));
            } else {
                literal = pattern->clone();
            }

            if (auto transformFunc = pattern->getTransformFunction(); transformFunc.has_value()) {
                auto result = transformFunc->func(evaluator, { std::move(literal) });

                if (!result.has_value())
                    LogConsole::abortEvaluation("transform function did not return a value", this);
                literal = std::move(result.value());
            }

            return std::unique_ptr<ASTNode>(new ASTNodeLiteral(std::move(literal)));
        }

        [[nodiscard]] std::vector<std::unique_ptr<PatternData>> createPatterns(Evaluator *evaluator) const override {
            std::vector<std::shared_ptr<PatternData>> searchScope;
            std::unique_ptr<PatternData> currPattern;
            i32 scopeIndex = 0;


            if (!evaluator->isGlobalScope()) {
                auto globalScope = evaluator->getGlobalScope().scope;
                std::copy(globalScope->begin(), globalScope->end(), std::back_inserter(searchScope));
            }

            {
                auto currScope = evaluator->getScope(scopeIndex).scope;
                std::copy(currScope->begin(), currScope->end(), std::back_inserter(searchScope));
            }

            for (const auto &part : this->getPath()) {

                if (part.index() == 0) {
                    // Variable access
                    auto name = std::get<std::string>(part);

                    if (name == "parent") {
                        scopeIndex--;

                        if (-scopeIndex >= evaluator->getScopeCount())
                            LogConsole::abortEvaluation("cannot access parent of global scope", this);

                        searchScope     = *evaluator->getScope(scopeIndex).scope;
                        auto currParent = evaluator->getScope(scopeIndex).parent;

                        if (currParent == nullptr) {
                            currPattern = nullptr;
                        } else {
                            currPattern = currParent->clone();
                        }

                        continue;
                    } else if (name == "this") {
                        searchScope = *evaluator->getScope(scopeIndex).scope;

                        auto currParent = evaluator->getScope(0).parent;

                        if (currParent == nullptr)
                            LogConsole::abortEvaluation("invalid use of 'this' outside of struct-like type", this);

                        currPattern = currParent->clone();
                        continue;
                    } else {
                        bool found = false;
                        for (auto iter = searchScope.crbegin(); iter != searchScope.crend(); ++iter) {
                            if ((*iter)->getVariableName() == name) {
                                currPattern = (*iter)->clone();
                                found       = true;
                                break;
                            }
                        }

                        if (name == "$")
                            LogConsole::abortEvaluation("invalid use of placeholder operator in rvalue");

                        if (!found) {
                            LogConsole::abortEvaluation(hex::format("no variable named '{}' found", name), this);
                        }
                    }
                } else {
                    // Array indexing
                    auto node  = std::get<std::unique_ptr<ASTNode>>(part)->evaluate(evaluator);
                    auto index = dynamic_cast<ASTNodeLiteral *>(node.get());

                    std::visit(overloaded {
                                   [this](const std::string &) { LogConsole::abortEvaluation("cannot use string to index array", this); },
                                   [this](const std::shared_ptr<PatternData> &) { LogConsole::abortEvaluation("cannot use custom type to index array", this); },
                                   [&, this](auto &&index) {
                                       if (auto dynamicArrayPattern = dynamic_cast<PatternDataDynamicArray *>(currPattern.get())) {
                                           if (index >= searchScope.size() || index < 0)
                                               LogConsole::abortEvaluation("array index out of bounds", this);

                                           currPattern = searchScope[index]->clone();
                                       } else if (auto staticArrayPattern = dynamic_cast<PatternDataStaticArray *>(currPattern.get())) {
                                           if (index >= staticArrayPattern->getEntryCount() || index < 0)
                                               LogConsole::abortEvaluation("array index out of bounds", this);

                                           auto newPattern = searchScope.front()->clone();
                                           newPattern->setOffset(staticArrayPattern->getOffset() + index * staticArrayPattern->getTemplate()->getSize());
                                           currPattern = std::move(newPattern);
                                       }
                                   } },
                        index->getValue());
                }

                if (currPattern == nullptr)
                    break;

                if (auto pointerPattern = dynamic_cast<PatternDataPointer *>(currPattern.get())) {
                    currPattern = pointerPattern->getPointedAtPattern()->clone();
                }

                std::shared_ptr<PatternData> indexPattern;
                if (currPattern->isLocal()) {
                    auto stackLiteral = evaluator->getStack()[currPattern->getOffset()];
                    if (auto stackPattern = std::get_if<std::shared_ptr<PatternData>>(&stackLiteral); stackPattern != nullptr)
                        indexPattern = *stackPattern;
                    else
                        return hex::moveToVector<std::unique_ptr<PatternData>>(std::move(currPattern));
                } else
                    indexPattern = currPattern->clone();

                if (auto structPattern = dynamic_cast<PatternDataStruct *>(indexPattern.get()))
                    searchScope = structPattern->getMembers();
                else if (auto unionPattern = dynamic_cast<PatternDataUnion *>(indexPattern.get()))
                    searchScope = unionPattern->getMembers();
                else if (auto bitfieldPattern = dynamic_cast<PatternDataBitfield *>(indexPattern.get()))
                    searchScope = bitfieldPattern->getFields();
                else if (auto dynamicArrayPattern = dynamic_cast<PatternDataDynamicArray *>(indexPattern.get()))
                    searchScope = dynamicArrayPattern->getEntries();
                else if (auto staticArrayPattern = dynamic_cast<PatternDataStaticArray *>(indexPattern.get()))
                    searchScope = { staticArrayPattern->getTemplate() };
            }

            if (currPattern == nullptr)
                LogConsole::abortEvaluation("cannot reference global scope", this);

            return hex::moveToVector<std::unique_ptr<PatternData>>(std::move(currPattern));
        }

    private:
        Path m_path;

        void readVariable(Evaluator *evaluator, auto &value, PatternData *variablePattern) const {
            constexpr bool isString = std::same_as<std::remove_cvref_t<decltype(value)>, std::string>;

            if (variablePattern->isLocal()) {
                auto &literal = evaluator->getStack()[variablePattern->getOffset()];

                std::visit(overloaded {
                               [&](std::string &assignmentValue) {
                                   if constexpr (isString) value = assignmentValue;
                               },
                               [&](std::shared_ptr<PatternData> &assignmentValue) { readVariable(evaluator, value, assignmentValue.get()); },
                               [&](auto &&assignmentValue) { value = assignmentValue; } },
                    literal);
            } else {
                if constexpr (isString) {
                    value.resize(variablePattern->getSize());
                    evaluator->getProvider()->read(variablePattern->getOffset(), value.data(), value.size());
                    value.erase(std::find(value.begin(), value.end(), '\0'), value.end());
                } else {
                    evaluator->getProvider()->read(variablePattern->getOffset(), &value, variablePattern->getSize());
                }
            }

            if constexpr (!isString)
                value = hex::changeEndianess(value, variablePattern->getSize(), variablePattern->getEndian());
        }
    };

}