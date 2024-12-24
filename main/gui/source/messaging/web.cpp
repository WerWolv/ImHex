#if defined(OS_WEB)

#include <stdexcept>

#include <hex/helpers/logger.hpp>

#include "messaging.hpp"

namespace hex::messaging {

    void sendToOtherInstance(const std::string &eventName, const std::vector<u8> &args) {
        std::ignore = eventName;
        std::ignore = args;
        log::error("Unimplemented function 'sendToOtherInstance()' called");
    }
    
    // Not implemented, so lets say we are the main instance every time so events are forwarded to ourselves
    bool setupNative() {
        return true;
    }
}

#endif
