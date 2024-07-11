#include <hex/api/content_registry.hpp>
#include <hex/data_processor/node.hpp>

namespace hex::plugin::builtin {

    class NodeIf : public dp::Node {
    public:
        NodeIf() : Node("hex.builtin.nodes.control_flow.if.header",
                       { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.control_flow.if.condition"),
                           dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.control_flow.if.true"),
                           dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.control_flow.if.false"),
                           dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.output") }) { }

        void process() override {
            const auto &cond      = this->getIntegerOnInput(0);
            const auto &trueData  = this->getBufferOnInput(1);
            const auto &falseData = this->getBufferOnInput(2);

            if (cond != 0)
                this->setBufferOnOutput(3, trueData);
            else
                this->setBufferOnOutput(3, falseData);
        }
    };

    class NodeEquals : public dp::Node {
    public:
        NodeEquals() : Node("hex.builtin.nodes.control_flow.equals.header",
                           { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.input.a"),
                               dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.input.b"),
                               dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.output") }) { }

        void process() override {
            const auto &inputA = this->getIntegerOnInput(0);
            const auto &inputB = this->getIntegerOnInput(1);

            this->setIntegerOnOutput(2, inputA == inputB);
        }
    };

    class NodeNot : public dp::Node {
    public:
        NodeNot() : Node("hex.builtin.nodes.control_flow.not.header",
                        { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.input"),
                            dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.output") }) { }

        void process() override {
            const auto &input = this->getIntegerOnInput(0);

            this->setIntegerOnOutput(1, !input);
        }
    };

    class NodeGreaterThan : public dp::Node {
    public:
        NodeGreaterThan() : Node("hex.builtin.nodes.control_flow.gt.header",
                                { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.input.a"),
                                    dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.input.b"),
                                    dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.output") }) { }

        void process() override {
            const auto &inputA = this->getIntegerOnInput(0);
            const auto &inputB = this->getIntegerOnInput(1);

            this->setIntegerOnOutput(2, inputA > inputB);
        }
    };

    class NodeLessThan : public dp::Node {
    public:
        NodeLessThan() : Node("hex.builtin.nodes.control_flow.lt.header",
                             { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.input.a"),
                                 dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.input.b"),
                                 dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.output") }) { }

        void process() override {
            const auto &inputA = this->getIntegerOnInput(0);
            const auto &inputB = this->getIntegerOnInput(1);

            this->setIntegerOnOutput(2, inputA < inputB);
        }
    };

    class NodeBoolAND : public dp::Node {
    public:
        NodeBoolAND() : Node("hex.builtin.nodes.control_flow.and.header",
                            { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.input.a"),
                                dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.input.b"),
                                dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.output") }) { }

        void process() override {
            const auto &inputA = this->getIntegerOnInput(0);
            const auto &inputB = this->getIntegerOnInput(1);

            this->setIntegerOnOutput(2, inputA && inputB);
        }
    };

    class NodeBoolOR : public dp::Node {
    public:
        NodeBoolOR() : Node("hex.builtin.nodes.control_flow.or.header",
                           { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.input.a"),
                               dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.input.b"),
                               dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.output") }) { }

        void process() override {
            const auto &inputA = this->getIntegerOnInput(0);
            const auto &inputB = this->getIntegerOnInput(1);

            this->setIntegerOnOutput(2, inputA || inputB);
        }
    };

    class NodeLoop : public dp::Node {
    public:
        NodeLoop() : Node("hex.builtin.nodes.control_flow.loop.header",
                                             { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.control_flow.loop.start"),
                                               dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.control_flow.loop.end"),
                                               dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.control_flow.loop.init"),
                                               dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.control_flow.loop.in"),
                                               dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "hex.builtin.nodes.control_flow.loop.out") }) {}

        void process() override {
            if (!m_started) {
                m_started = true;
                auto start = this->getIntegerOnInput(0);
                auto end   = this->getIntegerOnInput(1);

                m_value = this->getIntegerOnInput(2);
                for (auto value = start; value < end; value += 1) {
                    this->resetProcessedInputs();
                    m_value = this->getIntegerOnInput(3);
                }

                m_started = false;
            }

            this->setIntegerOnOutput(4, m_value);
        }

        void reset() override {
            m_started = false;
        }

    private:
        bool m_started = false;
        i128 m_value = 0;
    };

    void registerControlDataProcessorNodes() {
        ContentRegistry::DataProcessorNode::add<NodeIf>("hex.builtin.nodes.control_flow", "hex.builtin.nodes.control_flow.if");
        ContentRegistry::DataProcessorNode::add<NodeEquals>("hex.builtin.nodes.control_flow", "hex.builtin.nodes.control_flow.equals");
        ContentRegistry::DataProcessorNode::add<NodeNot>("hex.builtin.nodes.control_flow", "hex.builtin.nodes.control_flow.not");
        ContentRegistry::DataProcessorNode::add<NodeGreaterThan>("hex.builtin.nodes.control_flow", "hex.builtin.nodes.control_flow.gt");
        ContentRegistry::DataProcessorNode::add<NodeLessThan>("hex.builtin.nodes.control_flow", "hex.builtin.nodes.control_flow.lt");
        ContentRegistry::DataProcessorNode::add<NodeBoolAND>("hex.builtin.nodes.control_flow", "hex.builtin.nodes.control_flow.and");
        ContentRegistry::DataProcessorNode::add<NodeBoolOR>("hex.builtin.nodes.control_flow", "hex.builtin.nodes.control_flow.or");
        ContentRegistry::DataProcessorNode::add<NodeLoop>("hex.builtin.nodes.control_flow", "hex.builtin.nodes.control_flow.loop");
    }

}
