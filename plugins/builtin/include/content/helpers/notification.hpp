#pragma once

#include <content/popups/popup_notification.hpp>

namespace hex::plugin::builtin {

    void showError(const std::string& message){
        PopupError::open(message);
        log::error(message);
    }

    void showWarning(const std::string& message){
        PopupWarning::open(message);
        log::warn(message);
    }
}
