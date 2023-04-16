#include <hex/ui/popup.hpp>

namespace hex::impl {

    std::vector<std::unique_ptr<PopupBase>> PopupBase::s_openPopups;

}