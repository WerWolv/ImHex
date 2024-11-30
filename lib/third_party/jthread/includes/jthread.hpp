#pragma once

#if __cpp_lib_jthread >= 201911L
    #include <thread>
#else
    #define __stop_callback_base __stop_callback_base_j
    #define __stop_state __stop_state_j
    #include "../jthread/source/jthread.hpp"
    #undef __stop_callback_base
    #undef __stop_state
#endif