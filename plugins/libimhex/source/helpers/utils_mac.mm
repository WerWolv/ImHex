#if defined(OS_MACOS)
    #include <hex/helpers/utils_mac.h>

    #include <Foundation/Foundation.h>

    namespace hex {
        std::string getPathForMac(ImHexPath path) {
            @autoreleasepool {
                NSError * error = nil;
                NSURL * appSupportDir = [[NSFileManager defaultManager] URLForDirectory:NSApplicationSupportDirectory
                                                                            inDomain:NSUserDomainMask
                                                                    appropriateForURL:nil
                                                                                create:YES
                                                                                error:&error];
                if (error != nil) {
                    __builtin_unreachable();
                }

                NSURL * result = nil;
                switch (path) {
                    case ImHexPath::Patterns:
                        result = [appSupportDir URLByAppendingPathComponent:@"imhex/patterns"];
                        break;

                    case ImHexPath::PatternsInclude:
                        result = [appSupportDir URLByAppendingPathComponent:@"imhex/patterns"];
                        break;
                    case ImHexPath::Magic:
                        result = [appSupportDir URLByAppendingPathComponent:@"imhex/magic"];
                        break;
                    case ImHexPath::Python:
                        result = [appSupportDir URLByAppendingPathComponent:@"imhex"];
                        break;
                    case ImHexPath::Plugins:
                        result = [appSupportDir URLByAppendingPathComponent:@"imhex/plugins"];
                        break;
                    case ImHexPath::Yara:
                        result = [appSupportDir URLByAppendingPathComponent:@"imhex/yara"];
                        break;
                    case ImHexPath::Config:
                        result = [appSupportDir URLByAppendingPathComponent:@"imhex/config"];
                        break;
                    case ImHexPath::Resources:
                        result = [appSupportDir URLByAppendingPathComponent:@"imhex/resources"];
                        break;
                    case ImHexPath::Constants:
                        result = [appSupportDir URLByAppendingPathComponent:@"imhex/constants"];
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
