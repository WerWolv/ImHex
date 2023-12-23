#include <hex/helpers/logger.hpp>

#include <popups/popup_notification.hpp>

namespace hex::plugin::builtin {

    void showError(const std::string& message){
        ui::PopupError::open(message);
        log::error(message);
    }

    void showWarning(const std::string& message){
        ui::PopupWarning::open(message);
        log::warn(message);
    }
}
