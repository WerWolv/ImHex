#if defined(OS_WINDOWS)

    #define RESOURCE(name, path) __asm__ (      \
    ".section .rodata\n"                        \
    ".global " #name "\n"                       \
    ".global " #name "_size\n"                  \
        #name ":\n"                             \
        ".incbin \"" path "\"\n"                \
        #name "_size:\n"                        \
        ".int " #name "_size - " #name "\n"     \
        ".align 8\n"                            \
        )

#elif defined(OS_MACOS)

    #define RESOURCE(name, path) __asm__ (      \
    ".text;\n"                                  \
    ".global " #name ";\n"                      \
    ".global " #name "_size;\n"                 \
        #name ":\n"                             \
        ".incbin \"" path "\";\n"               \
        #name "_size:\n"                        \
        ".int " #name "_size - " #name ";\n"    \
        ".align 8;\n"                           \
        )

#elif defined(OS_LINUX)

    #define RESOURCE(name, path) __asm__ (      \
    ".section .rodata\n"                        \
    ".global " #name "\n"                       \
    ".global " #name "_size\n"                  \
        #name ":\n"                             \
        ".incbin \"" path "\"\n"                \
        ".type " #name ", @object\n"            \
        ".size " #name "_size, 1\n"             \
        #name "_size:\n"                        \
        ".int " #name "_size - " #name "\n"     \
        ".align 8\n"                            \
        )

#endif



RESOURCE(banner_light, "../../../res/resources/banner_light.png");
RESOURCE(banner_dark, "../../../res/resources/banner_dark.png");
RESOURCE(splash, "../../../res/resources/splash.png");
RESOURCE(imhex_logo, "../../../res/resources/logo.png");

RESOURCE(cacert, "../../../res/resources/cacert.pem");