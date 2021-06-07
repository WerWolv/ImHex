#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <hex/helpers/utils.hpp>
#include <hex/views/view.hpp>

struct GLFWwindow;
struct ImGuiSettingsHandler;

namespace hex {

    class Window {
    public:
        Window();
        ~Window();

        void loop();

        friend void *ImHexSettingsHandler_ReadOpenFn(ImGuiContext *ctx, ImGuiSettingsHandler *, const char *);
        friend void ImHexSettingsHandler_ReadLine(ImGuiContext*, ImGuiSettingsHandler *handler, void *, const char* line);
        friend void ImHexSettingsHandler_ApplyAll(ImGuiContext *ctx, ImGuiSettingsHandler *handler);
        friend void ImHexSettingsHandler_WriteAll(ImGuiContext* ctx, ImGuiSettingsHandler *handler, ImGuiTextBuffer *buf);

        bool setFont(const std::filesystem::path &font_path);
    private:
        void frameBegin();
        void frame();
        void frameEnd();

        void drawWelcomeScreen();
        void resetLayout();

        void initGLFW();
        void initImGui();
        void deinitGLFW();
        void deinitImGui();

        GLFWwindow* m_window = nullptr;

        float m_globalScale = 1.0f, m_fontScale = 1.0f;
        double m_targetFps = 60.0;
        bool m_demoWindowOpen = false;
        bool m_layoutConfigured = false;

        double m_lastFrameTime;

        bool m_prevKeysDown[512];

        std::string m_availableUpdate;
    };

}