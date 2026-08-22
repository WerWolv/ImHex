#include <hex/api/content_registry/data_processor.hpp>
#include <hex/data_processor/node.hpp>

#include <numeric>
#include <algorithm>
#include <cmath>

namespace hex::plugin::builtin {

    class NodeArithmeticAdd : public dp::Node {
    public:
        NodeArithmeticAdd() : Node(
            "hex.builtin.nodes.arithmetic.add.header"_unlocalized,
            {
                dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.input.a"_unlocalized),
                dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.input.b"_unlocalized),
                dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.output"_unlocalized)
            }) { }

        void process() override {
            const auto &inputA = this->getIntegerOnInput(0);
            const auto &inputB = this->getIntegerOnInput(1);

            auto output = inputA + inputB;

            this->setIntegerOnOutput(2, output);
        }
    };

    class NodeArithmeticSubtract : public dp::Node {
    public:
        NodeArithmeticSubtract() : Node(
            "hex.builtin.nodes.arithmetic.sub.header"_unlocalized,
            {
                dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.input.a"_unlocalized),
                dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.input.b"_unlocalized),
                dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.output"_unlocalized)
            }) { }

        void process() override {
            const auto &inputA = this->getIntegerOnInput(0);
            const auto &inputB = this->getIntegerOnInput(1);

            auto output = inputA - inputB;

            this->setIntegerOnOutput(2, output);
        }
    };

    class NodeArithmeticMultiply : public dp::Node {
    public:
        NodeArithmeticMultiply() : Node(
            "hex.builtin.nodes.arithmetic.mul.header"_unlocalized,
            {
                dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.input.a"_unlocalized),
                dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.input.b"_unlocalized),
                dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.output"_unlocalized)
            }) { }

        void process() override {
            const auto &inputA = this->getIntegerOnInput(0);
            const auto &inputB = this->getIntegerOnInput(1);

            auto output = inputA * inputB;

            this->setIntegerOnOutput(2, output);
        }
    };

    class NodeArithmeticDivide : public dp::Node {
    public:
        NodeArithmeticDivide() : Node(
            "hex.builtin.nodes.arithmetic.div.header"_unlocalized,
            {
                dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.input.a"_unlocalized),
                dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.input.b"_unlocalized),
                dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.output"_unlocalized)
            }) { }

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
        NodeArithmeticModulus() : Node(
            "hex.builtin.nodes.arithmetic.mod.header"_unlocalized,
            {
                dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.input.a"_unlocalized),
                dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.input.b"_unlocalized),
                dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.output"_unlocalized)
            }) { }

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
        NodeArithmeticAverage() : Node(
            "hex.builtin.nodes.arithmetic.average.header"_unlocalized, {
                dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.input"_unlocalized),
                dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Float, "hex.builtin.nodes.common.output"_unlocalized)
            }) { }

        void process() override {
            const auto &input = this->getBufferOnInput(0);

            double output = std::reduce(input.begin(), input.end(), double(0)) / double(input.size());

            this->setFloatOnOutput(1, output);
        }
    };

    class NodeArithmeticMedian : public dp::Node {
    public:
        NodeArithmeticMedian() : Node(
            "hex.builtin.nodes.arithmetic.median.header"_unlocalized,
            {
                dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.input"_unlocalized),
                dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Float, "hex.builtin.nodes.common.output"_unlocalized)
            }) { }

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

            this->setFloatOnOutput(1, double(median));
        }
    };

    class NodeArithmeticCeil : public dp::Node {
    public:
        NodeArithmeticCeil() : Node(
            "hex.builtin.nodes.arithmetic.ceil.header"_unlocalized,
            {
                dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Float, "hex.builtin.nodes.common.input"_unlocalized),
                dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Float, "hex.builtin.nodes.common.output"_unlocalized)
            }) { }

        void process() override {
            const auto &input = this->getFloatOnInput(0);

            this->setFloatOnOutput(1, std::ceil(input));
        }
    };

    class NodeArithmeticFloor : public dp::Node {
    public:
        NodeArithmeticFloor() : Node(
            "hex.builtin.nodes.arithmetic.floor.header"_unlocalized,
            {
                dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Float, "hex.builtin.nodes.common.input"_unlocalized),
                dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Float, "hex.builtin.nodes.common.output"_unlocalized)
            }) { }

        void process() override {
            const auto &input = this->getFloatOnInput(0);

            this->setFloatOnOutput(1, std::floor(input));
        }
    };

    class NodeArithmeticRound : public dp::Node {
    public:
        NodeArithmeticRound() : Node(
            "hex.builtin.nodes.arithmetic.round.header"_unlocalized,
            {
                dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Float, "hex.builtin.nodes.common.input"_unlocalized),
                dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Float, "hex.builtin.nodes.common.output"_unlocalized)
            }) { }

        void process() override {
            const auto &input = this->getFloatOnInput(0);

            this->setFloatOnOutput(1, std::round(input));
        }
    };
        
    void registerMathDataProcessorNodes() {
        ContentRegistry::DataProcessor::add<NodeArithmeticAdd>("hex.builtin.nodes.arithmetic"_unlocalized, "hex.builtin.nodes.arithmetic.add"_unlocalized);
        ContentRegistry::DataProcessor::add<NodeArithmeticSubtract>("hex.builtin.nodes.arithmetic"_unlocalized, "hex.builtin.nodes.arithmetic.sub"_unlocalized);
        ContentRegistry::DataProcessor::add<NodeArithmeticMultiply>("hex.builtin.nodes.arithmetic"_unlocalized, "hex.builtin.nodes.arithmetic.mul"_unlocalized);
        ContentRegistry::DataProcessor::add<NodeArithmeticDivide>("hex.builtin.nodes.arithmetic"_unlocalized, "hex.builtin.nodes.arithmetic.div"_unlocalized);
        ContentRegistry::DataProcessor::add<NodeArithmeticModulus>("hex.builtin.nodes.arithmetic"_unlocalized, "hex.builtin.nodes.arithmetic.mod"_unlocalized);
        ContentRegistry::DataProcessor::add<NodeArithmeticAverage>("hex.builtin.nodes.arithmetic"_unlocalized, "hex.builtin.nodes.arithmetic.average"_unlocalized);
        ContentRegistry::DataProcessor::add<NodeArithmeticMedian>("hex.builtin.nodes.arithmetic"_unlocalized, "hex.builtin.nodes.arithmetic.median"_unlocalized);
        ContentRegistry::DataProcessor::add<NodeArithmeticCeil>("hex.builtin.nodes.arithmetic"_unlocalized, "hex.builtin.nodes.arithmetic.ceil"_unlocalized);
        ContentRegistry::DataProcessor::add<NodeArithmeticFloor>("hex.builtin.nodes.arithmetic"_unlocalized, "hex.builtin.nodes.arithmetic.floor"_unlocalized);
        ContentRegistry::DataProcessor::add<NodeArithmeticRound>("hex.builtin.nodes.arithmetic"_unlocalized, "hex.builtin.nodes.arithmetic.round"_unlocalized);
    }

}