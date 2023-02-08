#pragma once

#if defined(OS_MACOS)

    extern "C" {

        void openWebpageMacos(const char *url);
        bool isMacosSystemDarkModeEnabled();
        float getBackingScaleFactorMacos();

    }

#endif