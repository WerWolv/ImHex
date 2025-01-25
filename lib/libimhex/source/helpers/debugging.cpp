#include <hex/helpers/debugging.hpp>

namespace hex::dbg {

    namespace impl {

        bool &getDebugWindowState() {
            static bool state = false;

            return state;
        }

    }

    static bool s_debugMode = false;
    bool debugModeEnabled() {
        return s_debugMode;
    }

    void setDebugModeEnabled(bool enabled) {
        s_debugMode = enabled;
    }

}