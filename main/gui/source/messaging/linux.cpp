#if defined(OS_LINUX)

#include <stdexcept>
#include <fcntl.h>
#include <sys/file.h>

#include <hex/helpers/logger.hpp>
#include <hex/api/events/events_lifecycle.hpp>

#include "messaging.hpp"

namespace hex::messaging {

    constexpr static auto CommunicationPipePath = "/tmp/imhex.fifo";
    constexpr static auto LockPath = "/tmp/imhex.lock";

    void sendToOtherInstance(const std::string &eventName, const std::vector<u8> &args) {
        log::debug("Sending event {} to another instance (not us)", eventName);

        // Create the message
        std::vector<u8> fullEventData(eventName.begin(), eventName.end());
        fullEventData.push_back('\0');

        fullEventData.insert(fullEventData.end(), args.begin(), args.end());

        u8 *data = &fullEventData[0];
        auto dataSize = fullEventData.size();

        int fifo = open(CommunicationPipePath, O_WRONLY);
        if (fifo < 0) return;

        ::write(fifo, data, dataSize);
        close(fifo);
    }

    void setupEventListener() {
        unlink(CommunicationPipePath);
        if (mkfifo(CommunicationPipePath, 0600) < 0) return;

        static int fifo = 0;
        fifo = open(CommunicationPipePath, O_RDWR);

        static auto listenerThread = std::jthread([](const std::stop_token &stopToken){
            std::vector<u8> buffer(0xFFFF);
            while (!stopToken.stop_requested()) {
                int result = ::read(fifo, buffer.data(), buffer.size());
                if (result > 0) {
                    EventNativeMessageReceived::post(std::vector<u8>{ buffer.begin(), buffer.begin() + result });
                }

                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        });

        std::atexit([]{
            listenerThread.request_stop();
            close(fifo);
            listenerThread.join();
        });
    }
    
    // Not implemented, so lets say we are the main instance every time so events are forwarded to ourselves
    bool setupNative() {
        int fd = open(LockPath, O_RDONLY);
        if (fd < 0) {
            fd = open(LockPath, O_CREAT, 0600);
            if (fd < 0)
                return false;
        }

        bool mainInstance = flock(fd, LOCK_EX | LOCK_NB) == 0;

        if (mainInstance)
            setupEventListener();

        return mainInstance;
    }
}

#endif
