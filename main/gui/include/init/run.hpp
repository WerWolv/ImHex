#pragma once

#include <init/splash_window.hpp>

namespace hex::init {

    void handleFileOpenRequest();

    void initializationFinished();
    std::unique_ptr<WindowSplash> initializeImHex();
    void deinitializeImHex();

}
