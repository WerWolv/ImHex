#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <list>
#include <vector>

#include <hex/ui/view.hpp>

struct GLFWwindow;
struct ImGuiSettingsHandler;


namespace hex {

    class Window {
    public:
        Window();
        ~Window();

        void loop();

        static void initNative();

    private:
        void setupNativeWindow();
        void beginNativeWindowFrame();
        void endNativeWindowFrame();
        void drawTitleBar();

        void frameBegin();
        void frame();
        void frameEnd();

        void initGLFW();
        void initImGui();
        void exitGLFW();
        void exitImGui();

        friend void *ImHexSettingsHandler_ReadOpenFn(ImGuiContext *ctx, ImGuiSettingsHandler *, const char *);
        friend void ImHexSettingsHandler_ReadLine(ImGuiContext *, ImGuiSettingsHandler *handler, void *, const char *line);
        friend void ImHexSettingsHandler_WriteAll(ImGuiContext *ctx, ImGuiSettingsHandler *handler, ImGuiTextBuffer *buf);

        GLFWwindow *m_window = nullptr;

        std::string m_windowTitle;

        double m_lastFrameTime = 0;

        ImGui::Texture m_logoTexture = { nullptr };

        std::list<std::string> m_popupsToOpen;
        std::vector<int> m_pressedKeys;

        std::fs::path m_imguiSettingsPath;

        bool m_mouseButtonDown = false;
    };

}