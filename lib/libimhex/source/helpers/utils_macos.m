#if defined(OS_MACOS)

    #include <CoreFoundation/CFBundle.h>
    #include <ApplicationServices/ApplicationServices.h>
    #include <Foundation/NSUserDefaults.h>
    #include <Foundation/Foundation.h>
    #include <AppKit/NSScreen.h>

    #include <string.h>
    #include <stdlib.h>
    #include <stdint.h>

    #import <Cocoa/Cocoa.h>

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

    @interface HexDocument : NSDocument

    @property (nonatomic, strong) NSData *fileData;

    @end

    @implementation HexDocument

    - (BOOL)readFromData:(NSData *)data ofType:(NSString *)typeName error:(NSError **)outError {
        // Set the file data to the given data
        self.fileData = data;
        return YES;
    }

    @end

#endif
