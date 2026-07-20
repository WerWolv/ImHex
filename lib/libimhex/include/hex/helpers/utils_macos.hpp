#pragma once

#include <hex/helpers/keys.hpp>

#if defined(OS_MACOS)

    #if !defined(HEX_MODULE_EXPORT)
    #endif

    extern "C" {

        void errorMessageMacos(const char *message);
        void openWebpageMacos(const char *url);
        bool isMacosSystemDarkModeEnabled();
        bool isMacosFullScreenModeEnabled(void *window);
        float getBackingScaleFactor();

        void setupMacosWindowStyle(void *window, bool borderlessWindowMode);

        void enumerateFontsMacos();
    
        void macosHandleTitlebarDoubleClickGesture(void *window);
        void macosSetWindowMovable(void *window, bool movable);
        bool macosIsWindowBeingResizedByUser(void *window);
        void macosMarkContentEdited(void *window, bool edited = true);

        void macosGetKey(Keys key, int *output);

        bool macosIsMainInstance();
        void macosSendMessageToMainInstance(const unsigned char *data, size_t size);
        void macosInstallEventListener();

        void toastMessageMacos(const char *title, const char *message);
        void macosSetupDockMenu(void);
    }

#endif
