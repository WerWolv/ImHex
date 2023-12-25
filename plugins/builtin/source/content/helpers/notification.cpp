#include <hex/helpers/logger.hpp>

#include <toasts/toast_notification.hpp>

namespace hex::plugin::builtin {

    void showError(const std::string& message){
        ui::ToastError::open(message);
        log::error(message);
    }

    void showWarning(const std::string& message){
        ui::ToastWarning::open(message);
        log::warn(message);
    }
}
