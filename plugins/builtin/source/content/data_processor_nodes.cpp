#include <hex/api/localization.hpp>

#include <hex/data_processor/node.hpp>

#include <hex/helpers/crypto.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/helpers/logger.hpp>
#include <hex/providers/provider.hpp>

#include <content/helpers/diagrams.hpp>
#include <content/data_processor_nodes.hpp>

#include <cctype>
#include <random>
#include <ranges>

#include <nlohmann/json.hpp>

#include <imgui.h>
#include <implot.h>
#include <hex/ui/imgui_imhex_extensions.h>

#include <wolv/utils/core.hpp>
#include <wolv/utils/lock.hpp>
#include <wolv/utils/string.hpp>

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