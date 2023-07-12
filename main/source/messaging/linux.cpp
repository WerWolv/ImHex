#if defined(OS_LINUX)

#include<stdexcept>

#include <hex/helpers/intrinsics.hpp>
#include <hex/helpers/logger.hpp>

#include "messaging.hpp"

namespace hex::messaging {

    void sendToOtherInstance(const std::string &evtName, const std::vector<u8> &args) {
        hex::unused(evtName);
        hex::unused(args);
        log::error("Not implemented function sendToOtherInstance() called");
    }
    
    // Not implemented, so lets say we are the main instance every time so events are forwarded to ourselves
    bool setupNative() {
        return true;
    }
}

#endif
