#if defined(OS_MACOS)

    #include <CoreFoundation/CFBundle.h>
    #include <ApplicationServices/ApplicationServices.h>
    #include <Foundation/NSUserDefaults.h>
    #include <Foundation/Foundation.h>
    #include <AppKit/NSScreen.h>

    #include <string.h>
    #include <stdlib.h>
    #include <stdint.h>

    #include <hex/api/event.hpp>

    #include <string>
    #import <Foundation/Foundation.h>

    static std::string nsurl_to_string(NSURL* url) {
        NSString* urlString = [url absoluteString];
        const char* utf8String = [urlString UTF8String];

        return std::string(utf8String);
    }

    extern "C" void openWebpageMacos(const char *url) {
        CFURLRef urlRef = CFURLCreateWithBytes(NULL, (uint8_t*)(url), strlen(url), kCFStringEncodingASCII, NULL);
        LSOpenCFURLRef(urlRef, NULL);
        CFRelease(urlRef);
    }

    extern "C" bool isMacosSystemDarkModeEnabled(void) {
        NSString * appleInterfaceStyle = [[NSUserDefaults standardUserDefaults] stringForKey:@"AppleInterfaceStyle"];

        if (appleInterfaceStyle && [appleInterfaceStyle length] > 0) {
            return [[appleInterfaceStyle lowercaseString] containsString:@"dark"];
        } else {
            return false;
        }
    }

    extern "C" float getBackingScaleFactor(void) {
        return [[NSScreen mainScreen] backingScaleFactor];
    }

    @interface HexDocument : NSDocument

    @end

    @implementation HexDocument

    - (BOOL) readFromURL:(NSURL *)url ofType:(NSString *)typeName error:(NSError **)outError {
        hex::EventManager::post<hex::RequestOpenFile>(nsurl_to_string(url));

        return YES;
    }

    @end

#endif
