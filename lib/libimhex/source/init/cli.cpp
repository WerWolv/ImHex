#include <hex.hpp>

#include <hex/api/imhex_api/system.hpp>

namespace hex::init::cli_support {

    void applyGlobalFlag(const std::string &arg) {
        if (arg == "--readonly" || arg == "-r") {
            ImHexApi::System::impl::setReadOnlyMode(true);
        }
    }

}


