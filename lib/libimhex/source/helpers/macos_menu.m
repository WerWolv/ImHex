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

bool macosBeginMenu(const char* label, bool enabled) {
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

bool macosMenuItem(const char* label, struct KeyEquivalent keyEquivalent, bool selected, bool enabled) {
    NSString* title = [NSString stringWithUTF8String:label];

    if (s_constructingMenu) {
        NSMenuItem* menuItem = [[NSMenuItem alloc] init];
        menuItem.title = title;
        menuItem.action = @selector(OnClick:);
        menuItem.target = s_menuItemHandler;

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

bool macosMenuItemSelect(const char* label, struct KeyEquivalent keyEquivalent, bool* selected, bool enabled) {
    if (macosMenuItem(label, keyEquivalent, selected != NULL ? *selected : false, enabled)) {
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
