#pragma once

#define RESOURCE_EXPORT(name)           \
    extern "C" unsigned char name[];    \
    extern "C" int name##_size;



RESOURCE_EXPORT(splash);
RESOURCE_EXPORT(banner_light);
RESOURCE_EXPORT(banner_dark);
RESOURCE_EXPORT(imhex_logo);

RESOURCE_EXPORT(cacert);



#undef RESOURCE_EXPORT