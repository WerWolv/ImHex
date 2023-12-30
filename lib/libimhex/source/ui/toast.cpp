#include <hex/ui/toast.hpp>

namespace hex::impl {

    [[nodiscard]] std::list<std::unique_ptr<ToastBase>> &ToastBase::getQueuedToasts() {
        static std::list<std::unique_ptr<ToastBase>> queuedToasts;

        return queuedToasts;
    }

    std::mutex& ToastBase::getMutex() {
        static std::mutex mutex;

        return mutex;
    }

}