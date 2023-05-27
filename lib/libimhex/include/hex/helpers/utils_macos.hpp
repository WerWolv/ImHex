#pragma once

#if defined(OS_MACOS)

    extern "C" {

        void errorMessageMacos(const char *message);
        void openWebpageMacos(const char *url);
        bool isMacosSystemDarkModeEnabled();
        float getBackingScaleFactor();

    }

#endif