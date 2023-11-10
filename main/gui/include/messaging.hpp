#pragma once

#include <string>
#include <vector>

#include <hex/helpers/types.hpp>

/**
 * Cross-instance (cross-process) messaging system
 * As of now, this system may not be stable for use beyond its current use:
 * forwarding providers opened in new instances
 */
namespace hex::messaging {

    /**
     * @brief Setup everything to be able to send/receive messages
     */
    void setupMessaging();

    /**
     * @brief Internal method - setup platform-specific things to be able to send messages
     * @return if this instance has been determined to be the main instance
     */
    bool setupNative();

    /**
     * @brief Internal method - send a message to another Imhex instance in a platform-specific way
     */
    void sendToOtherInstance(const std::string &eventName, const std::vector<u8> &args);
    
    /**
     * @brief Internal method - called by platform-specific code when a event has been received
     */
    void messageReceived(const std::string &eventName, const std::vector<u8> &args);

}
