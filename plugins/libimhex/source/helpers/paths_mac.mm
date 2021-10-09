#if defined(OS_MACOS)
    #include <hex/helpers/paths_mac.h>

    #include <Foundation/Foundation.h>

    namespace hex {
        std::string getPathForMac(ImHexPath path) {
            @autoreleasepool {
                NSError * error = nil;
                NSURL * baseUrl;

                if (path == ImHexPath::Plugins) {
                    baseUrl = [[[NSBundle mainBundle] executableURL] URLByDeletingLastPathComponent];
                } else {
                    baseUrl = [[NSFileManager defaultManager] URLForDirectory:NSApplicationSupportDirectory
                                                                     inDomain:NSUserDomainMask
                                                            appropriateForURL:nil
                                                                       create:YES
                                                                        error:&error];
                }

                if (error != nil) {
                    __builtin_unreachable();
                }

                NSURL * result = nil;
                switch (path) {
                    case ImHexPath::Patterns:
                        result = [baseUrl URLByAppendingPathComponent:@"imhex/patterns"];
                        break;

                    case ImHexPath::PatternsInclude:
                        result = [baseUrl URLByAppendingPathComponent:@"imhex/patterns"];
                        break;
                    case ImHexPath::Magic:
                        result = [baseUrl URLByAppendingPathComponent:@"imhex/magic"];
                        break;
                    case ImHexPath::Python:
                        result = [baseUrl URLByAppendingPathComponent:@"imhex"];
                        break;
                    case ImHexPath::Plugins:
                        result = [baseUrl URLByAppendingPathComponent:@"plugins"];
                        break;
                    case ImHexPath::Yara:
                        result = [baseUrl URLByAppendingPathComponent:@"imhex/yara"];
                        break;
                    case ImHexPath::Config:
                        result = [baseUrl URLByAppendingPathComponent:@"imhex/config"];
                        break;
                    case ImHexPath::Resources:
                        result = [baseUrl URLByAppendingPathComponent:@"imhex/resources"];
                        break;
                    case ImHexPath::Constants:
                        result = [baseUrl URLByAppendingPathComponent:@"imhex/constants"];
                        break;
                }

                if (result == nil) {
                    __builtin_unreachable();
                }

                return std::string([[result path] UTF8String]);
            }
        }
    }
#endif
