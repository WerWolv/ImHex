#pragma once

#include <hex.hpp>

#include <functional>
#include <string>
#include <map>
#include <vector>

EXPORT_MODULE namespace hex {

    /**
     * Cross-instance messaging system
     * This allows you to send messages to the "main" instance of ImHex running, from any other instance
     */
    namespace ImHexApi::Messaging {

        namespace impl {

            using MessagingHandler = std::function<void(const std::vector<u8> &)>;

            const std::map<std::string, MessagingHandler>& getHandlers();
            void runHandler(const std::string &eventName, const std::vector<u8> &args);

        }

        /**
         * @brief Register the handler for this specific event name
         */
        void registerHandler(const std::string &eventName, const impl::MessagingHandler &handler);
    }

}