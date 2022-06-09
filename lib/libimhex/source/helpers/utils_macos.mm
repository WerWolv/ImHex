#if defined(OS_MACOS)

    #include <hex.hpp>

    #include <hex/helpers/utils_macos.hpp>

    #include <CoreFoundation/CFBundle.h>
    #include <ApplicationServices/ApplicationServices.h>

    extern "C" void openWebpageMacos(std::string url) {
        CFURLRef urlRef = CFURLCreateWithBytes(nullptr, reinterpret_cast<u8 *>(url.data()), url.length(), kCFStringEncodingASCII, nullptr);
        LSOpenCFURLRef(urlRef, nullptr);
        CFRelease(urlRef);
    }

#endif