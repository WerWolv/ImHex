#pragma once

#include <content/popups/popup_notification.hpp>

namespace hex::plugin::builtin {

    void showError(const std::string& message);

    void showWarning(const std::string& message);
}
