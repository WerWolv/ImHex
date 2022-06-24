#if defined(OS_MACOS)

    #include <CoreFoundation/CFBundle.h>
    #include <ApplicationServices/ApplicationServices.h>

    #include <string.h>
    #include <stdlib.h>
    #include <stdint.h>

    void openWebpageMacos(const char *url) {
        CFURLRef urlRef = CFURLCreateWithBytes(NULL, (uint8_t*)(url), strlen(url), kCFStringEncodingASCII, nullptr);
        LSOpenCFURLRef(urlRef, nullptr);
        CFRelease(urlRef);
    }

#endif