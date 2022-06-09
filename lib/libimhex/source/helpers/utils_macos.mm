#if defined(OS_MACOS)
    #include <CoreFoundation/CFBundle.h>
    #include <ApplicationServices/ApplicationServices.h>

    namespace hex {

        void openWebpageMacos(const std::string &url) {
            CFURLRef urlRef = CFURLCreateWithBytes(nullptr, reinterpret_cast<u8 *>(url.data()), url.length(), kCFStringEncodingASCII, nullptr);
            LSOpenCFURLRef(urlRef, nullptr);
            CFRelease(urlRef);
        }

    }
#endif
