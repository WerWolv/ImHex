#pragma once

struct GLFWwindow;
struct GLFWmonitor;
typedef void (* GLFWcursorposfun)(GLFWwindow* window, double diX, double diY);
typedef void (* GLFWframebuffersizefun)(GLFWwindow* window, int width, int height);
typedef void (* GLFWwindowcontentscalefun)(GLFWwindow* window, float xscale, float yscale);
typedef void (* GLFWwindowposfun)(GLFWwindow* window, int diX, int diY);
typedef void (* GLFWwindowsizefun)(GLFWwindow* window, int diWidth, int diHeight);

namespace hex {
    /**
     * These functions translate GLFW coordinates to/from a device-independent
     * coordinate system so application code does not need to perform any
     * platform-specific scaling transformations itself.
     *
     * This ought to be handled by GLFW, but GLFW <=3.4 leaks the platform’s
     * coordinate system instead of abstracting it. Some platforms use a
     * device-independent coordinate system (Wayland, macOS, Web) where others
     * do not (X11, Win32), and this detail should not be leaking into
     * application code.
     */
    namespace glfw {
        GLFWwindow *CreateWindow(int diWidth, int diHeight, const char* title, GLFWmonitor* monitor, GLFWwindow* share);
        void DestroyWindow(GLFWwindow *window);
        void *GetWindowUserPointer(GLFWwindow *window);
        void *SetWindowUserPointer(GLFWwindow *window, void *pointer);
        void GetMonitorPos(GLFWmonitor *monitor, int *diX, int *diY);
        bool GetMonitorSize(GLFWmonitor *monitor, int *diWidth, int *diHeight);
        void SetWindowMonitor(GLFWwindow* window, GLFWmonitor* monitor, int diXpos, int diYpos, int diWidth, int diHeight, int refreshRate);
        void GetWindowPos(GLFWwindow *window, int *diX, int *diY);
        void SetWindowPos(GLFWwindow *window, int diX, int diY);
        void GetWindowSize(GLFWwindow *window, int *diWidth, int *diHeight);
        void SetWindowSize(GLFWwindow *window, int diWidth, int diHeight);
        void SetWindowSizeLimits(GLFWwindow *window, int diMinWidth, int diMinHeight, int diMaxWidth, int diMaxHeight);
        GLFWcursorposfun SetCursorPosCallback(GLFWwindow *window, GLFWcursorposfun callback);
        GLFWwindowposfun SetFramebufferSizeCallback(GLFWwindow *window, GLFWframebuffersizefun callback);
        GLFWwindowcontentscalefun SetWindowContentScaleCallback(GLFWwindow *window, GLFWwindowcontentscalefun callback);
        GLFWwindowposfun SetWindowPosCallback(GLFWwindow *window, GLFWwindowposfun callback);
        GLFWwindowsizefun SetWindowSizeCallback(GLFWwindow *window, GLFWwindowsizefun callback);
    }
}
