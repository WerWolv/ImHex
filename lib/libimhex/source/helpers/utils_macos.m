#if defined(OS_MACOS)

    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wdeprecated-declarations"

    #include <CoreFoundation/CFBundle.h>
    #include <ApplicationServices/ApplicationServices.h>
    #include <Foundation/NSUserDefaults.h>
    #include <AppKit/NSScreen.h>
    #include <CoreFoundation/CoreFoundation.h>
    #include <CoreText/CoreText.h>

    #include <string.h>
    #include <stdlib.h>
    #include <stdint.h>

    #define GLFW_EXPOSE_NATIVE_COCOA
    #include <GLFW/glfw3.h>
    #include <GLFW/glfw3native.h>

    #import <Cocoa/Cocoa.h>
    #import <Foundation/Foundation.h>
    #import <AppleScriptObjC/AppleScriptObjC.h>
    #import <UserNotifications/UserNotifications.h>

    #include <hex/helpers/keys.hpp>

    void errorMessageMacos(const char *cMessage) {
        CFStringRef strMessage = CFStringCreateWithCString(NULL, cMessage, kCFStringEncodingUTF8);
        CFUserNotificationDisplayAlert(0, kCFUserNotificationStopAlertLevel, NULL, NULL, NULL, strMessage, NULL, NULL, NULL, NULL, NULL);
    }

    void openFile(const char *path);
    void registerFont(const char *fontName, const char *fontPath);

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

    void macOSCloseButtonPressed(void);

    @interface CloseButtonHandler : NSObject
    @end

    @implementation CloseButtonHandler
    - (void)pressed:(id)sender {
        macOSCloseButtonPressed();
    }
    @end

    void setupMacosWindowStyle(GLFWwindow *window, bool borderlessWindowMode) {
        NSWindow* cocoaWindow = glfwGetCocoaWindow(window);

        cocoaWindow.titleVisibility = NSWindowTitleHidden;

        if (borderlessWindowMode) {
            cocoaWindow.titlebarAppearsTransparent = YES;
            cocoaWindow.styleMask |= NSWindowStyleMaskFullSizeContentView;

            // Setup liquid glass background effect
            {
                NSView* glfwContentView = [cocoaWindow contentView];

                NSOpenGLContext* context = [NSOpenGLContext currentContext];
                if (!context) {
                    glfwMakeContextCurrent(window);
                    context = [NSOpenGLContext currentContext];
                }

                GLint opaque = 0;
                [context setValues:&opaque forParameter:NSOpenGLCPSurfaceOpacity];
                [context update];

                NSView* containerView = [[NSView alloc] initWithFrame:[glfwContentView frame]];
                containerView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
                [containerView setWantsLayer:YES];

                Class glassEffectClass = NSClassFromString(@"NSGlassEffectView");
                NSView* effectView = nil;
                if (glassEffectClass) {
                    // Use the new liquid glass effect
                    effectView = [[glassEffectClass alloc] initWithFrame:[containerView bounds]];
                } else {
                    // Fall back to NSVisualEffectView for older systems
                    NSVisualEffectView* visualEffectView = [[NSVisualEffectView alloc] initWithFrame:[containerView bounds]];
                    visualEffectView.material = NSVisualEffectMaterialHUDWindow;
                    visualEffectView.blendingMode = NSVisualEffectBlendingModeBehindWindow;
                    visualEffectView.state = NSVisualEffectStateActive;
                    effectView = visualEffectView;
                }

                effectView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;

                [containerView addSubview:effectView];

                [glfwContentView removeFromSuperview];
                glfwContentView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
                [containerView addSubview:glfwContentView];

                [cocoaWindow setContentView:containerView];
            }

            [cocoaWindow setOpaque:NO];
            [cocoaWindow setHasShadow:YES];
            [cocoaWindow setBackgroundColor:[NSColor colorWithWhite: 0 alpha: 0.001f]];
        }

        NSButton *closeButton = [cocoaWindow standardWindowButton:NSWindowCloseButton];
        [closeButton setAction:@selector(pressed:)];
        [closeButton setTarget:[CloseButtonHandler alloc]];
    }

    bool isMacosFullScreenModeEnabled(GLFWwindow *window) {
        NSWindow* cocoaWindow = glfwGetCocoaWindow(window);
        return (cocoaWindow.styleMask & NSWindowStyleMaskFullScreen) == NSWindowStyleMaskFullScreen;
    }

    void enumerateFontsMacos(void) {
        CFArrayRef fontDescriptors = CTFontManagerCopyAvailableFontFamilyNames();
        CFIndex count = CFArrayGetCount(fontDescriptors);

        for (CFIndex i = 0; i < count; i++) {
            CFStringRef fontName = (CFStringRef)CFArrayGetValueAtIndex(fontDescriptors, i);

            // Get font path - skip fonts without valid URLs
            CFDictionaryRef attributes = (__bridge CFDictionaryRef)@{ (__bridge NSString *)kCTFontFamilyNameAttribute : (__bridge NSString *)fontName };
            CTFontDescriptorRef descriptor = CTFontDescriptorCreateWithAttributes(attributes);
            CFURLRef fontURL = CTFontDescriptorCopyAttribute(descriptor, kCTFontURLAttribute);

            if (fontURL != NULL) {
                CFStringRef fontPath = CFURLCopyFileSystemPath(fontURL, kCFURLPOSIXPathStyle);

                if (fontPath != NULL) {
                    registerFont([(__bridge NSString *)fontName UTF8String], [(__bridge NSString *)fontPath UTF8String]);
                    CFRelease(fontPath);
                }

                CFRelease(fontURL);
            }

            CFRelease(descriptor);
        }

        CFRelease(fontDescriptors);
    }

    void macosHandleTitlebarDoubleClickGesture(GLFWwindow *window) {
        NSWindow* cocoaWindow = glfwGetCocoaWindow(window);

        // Consult user preferences: "System Settings -> Desktop & Dock -> Double-click a window's title bar to"
        NSString* action = [[NSUserDefaults standardUserDefaults] stringForKey:@"AppleActionOnDoubleClick"];
        
        if (action == nil || [action isEqualToString:@"None"]) {
            // Nothing to do
        } else if ([action isEqualToString:@"Minimize"]) {
            if ([cocoaWindow isMiniaturizable]) {
                [cocoaWindow miniaturize:nil];
            }
        } else if ([action isEqualToString:@"Maximize"]) {
            // `[NSWindow zoom:_ sender]` takes over pumping the main runloop for the duration of the resize,
            // and would interfere with our renderer's frame logic. Schedule it for the next frame
            
            CFRunLoopPerformBlock(CFRunLoopGetMain(), kCFRunLoopCommonModes, ^{
                if ([cocoaWindow isZoomable]) {
                    [cocoaWindow zoom:nil];
                }
            });
        }
    }

    void macosSetWindowMovable(GLFWwindow *window, bool movable) {
        NSWindow* cocoaWindow = glfwGetCocoaWindow(window);

        [cocoaWindow setMovable:movable];
    }

    bool macosIsWindowBeingResizedByUser(GLFWwindow *window) {
        NSWindow* cocoaWindow = glfwGetCocoaWindow(window);
        
        return cocoaWindow.inLiveResize;
    }

    void macosMarkContentEdited(GLFWwindow *window, bool edited) {
        NSWindow* cocoaWindow = glfwGetCocoaWindow(window);

        [cocoaWindow setDocumentEdited:edited];
    }

    static NSArray* getRunningInstances(NSString *bundleIdentifier) {
        return [NSRunningApplication runningApplicationsWithBundleIdentifier: bundleIdentifier];
    }

    bool macosIsMainInstance(void) {
        NSArray *applications = getRunningInstances(@"net.WerWolv.ImHex");
        return applications.count == 0;
    }

    extern void macosEventDataReceived(const unsigned char *data, size_t length);
    static OSErr handleAppleEvent(const AppleEvent *event, AppleEvent *reply, void *refcon) {
        (void)reply;
        (void)refcon;

        // Extract the raw binary data from the event's parameter
        AEDesc paramDesc;
        OSErr err = AEGetParamDesc(event, keyDirectObject, typeWildCard, &paramDesc);
        if (err != noErr) {
            NSLog(@"Failed to get parameter: %d", err);
            return err;
        }

        // Convert the AEDesc to NSData
        NSAppleEventDescriptor *descriptor = [[NSAppleEventDescriptor alloc] initWithAEDescNoCopy:&paramDesc];
        NSData *binaryData = descriptor.data;

        // Process the binary data
        if (binaryData) {
            macosEventDataReceived(binaryData.bytes, binaryData.length);
        }

        return noErr;
    }

    void macosInstallEventListener(void) {
        AEInstallEventHandler('misc', 'imhx', NewAEEventHandlerUPP(handleAppleEvent), 0, false);
    }

    void macosSendMessageToMainInstance(const unsigned char *data, size_t size) {
        NSString *bundleIdentifier = @"net.WerWolv.ImHex";

        NSData *binaryData = [NSData dataWithBytes:data length:size];
        // Find the target application by its bundle identifier
        NSAppleEventDescriptor *targetApp = [NSAppleEventDescriptor descriptorWithBundleIdentifier:bundleIdentifier];
        if (!targetApp) {
            NSLog(@"Application with bundle identifier %@ not found.", bundleIdentifier);
            return;
        }

        // Create the Apple event
        NSAppleEventDescriptor *event = [[NSAppleEventDescriptor alloc] initWithEventClass:'misc'
                                                                                   eventID:'imhx'
                                                                          targetDescriptor:targetApp
                                                                                  returnID:kAutoGenerateReturnID
                                                                             transactionID:kAnyTransactionID];

        // Add a parameter with raw binary data
        NSAppleEventDescriptor *binaryDescriptor = [NSAppleEventDescriptor descriptorWithDescriptorType:typeData
                                                                                                   data:binaryData];
        [event setParamDescriptor:binaryDescriptor forKeyword:keyDirectObject];

        // Send the event
        OSStatus status = AESendMessage([event aeDesc], NULL, kAENoReply, kAEDefaultTimeout);
        if (status != noErr) {
            NSLog(@"Failed to send Apple event: %d", status);
        }
    }

    @interface HexDocument : NSDocument

    @end

    @implementation HexDocument

    - (BOOL) readFromURL:(NSURL *)url ofType:(NSString *)typeName error:(NSError **)outError {
        NSString* urlString = [url absoluteString];
        const char* utf8String = [urlString UTF8String];

        const char *prefix = "file://";
        if (strncmp(utf8String, prefix, strlen(prefix)) == 0)
            utf8String += strlen(prefix);

        openFile(utf8String);

        return YES;
    }

    @end

    void macosGetKey(enum Keys key, int *output) {
        *output = 0x00;
        switch (key) {
            case Space:             *output = ' ';  break;
            case Apostrophe:        *output = '\''; break;
            case Comma:             *output = ',';  break;
            case Minus:             *output = '-';  break;
            case Period:            *output = '.';  break;
            case Slash:             *output = '/';  break;
            case Num0:              *output = '0';  break;
            case Num1:              *output = '1';  break;
            case Num2:              *output = '2';  break;
            case Num3:              *output = '3';  break;
            case Num4:              *output = '4';  break;
            case Num5:              *output = '5';  break;
            case Num6:              *output = '6';  break;
            case Num7:              *output = '7';  break;
            case Num8:              *output = '8';  break;
            case Num9:              *output = '9';  break;
            case Semicolon:         *output = ';';  break;
            case Equals:            *output = '=';  break;
            case A:                 *output = 'a';  break;
            case B:                 *output = 'b';  break;
            case C:                 *output = 'c';  break;
            case D:                 *output = 'd';  break;
            case E:                 *output = 'e';  break;
            case F:                 *output = 'f';  break;
            case G:                 *output = 'g';  break;
            case H:                 *output = 'h';  break;
            case I:                 *output = 'i';  break;
            case J:                 *output = 'j';  break;
            case K:                 *output = 'k';  break;
            case L:                 *output = 'l';  break;
            case M:                 *output = 'm';  break;
            case N:                 *output = 'n';  break;
            case O:                 *output = 'o';  break;
            case P:                 *output = 'p';  break;
            case Q:                 *output = 'q';  break;
            case R:                 *output = 'r';  break;
            case S:                 *output = 's';  break;
            case T:                 *output = 't';  break;
            case U:                 *output = 'u';  break;
            case V:                 *output = 'v';  break;
            case W:                 *output = 'w';  break;
            case X:                 *output = 'x';  break;
            case Y:                 *output = 'y';  break;
            case Z:                 *output = 'z';  break;
            case LeftBracket:       *output = '/';  break;
            case Backslash:         *output = '\\'; break;
            case RightBracket:      *output = ']';  break;
            case GraveAccent:       *output = '`';  break;
            case World1:            break;
            case World2:            break;
            case Escape:            break;
            case Enter:             *output = NSEnterCharacter; break;
            case Tab:               *output = NSTabCharacter; break;
            case Backspace:         *output = NSBackspaceCharacter; break;
            case Insert:            *output = NSInsertFunctionKey; break;
            case Delete:            *output = NSDeleteCharacter; break;
            case Right:             *output = NSRightArrowFunctionKey; break;
            case Left:              *output = NSLeftArrowFunctionKey; break;
            case Down:              *output = NSDownArrowFunctionKey; break;
            case Up:                *output = NSUpArrowFunctionKey; break;
            case PageUp:            *output = NSPageUpFunctionKey; break;
            case PageDown:          *output = NSPageDownFunctionKey; break;
            case Home:              *output = NSHomeFunctionKey; break;
            case End:               *output = NSEndFunctionKey; break;
            case CapsLock:          break;
            case ScrollLock:        *output = NSScrollLockFunctionKey; break;
            case NumLock:           break;
            case PrintScreen:       *output = NSPrintScreenFunctionKey; break;
            case Pause:             *output = NSPauseFunctionKey; break;
            case F1:                *output = NSF1FunctionKey;  break;
            case F2:                *output = NSF2FunctionKey;  break;
            case F3:                *output = NSF3FunctionKey;  break;
            case F4:                *output = NSF4FunctionKey;  break;
            case F5:                *output = NSF5FunctionKey;  break;
            case F6:                *output = NSF6FunctionKey;  break;
            case F7:                *output = NSF7FunctionKey;  break;
            case F8:                *output = NSF8FunctionKey;  break;
            case F9:                *output = NSF9FunctionKey;  break;
            case F10:               *output = NSF10FunctionKey; break;
            case F11:               *output = NSF11FunctionKey; break;
            case F12:               *output = NSF12FunctionKey; break;
            case F13:               *output = NSF13FunctionKey; break;
            case F14:               *output = NSF14FunctionKey; break;
            case F15:               *output = NSF15FunctionKey; break;
            case F16:               *output = NSF16FunctionKey; break;
            case F17:               *output = NSF17FunctionKey; break;
            case F18:               *output = NSF18FunctionKey; break;
            case F19:               *output = NSF19FunctionKey; break;
            case F20:               *output = NSF20FunctionKey; break;
            case F21:               *output = NSF21FunctionKey; break;
            case F22:               *output = NSF22FunctionKey; break;
            case F23:               *output = NSF23FunctionKey; break;
            case F24:               *output = NSF24FunctionKey; break;
            case F25:               *output = NSF25FunctionKey; break;
            case KeyPad0:           *output = '0'; break;
            case KeyPad1:           *output = '1'; break;
            case KeyPad2:           *output = '2'; break;
            case KeyPad3:           *output = '3'; break;
            case KeyPad4:           *output = '4'; break;
            case KeyPad5:           *output = '5'; break;
            case KeyPad6:           *output = '6'; break;
            case KeyPad7:           *output = '7'; break;
            case KeyPad8:           *output = '8'; break;
            case KeyPad9:           *output = '9'; break;
            case KeyPadDecimal:     *output = '.'; break;
            case KeyPadDivide:      *output = '/'; break;
            case KeyPadMultiply:    *output = '*'; break;
            case KeyPadSubtract:    *output = '-'; break;
            case KeyPadAdd:         *output = '+'; break;
            case KeyPadEnter:       *output = NSEnterCharacter; break;
            case KeyPadEqual:       *output = '='; break;
            case Menu:              *output = NSMenuFunctionKey; break;
            default:                break;
        }
    }


    static bool isRunningInAppBundle(void) {
        NSString *bundlePath = [[NSBundle mainBundle] bundlePath];
        return [bundlePath.pathExtension.lowercaseString isEqualToString:@"app"];
    }

    void toastMessageMacos(const char *title, const char *message) {
        @autoreleasepool {
            // Only show notification if we're inside a bundle
            if (!isRunningInAppBundle()) {
                return;
            }

            NSString *nsTitle = [NSString stringWithUTF8String:title];
            NSString *nsMessage = [NSString stringWithUTF8String:message];

            UNUserNotificationCenter *center = [UNUserNotificationCenter currentNotificationCenter];

            // Request permission if needed
            [center requestAuthorizationWithOptions:(UNAuthorizationOptionAlert | UNAuthorizationOptionSound)
                                  completionHandler:^(BOOL granted, NSError * _Nullable error) {

                (void)error;
                if (!granted) return;

                UNMutableNotificationContent *content = [[UNMutableNotificationContent alloc] init];
                content.title = nsTitle;
                content.body = nsMessage;
                content.sound = [UNNotificationSound defaultSound];

                // Show notification immediately
                UNTimeIntervalNotificationTrigger *trigger = [UNTimeIntervalNotificationTrigger triggerWithTimeInterval:0.1 repeats:NO];

                NSString *identifier = [[NSUUID UUID] UUIDString];
                UNNotificationRequest *request = [UNNotificationRequest requestWithIdentifier:identifier
                                                                                      content:content
                                                                                      trigger:trigger];

                [center addNotificationRequest:request withCompletionHandler:nil];
            }];
        }
    }

    #pragma clang diagnostic pop

#endif
