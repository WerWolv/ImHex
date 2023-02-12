#if defined(OS_MACOS)

    #include <CoreFoundation/CFBundle.h>
    #include <ApplicationServices/ApplicationServices.h>
    #include <Foundation/NSUserDefaults.h>
    #include <Foundation/Foundation.h>
    #include <AppKit/NSScreen.h>

    #include <string.h>
    #include <stdlib.h>
    #include <stdint.h>

    void openWebpageMacos(const char *url) {
        CFURLRef urlRef = CFURLCreateWithBytes(NULL, (uint8_t*)(url), strlen(url), kCFStringEncodingASCII, NULL);
        LSOpenCFURLRef(urlRef, NULL);
        CFRelease(urlRef);
    }

    bool isMacosSystemDarkModeEnabled(void) {
        NSString * appleInterfaceStyle = [[NSUserDefaults standardUserDefaults] stringForKey:@"AppleInterfaceStyle"];

        if (appleInterfaceStyle && [appleInterfaceStyle length] > 0) {
            return [[appleInterfaceStyle lowercaseString] containsString:@"dark"];
        } else {
            return false;
        }
    }

    float getBackingScaleFactor(void) {
        return [[NSScreen mainScreen] backingScaleFactor];
    }

#endif
