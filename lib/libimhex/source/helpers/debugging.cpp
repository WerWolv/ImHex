#include <hex/helpers/debugging.hpp>

namespace hex::dbg {

    namespace impl {

        bool &getDebugWindowState() {
            static bool state = false;

            return state;
        }

    }

}