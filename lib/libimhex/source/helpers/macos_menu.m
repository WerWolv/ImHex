#import <Foundation/Foundation.h>
#import <Cocoa/Cocoa.h>

struct KeyEquivalent {
    bool valid;
    bool ctrl, opt, cmd, shift;
    int key;
};

const static int MenuBegin = 1;
static NSInteger s_currTag = MenuBegin;
static NSInteger s_selectedTag = -1;

@interface MenuItemHandler : NSObject
-(void) OnClick: (id) sender;
@end

@implementation MenuItemHandler
-(void) OnClick: (id) sender {
    NSMenuItem* menu_item = sender;
    s_selectedTag = menu_item.tag;
}
@end


static NSMenu* s_menuStack[1024];
static int s_menuStackSize = 0;

static MenuItemHandler* s_menuItemHandler;

static bool s_constructingMenu = false;
static bool s_resetNeeded = true;

void macosMenuBarInit(void) {
    s_menuStackSize = 0;
    s_menuStack[0] = NSApp.mainMenu;
    s_menuStackSize += 1;

    s_menuItemHandler = [[MenuItemHandler alloc] init];
}

void macosClearMenu(void) {
    // Remove all items except the Application menu
    while (s_menuStack[0].itemArray.count > 2) {
        [s_menuStack[0] removeItemAtIndex:1];
    }

    s_currTag = MenuBegin;
}

bool macosBeginMainMenuBar(void) {
    if (s_resetNeeded) {
        macosClearMenu();
        s_resetNeeded = false;
    }

    return true;
}

void macosEndMainMenuBar(void) {
    s_constructingMenu = false;
}


static NSMutableArray* g_RegisteredIconFontDescriptors = nil;
void macosRegisterFont(const unsigned char* fontBytes, size_t fontLength) {
    if (!fontBytes || fontLength == 0 || fontLength > 100 * 1024 * 1024) { // Max 100MB sanity check
        NSLog(@"Invalid font data: bytes=%p, length=%zu", fontBytes, fontLength);
        return;
    }

    // Initialize array on first use
    if (!g_RegisteredIconFontDescriptors) {
        g_RegisteredIconFontDescriptors = [[NSMutableArray alloc] init];
    }

    // Create NSData - this will copy the bytes
    NSData *fontData = [NSData dataWithBytes:fontBytes length:fontLength];
    if (!fontData) {
        NSLog(@"Failed to create NSData from font bytes");
        return;
    }

    CFErrorRef error = NULL;
    CFArrayRef descriptors = CTFontManagerCreateFontDescriptorsFromData((__bridge CFDataRef)fontData);

    if (descriptors && CFArrayGetCount(descriptors) > 0) {
        // Register all descriptors from this font file
        CTFontManagerRegisterFontDescriptors(descriptors, kCTFontManagerScopeProcess, true, NULL);
        if (true) {
            // Store each descriptor for later use
            for (CFIndex i = 0; i < CFArrayGetCount(descriptors); i++) {
                CTFontDescriptorRef descriptor = (CTFontDescriptorRef)CFArrayGetValueAtIndex(descriptors, i);
                [g_RegisteredIconFontDescriptors addObject:(__bridge id)descriptor];

                // Log the font name for debugging
                CTFontRef font = CTFontCreateWithFontDescriptor(descriptor, 16.0, NULL);
                if (font) {
                    CFRelease(font);
                }
            }
        } else {
            NSLog(@"Failed to register font descriptors");
        }

        CFRelease(descriptors);
    } else {
        NSLog(@"Failed to create font descriptors from data (length: %zu)", fontLength);
        if (error) {
            NSLog(@"Error: %@", (__bridge NSError *)error);
            CFRelease(error);
        }
    }
}

static NSImage* imageFromIconFont(NSString* character,
                           CGFloat size,
                           NSColor* color) {

    if (!character || [character length] == 0) {
        return nil;
    }

    if (!g_RegisteredIconFontDescriptors || [g_RegisteredIconFontDescriptors count] == 0) {
        NSLog(@"No icon fonts registered");
        return nil;
    }

    NSFont *font = nil;
    unichar unicode = [character characterAtIndex:0];

    for (id descriptorObj in g_RegisteredIconFontDescriptors) {
        CTFontDescriptorRef descriptor = (__bridge CTFontDescriptorRef)descriptorObj;
        CTFontRef ctFont = CTFontCreateWithFontDescriptor(descriptor, size, NULL);

        if (ctFont) {
            CGGlyph glyph;
            UniChar unichar = unicode;
            if (CTFontGetGlyphsForCharacters(ctFont, &unichar, &glyph, 1)) {
                font = (__bridge NSFont *)ctFont;
                break;
            }
            CFRelease(ctFont);
        }
    }

    if (!font) {
        NSLog(@"No font found with glyph for character U+%04X (string: '%@', length: %lu)",
              unicode, character, (unsigned long)[character length]);
        return nil;
    }

    NSDictionary *attributes = @{
        NSFontAttributeName: font,
        NSForegroundColorAttributeName: color
    };
    NSAttributedString *attrString = [[NSAttributedString alloc]
                                      initWithString:character
                                      attributes:attributes];

    NSSize stringSize = [attrString size];

    NSImage *image = [[NSImage alloc] initWithSize:NSMakeSize(size, size)];

    [image lockFocus];

    NSPoint drawPoint = NSMakePoint((size - stringSize.width) / 2.0,
                                    (size - stringSize.height) / 2.0);
    [attrString drawAtPoint:drawPoint];

    [image unlockFocus];
    [image setTemplate:YES];

    return image;
}

bool macosBeginMenu(const char* label, const char *icon, bool enabled) {
    NSString* title = [NSString stringWithUTF8String:label];

    // Search for menu item with the given name
    NSInteger menuIndex = [s_menuStack[s_menuStackSize - 1] indexOfItemWithTitle:title];
    if (menuIndex == -1) {
        // Construct the content of the menu if it doesn't exist yet

        s_constructingMenu = true;

        NSMenu* newMenu = [[NSMenu alloc] init];
        newMenu.autoenablesItems = false;
        newMenu.title = title;

        NSMenuItem* menuItem = [[NSMenuItem alloc] init];
        menuItem.title = title;
        [menuItem setSubmenu:newMenu];

        if (icon != NULL) {
            NSString *iconString = [NSString stringWithUTF8String:icon];
            NSImage* iconImage = imageFromIconFont(iconString, 16.0, [NSColor blackColor]);
            [menuItem setImage:iconImage];
        }

        // Add the new menu to the end of the list
        menuIndex = [s_menuStack[s_menuStackSize - 1] numberOfItems];

        if (s_menuStackSize == 1)
            menuIndex -= 1;

        [s_menuStack[s_menuStackSize - 1] insertItem:menuItem atIndex:menuIndex];
    }

    NSMenuItem* menuItem = [s_menuStack[s_menuStackSize - 1] itemAtIndex:menuIndex];
    if (menuItem != NULL) {
        menuItem.enabled = enabled;

        s_menuStack[s_menuStackSize] = menuItem.submenu;
        s_menuStackSize += 1;
    }

    return true;
}

void macosEndMenu(void) {
    s_menuStack[s_menuStackSize - 1] = NULL;
    s_menuStackSize -= 1;
}

bool macosMenuItem(const char* label, const char *icon, struct KeyEquivalent keyEquivalent, bool selected, bool enabled) {
    NSString* title = [NSString stringWithUTF8String:label];

    if (s_constructingMenu) {
        NSMenuItem* menuItem = [[NSMenuItem alloc] init];
        menuItem.title = title;
        menuItem.action = @selector(OnClick:);
        menuItem.target = s_menuItemHandler;

        if (icon != NULL) {
            NSString *iconString = [NSString stringWithUTF8String:icon];
            NSImage* iconImage = imageFromIconFont(iconString, 16.0, [NSColor blackColor]);
            [menuItem setImage:iconImage];
        }

        [menuItem setTag:s_currTag];
        s_currTag += 1;

        // Setup the key equivalent, aka the shortcut
        if (keyEquivalent.valid) {
            NSInteger flags = 0x00;

            if (keyEquivalent.ctrl)
                flags |= NSEventModifierFlagControl;
            if (keyEquivalent.shift)
                flags |= NSEventModifierFlagShift;
            if (keyEquivalent.cmd)
                flags |= NSEventModifierFlagCommand;
            if (keyEquivalent.opt)
                flags |= NSEventModifierFlagOption;

            [menuItem setKeyEquivalentModifierMask:flags];
            menuItem.keyEquivalent = [[NSString alloc] initWithCharacters:(const unichar *)&keyEquivalent.key length:1];
        }

        [s_menuStack[s_menuStackSize - 1] addItem:menuItem];
    }

    NSInteger menuIndex = [s_menuStack[s_menuStackSize - 1] indexOfItemWithTitle:title];
    NSMenuItem* menuItem = NULL;
    if (menuIndex >= 0 && menuIndex < [s_menuStack[s_menuStackSize - 1] numberOfItems]) {
        menuItem = [s_menuStack[s_menuStackSize - 1] itemAtIndex:menuIndex];
        if (menuItem != NULL) {
            if (s_constructingMenu == false) {
                if (![title isEqualToString:menuItem.title]) {
                    s_resetNeeded = true;
                }
            }

            menuItem.enabled = enabled;
            menuItem.state = selected ? NSControlStateValueOn : NSControlStateValueOff;
        }

        if (enabled && menuItem != NULL) {
            if ([menuItem tag] == s_selectedTag) {
                s_selectedTag = -1;
                return true;
            }
        }
    } else {
        s_resetNeeded = true;
    }

    return false;
}

bool macosMenuItemSelect(const char* label, const char *icon, struct KeyEquivalent keyEquivalent, bool* selected, bool enabled) {
    if (macosMenuItem(label, icon, keyEquivalent, selected != NULL ? *selected : false, enabled)) {
        if (selected != NULL)
            *selected = !(*selected);

        return true;
    }
    return false;
}

void macosSeparator(void) {
    if (s_constructingMenu) {
        NSMenuItem* separator = [NSMenuItem separatorItem];
        [s_menuStack[s_menuStackSize - 1] addItem:separator];
    }
}
