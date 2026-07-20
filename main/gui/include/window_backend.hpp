#pragma once

#include <hex/api/imhex_api/system.hpp>

#include <memory>

namespace hex {

    bool initializeWindowing();
    void shutdownWindowing();
    std::unique_ptr<ImHexApi::System::WindowBackend> createWindowBackend();

}
