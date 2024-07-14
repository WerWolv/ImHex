#include <hex/ui/glfw_di.h>

#include <GLFW/glfw3.h>

#include <new>
#include <stdexcept>

namespace hex::glfw {
    struct GlfwData {
        void *userPointer = nullptr;
        GLFWcursorposfun userCursorPosFn = nullptr;
        GLFWframebuffersizefun userFramebufferSizeFn = nullptr;
        GLFWwindowcontentscalefun userWindowContentScaleFn = nullptr;
        GLFWwindowposfun userWindowPosFn = nullptr;
        GLFWwindowsizefun userWindowSizeFn = nullptr;
        float scaleX = 1.0F;
        float scaleY = 1.0F;
    };

    static void RecalculateScale(GLFWwindow *window, GlfwData *data) {
        int width, height, framebufferWidth, framebufferHeight;
        float scaleX, scaleY;
        glfwGetWindowSize(window, &width, &height);
        glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
        glfwGetWindowContentScale(window, &scaleX, &scaleY);
        data->scaleX = scaleX / (float(framebufferWidth) / width);
        data->scaleY = scaleY / (float(framebufferHeight) / height);
    }

    static GlfwData* GetWindowData(GLFWwindow *window) {
        if (!window)
            return nullptr;

        return static_cast<GlfwData *>(glfwGetWindowUserPointer(window));
    }

    template <typename T>
    void FromGLFW(GLFWwindow *window, T *x, T *y) {
        auto data = GetWindowData(window);
        if (!data)
            throw std::runtime_error("Missing window data; did you use glfwCreateWindow directly instead of hex::glfw::CreateWindow?");
        if (x)
            *x /= data->scaleX;
        if (y)
            *y /= data->scaleY;
    }

    template <typename T>
    static void FromGLFW(GLFWmonitor *monitor, T *x, T *y) {
        float xScale, yScale;
        glfwGetMonitorContentScale(monitor, &xScale, &yScale);
        if (x)
            *x /= xScale;
        if (y)
            *y /= yScale;
    }

    template <typename T>
    void ToGLFW(GLFWwindow *window, T *x, T *y) {
        auto data = GetWindowData(window);
        if (!data)
            throw std::runtime_error("Missing window data; did you use glfwCreateWindow directly instead of hex::glfw::CreateWindow?");
        if (x)
            *x *= data->scaleX;
        if (y)
            *y *= data->scaleY;
    }

    template <typename T>
    static void ToGLFW(GLFWmonitor *monitor, T *x, T *y) {
        float xScale, yScale;
        glfwGetMonitorContentScale(monitor, &xScale, &yScale);
        if (x)
            *x *= xScale;
        if (y)
            *y *= yScale;
    }

    GLFWwindow* CreateWindow(int diWidth, int diHeight, const char* title, GLFWmonitor* monitor, GLFWwindow* share) {
        auto window = glfwCreateWindow(1, 1, title, monitor, share);
        if (!window)
            return nullptr;

        auto *data = new (std::nothrow) GlfwData;
        if (!data) {
            glfwDestroyWindow(window);
            return nullptr;
        }

        glfwSetWindowUserPointer(window, data);
        RecalculateScale(window, data);
        glfwSetWindowSize(window, diWidth * data->scaleX, diHeight * data->scaleY);
        glfwSetFramebufferSizeCallback(window, [](GLFWwindow *window, int width, int height) {
            auto data = GetWindowData(window);
            RecalculateScale(window, data);
            if (data->userFramebufferSizeFn)
                (data->userFramebufferSizeFn)(window, width, height);
        });
        glfwSetWindowContentScaleCallback(window, [](GLFWwindow *window, float scaleX, float scaleY) {
            auto data = GetWindowData(window);
            RecalculateScale(window, data);
            if (data->userWindowContentScaleFn)
                (data->userWindowContentScaleFn)(window, scaleX, scaleY);
        });

        return window;
    }

    void DestroyWindow(GLFWwindow *window) {
        if (!window)
            return;

        delete GetWindowData(window);
        glfwDestroyWindow(window);
    }

    void *GetWindowUserPointer(GLFWwindow *window) {
        if (!window)
            return nullptr;

        return GetWindowData(window)->userPointer;
    }

    void *SetWindowUserPointer(GLFWwindow *window, void *pointer) {
        if (!window)
            return nullptr;

        auto data = GetWindowData(window);
        auto prev = data->userPointer;
        data->userPointer = pointer;
        return prev;
    }

    bool GetMonitorSize(GLFWmonitor *monitor, int *diWidth, int *diHeight) {
        auto mode = glfwGetVideoMode(monitor);
        if (!mode)
            return false;

        if (diWidth)
            *diWidth = mode->width;
        if (diHeight)
            *diHeight = mode->height;
        FromGLFW(monitor, diWidth, diHeight);

        return true;
    }

    void GetMonitorPos(GLFWmonitor *monitor, int *diX, int *diY) {
        glfwGetMonitorPos(monitor, diX, diY);
        FromGLFW(monitor, diX, diY);
    }

    void SetWindowMonitor(GLFWwindow* window, GLFWmonitor* monitor, int diXpos, int diYpos, int diWidth, int diHeight, int refreshRate) {
        if (monitor)
            ToGLFW(monitor, &diXpos, &diYpos);
        else
            ToGLFW(window, &diXpos, &diYpos);

        ToGLFW(window, &diWidth, &diHeight);
        glfwSetWindowMonitor(window, monitor, diXpos, diYpos, diWidth, diHeight, refreshRate);
    }

    void GetWindowPos(GLFWwindow *window, int *diX, int *diY) {
        glfwGetWindowPos(window, diX, diY);
        FromGLFW(window, diX, diY);
    }

    void SetWindowPos(GLFWwindow *window, int diX, int diY) {
        ToGLFW(window, &diX, &diY);
        glfwSetWindowPos(window, diX, diY);
    }

    void GetWindowSize(GLFWwindow *window, int *diWidth, int *diHeight) {
        glfwGetWindowSize(window, diWidth, diHeight);
        FromGLFW(window, diWidth, diHeight);
    }

    void SetWindowSize(GLFWwindow *window, int diWidth, int diHeight) {
        ToGLFW(window, &diWidth, &diHeight);
        glfwSetWindowSize(window, diWidth, diHeight);
    }

    void SetWindowSizeLimits(GLFWwindow *window, int diMinWidth, int diMinHeight, int diMaxWidth, int diMaxHeight) {
        ToGLFW(window,
            diMinWidth == GLFW_DONT_CARE ? nullptr : &diMinWidth,
            diMinHeight == GLFW_DONT_CARE ? nullptr : &diMinHeight);
        ToGLFW(window,
            diMaxWidth == GLFW_DONT_CARE ? nullptr : &diMaxWidth,
            diMaxHeight == GLFW_DONT_CARE ? nullptr : &diMaxHeight);
        glfwSetWindowSizeLimits(window, diMinWidth, diMinHeight, diMaxWidth, diMaxHeight);
    }

    #define IMHEX_WINDOW_CALLBACK_SCALE(fnName, cbType, paramType)              \
    static void fnName##Wrapper(GLFWwindow *window, paramType x, paramType y) { \
        FromGLFW(window, &x, &y);                                               \
        (GetWindowData(window)->user##fnName##Fn)(window, x, y);                \
    }                                                                           \
                                                                                \
    cbType Set##fnName##Callback(GLFWwindow *window, cbType callback) {         \
        auto data = GetWindowData(window);                                      \
        auto previous = data->user##fnName##Fn;                                 \
        data->user##fnName##Fn = callback;                                      \
        glfwSet##fnName##Callback(window, callback ? fnName##Wrapper : nullptr);\
        return previous;                                                        \
    }

    IMHEX_WINDOW_CALLBACK_SCALE(CursorPos, GLFWcursorposfun, double)
    IMHEX_WINDOW_CALLBACK_SCALE(WindowPos, GLFWwindowposfun, int)
    IMHEX_WINDOW_CALLBACK_SCALE(WindowSize, GLFWwindowsizefun, int)
    #undef IMHEX_WINDOW_CALLBACK_SCALE

    #define IMHEX_WINDOW_CALLBACK_RECALC(fnName, cbType)                        \
    cbType Set##fnName##Callback(GLFWwindow *window, cbType callback) {         \
        auto data = GetWindowData(window);                                      \
        auto previous = data->user##fnName##Fn;                                 \
        data->user##fnName##Fn = callback;                                      \
        return previous;                                                        \
    }

    IMHEX_WINDOW_CALLBACK_RECALC(FramebufferSize, GLFWframebuffersizefun)
    IMHEX_WINDOW_CALLBACK_RECALC(WindowContentScale, GLFWwindowcontentscalefun)
    #undef IMHEX_WINDOW_CALLBACK_RECALC
}
