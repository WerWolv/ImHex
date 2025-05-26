#include <mutex>

namespace hex {

    void functionEntry(void *);
    void functionExit(void *);

}

extern "C" {

    static std::mutex s_mutex;

    [[gnu::no_instrument_function]]
    void __cyg_profile_func_enter(void *functionAddress, void *) {
        std::scoped_lock lock(s_mutex);

        hex::functionEntry(functionAddress);
    }

    [[gnu::no_instrument_function]]
    void __cyg_profile_func_exit(void *functionAddress, void *) {
        std::scoped_lock lock(s_mutex);

        hex::functionExit(functionAddress);
    }

}