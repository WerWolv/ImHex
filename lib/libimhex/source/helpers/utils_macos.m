#if defined(OS_MACOS)

    #include <CoreFoundation/CFBundle.h>
    #include <ApplicationServices/ApplicationServices.h>

    #include <string.h>

    void openWebpageMacos(const char *url) {
        CFURLRef urlRef = CFURLCreateWithBytes(nullptr, reinterpret_cast<uint8_t *>(url), strlen(url), kCFStringEncodingASCII, nullptr);
        LSOpenCFURLRef(urlRef, nullptr);
        CFRelease(urlRef);
    }

#endif