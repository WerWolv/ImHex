#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <hex/helpers/utils.hpp>
#include <hex/views/view.hpp>

#include <imgui_imhex_extensions.h>

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
        void updateNativeWindow();
        void drawTitleBar();

        void frameBegin();
        void frame();
        void frameEnd();

        void drawWelcomeScreen();
        void resetLayout();

        void initGLFW();
        void initImGui();
        void deinitGLFW();
        void deinitImGui();

        friend void *ImHexSettingsHandler_ReadOpenFn(ImGuiContext *ctx, ImGuiSettingsHandler *, const char *);
        friend void ImHexSettingsHandler_ReadLine(ImGuiContext*, ImGuiSettingsHandler *handler, void *, const char* line);
        friend void ImHexSettingsHandler_WriteAll(ImGuiContext* ctx, ImGuiSettingsHandler *handler, ImGuiTextBuffer *buf);

        void setFont(const std::filesystem::path &font_path);

        GLFWwindow* m_window = nullptr;

        float m_globalScale = 1.0F, m_fontScale = 1.0F;
        double m_targetFps = 60.0;
        bool m_demoWindowOpen = false;
        bool m_layoutConfigured = false;

        std::string m_windowTitle;

        double m_lastFrameTime;

        bool m_prevKeysDown[512];

        std::string m_availableUpdate;

        ImGui::Texture m_bannerTexture;
        ImGui::Texture m_logoTexture;

        std::filesystem::path m_safetyBackupPath;
    };

}