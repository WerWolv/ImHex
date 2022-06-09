#if defined(OS_MACOS)
    #include <hex/helpers/fs_macos.hpp>

    #include <Foundation/Foundation.h>

    extern "C" void getMacExecutableDirectoryPath(std::string &result) {
        @autoreleasepool {
            result = {[[[[[NSBundle mainBundle] executableURL] URLByDeletingLastPathComponent] path] UTF8String]};
        }
    }

    extern "C" void getMacApplicationSupportDirectoryPath(std::string &result) {
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

            result = {[[[dirUrl URLByAppendingPathComponent:(@"imhex")] path] UTF8String]};
        }
    }
#endif
