#if defined(OS_LINUX)

#include<stdexcept>

#include <hex/helpers/intrinsics.hpp>
#include "messaging.hpp"

namespace hex::messaging {

    void sendToOtherInstance(const std::string &evtName, const std::vector<std::string> &args){
        hex::unused(evtName);
        hex::unused(args);
        std::logic_error("Not implemented");
    }
    
    // Not implemented, so lets say we are the main instance every time so events are forwarded to ourselves
    bool setupNative(){
        return true;
    }
}

#endif
