#pragma once

#if __has_include(<jthread>)
    #include <jthread>
#else
    #include "../jthread/source/jthread.hpp"
#endif