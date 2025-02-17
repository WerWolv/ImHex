#pragma once

#include <hex/helpers/types.hpp>

#if defined(HEX_MODULE_EXPORT)
    #define EXPORT_MODULE export
#else
    #define EXPORT_MODULE
#endif