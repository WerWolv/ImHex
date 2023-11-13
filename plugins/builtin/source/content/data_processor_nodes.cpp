#include <content/data_processor_nodes.hpp>

namespace hex::plugin::builtin {

    void registerDataProcessorNodes() {
        registerBasicDataProcessorNodes();
        registerVisualDataProcessorNodes();
        registerLogicDataProcessorNodes();
        registerControlDataProcessorNodes();
        registerDecodeDataProcessorNodes();
        registerMathDataProcessorNodes();
        registerOtherDataProcessorNodes();
    }
    
}