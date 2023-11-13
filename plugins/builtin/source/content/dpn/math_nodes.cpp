#include <hex/api/content_registry.hpp>
#include <hex/data_processor/node.hpp>

#include <numeric>
#include <algorithm>
#include <cmath>

namespace hex::plugin::builtin {

    class NodeArithmeticAdd : public dp::Node {
    public:
        NodeArithmeticAdd() : Node("hex.builtin.nodes.arithmetic.add.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.input.a"), dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.input.b"), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.output") }) { }

        void process() override {
            const auto &inputA = this->getIntegerOnInput(0);
            const auto &inputB = this->getIntegerOnInput(1);

            auto output = inputA + inputB;

            this->setIntegerOnOutput(2, output);
        }
    };

    class NodeArithmeticSubtract : public dp::Node {
    public:
        NodeArithmeticSubtract() : Node("hex.builtin.nodes.arithmetic.sub.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.input.a"), dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.input.b"), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.output") }) { }

        void process() override {
            const auto &inputA = this->getIntegerOnInput(0);
            const auto &inputB = this->getIntegerOnInput(1);

            auto output = inputA - inputB;

            this->setIntegerOnOutput(2, output);
        }
    };

    class NodeArithmeticMultiply : public dp::Node {
    public:
        NodeArithmeticMultiply() : Node("hex.builtin.nodes.arithmetic.mul.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.input.a"), dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.input.b"), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.output") }) { }

        void process() override {
            const auto &inputA = this->getIntegerOnInput(0);
            const auto &inputB = this->getIntegerOnInput(1);

            auto output = inputA * inputB;

            this->setIntegerOnOutput(2, output);
        }
    };

    class NodeArithmeticDivide : public dp::Node {
    public:
        NodeArithmeticDivide() : Node("hex.builtin.nodes.arithmetic.div.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.input.a"), dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.input.b"), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.output") }) { }

        void process() override {
            const auto &inputA = this->getIntegerOnInput(0);
            const auto &inputB = this->getIntegerOnInput(1);

            if (inputB == 0)
                throwNodeError("Division by zero");

            auto output = inputA / inputB;

            this->setIntegerOnOutput(2, output);
        }
    };

    class NodeArithmeticModulus : public dp::Node {
    public:
        NodeArithmeticModulus() : Node("hex.builtin.nodes.arithmetic.mod.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.input.a"), dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.input.b"), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.output") }) { }

        void process() override {
            const auto &inputA = this->getIntegerOnInput(0);
            const auto &inputB = this->getIntegerOnInput(1);

            if (inputB == 0)
                throwNodeError("Division by zero");

            auto output = inputA % inputB;

            this->setIntegerOnOutput(2, output);
        }
    };

    class NodeArithmeticAverage : public dp::Node {
    public:
        NodeArithmeticAverage() : Node("hex.builtin.nodes.arithmetic.average.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.input"), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Float, "hex.builtin.nodes.common.output") }) { }

        void process() override {
            const auto &input = this->getBufferOnInput(0);

            double output = std::reduce(input.begin(), input.end(), double(0)) / double(input.size());

            this->setFloatOnOutput(1, output);
        }
    };

    class NodeArithmeticMedian : public dp::Node {
    public:
        NodeArithmeticMedian() : Node("hex.builtin.nodes.arithmetic.median.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.input"), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Float, "hex.builtin.nodes.common.output") }) { }

        void process() override {
            auto input = this->getBufferOnInput(0);

            u64 medianIndex = input.size() / 2;
            std::nth_element(input.begin(), input.begin() + medianIndex, input.end());
            i128 median = 0;

            if (input.size() % 2 == 0) {
                std::nth_element(input.begin(), input.begin() + medianIndex - 1, input.end());
                median = (input[medianIndex] + input[medianIndex - 1]) / 2;
            } else {
                median = input[medianIndex];
            }

            this->setFloatOnOutput(1, median);
        }
    };

    class NodeArithmeticCeil : public dp::Node {
    public:
        NodeArithmeticCeil() : Node("hex.builtin.nodes.arithmetic.ceil.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Float, "hex.builtin.nodes.common.input"), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Float, "hex.builtin.nodes.common.output") }) { }

        void process() override {
            const auto &input = this->getFloatOnInput(0);

            this->setFloatOnOutput(1, std::ceil(input));
        }
    };

    class NodeArithmeticFloor : public dp::Node {
    public:
        NodeArithmeticFloor() : Node("hex.builtin.nodes.arithmetic.floor.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Float, "hex.builtin.nodes.common.input"), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Float, "hex.builtin.nodes.common.output") }) { }

        void process() override {
            const auto &input = this->getFloatOnInput(0);

            this->setFloatOnOutput(1, std::floor(input));
        }
    };

    class NodeArithmeticRound : public dp::Node {
    public:
        NodeArithmeticRound() : Node("hex.builtin.nodes.arithmetic.round.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Float, "hex.builtin.nodes.common.input"), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Float, "hex.builtin.nodes.common.output") }) { }

        void process() override {
            const auto &input = this->getFloatOnInput(0);

            this->setFloatOnOutput(1, std::round(input));
        }
    };
        
    void registerMathDataProcessorNodes() {
        ContentRegistry::DataProcessorNode::add<NodeArithmeticAdd>("hex.builtin.nodes.arithmetic", "hex.builtin.nodes.arithmetic.add");
        ContentRegistry::DataProcessorNode::add<NodeArithmeticSubtract>("hex.builtin.nodes.arithmetic", "hex.builtin.nodes.arithmetic.sub");
        ContentRegistry::DataProcessorNode::add<NodeArithmeticMultiply>("hex.builtin.nodes.arithmetic", "hex.builtin.nodes.arithmetic.mul");
        ContentRegistry::DataProcessorNode::add<NodeArithmeticDivide>("hex.builtin.nodes.arithmetic", "hex.builtin.nodes.arithmetic.div");
        ContentRegistry::DataProcessorNode::add<NodeArithmeticModulus>("hex.builtin.nodes.arithmetic", "hex.builtin.nodes.arithmetic.mod");
        ContentRegistry::DataProcessorNode::add<NodeArithmeticAverage>("hex.builtin.nodes.arithmetic", "hex.builtin.nodes.arithmetic.average");
        ContentRegistry::DataProcessorNode::add<NodeArithmeticMedian>("hex.builtin.nodes.arithmetic", "hex.builtin.nodes.arithmetic.median");
        ContentRegistry::DataProcessorNode::add<NodeArithmeticCeil>("hex.builtin.nodes.arithmetic", "hex.builtin.nodes.arithmetic.ceil");
        ContentRegistry::DataProcessorNode::add<NodeArithmeticFloor>("hex.builtin.nodes.arithmetic", "hex.builtin.nodes.arithmetic.floor");
        ContentRegistry::DataProcessorNode::add<NodeArithmeticRound>("hex.builtin.nodes.arithmetic", "hex.builtin.nodes.arithmetic.round");
    }

}