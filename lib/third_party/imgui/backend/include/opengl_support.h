#pragma once

#if !defined(WINGDIAPI)
#define WINGDIAPI extern "C"
#define APIENTRY
#endif

#if defined(OS_WEB)
    #define GLFW_INCLUDE_ES3
    #include <GLES3/gl3.h>
#else
    #include <imgui_impl_opengl3_loader.h>
#endif