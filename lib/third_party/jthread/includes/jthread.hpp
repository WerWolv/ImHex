#pragma once

#if __cpp_lib_jthread >= 201911L
    #include <thread>
#else
    #include "../jthread/source/jthread.hpp"
#endif