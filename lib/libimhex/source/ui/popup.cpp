#include <hex/ui/popup.hpp>
#include <hex/helpers/auto_reset.hpp>

namespace hex::impl {


    [[nodiscard]] std::vector<std::unique_ptr<PopupBase>> &PopupBase::getOpenPopups() {
        static AutoReset<std::vector<std::unique_ptr<PopupBase>>> openPopups;

        return openPopups;
    }

    std::mutex& PopupBase::getMutex() {
        static std::mutex mutex;

        return mutex;
    }



}