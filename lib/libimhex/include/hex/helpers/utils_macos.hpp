#pragma once

#if defined(OS_MACOS)

    struct GLFWwindow;

    extern "C" {

        void errorMessageMacos(const char *message);
        void openWebpageMacos(const char *url);
        bool isMacosSystemDarkModeEnabled();
        bool isMacosFullScreenModeEnabled(GLFWwindow *window);
        float getBackingScaleFactor();

        void setupMacosWindowStyle(GLFWwindow *window, bool borderlessWindowMode);

        void enumerateFontsMacos();
    
        void macosHandleTitlebarDoubleClickGesture(GLFWwindow *window);
        bool macosIsWindowBeingResizedByUser(GLFWwindow *window);
        void macosMarkContentEdited(GLFWwindow *window, bool edited = true);
    }

#endif
