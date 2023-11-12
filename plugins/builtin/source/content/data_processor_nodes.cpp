#include <content/data_processor_nodes.hpp>

namespace hex::plugin::builtin {

    void registerDataProcessorNodes() {
        registerBasicDPNs();
        registerVisualDPNs();
        registerLogicDPNs();
        registerControlDPNs();
        registerDecodeDPNs();
        registerMathDPNs();
        registerOtherDPNs();
    }
}