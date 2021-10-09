#if defined(OS_MACOS)
    #include <hex/helpers/paths_mac.h>

    #include <Foundation/Foundation.h>

    namespace hex {
        std::string getMacExecutableDirectoryPath() {
            @autoreleasepool {
                return {[[[[[NSBundle mainBundle] executableURL] URLByDeletingLastPathComponent] path] UTF8String]};
            }
        }

        std::string getMacApplicationSupportDirectoryPath() {
            @autoreleasepool {
                NSError* error = nil;
                NSURL* dirUrl = [[NSFileManager defaultManager] URLForDirectory:NSApplicationSupportDirectory
                                                                       inDomain:NSUserDomainMask
                                                              appropriateForURL:nil
                                                                         create:YES
                                                                          error:&error];

                if (error != nil) {
                    __builtin_unreachable();
                }

                return {[[[dirUrl URLByAppendingPathComponent:(@"imhex")] path] UTF8String]};
            }
        }
    }
#endif
