#include <hex/ui/popup.hpp>

namespace hex::impl {


    [[nodiscard]] std::vector<std::unique_ptr<PopupBase>> &PopupBase::getOpenPopups() {
        static std::vector<std::unique_ptr<PopupBase>> openPopups;

        return openPopups;
    }


}