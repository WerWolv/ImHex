#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <list>
#include <vector>

#include <hex/views/view.hpp>

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

        void drawWelcomeScreen();
        void resetLayout();

        void initGLFW();
        void initImGui();
        void deinitGLFW();
        void deinitImGui();

        friend void *ImHexSettingsHandler_ReadOpenFn(ImGuiContext *ctx, ImGuiSettingsHandler *, const char *);
        friend void ImHexSettingsHandler_ReadLine(ImGuiContext*, ImGuiSettingsHandler *handler, void *, const char* line);
        friend void ImHexSettingsHandler_WriteAll(ImGuiContext* ctx, ImGuiSettingsHandler *handler, ImGuiTextBuffer *buf);

        GLFWwindow* m_window = nullptr;

        double m_targetFps = 60.0;
        bool m_demoWindowOpen = false;
        bool m_layoutConfigured = false;

        std::string m_windowTitle;

        double m_lastFrameTime;

        bool m_prevKeysDown[512];

        std::string m_availableUpdate;

        bool m_showTipOfTheDay;
        std::string m_tipOfTheDay;

        ImGui::Texture m_bannerTexture = { 0 };
        ImGui::Texture m_logoTexture = { 0 };

        std::filesystem::path m_safetyBackupPath;

        std::list<std::string> m_popupsToOpen;
        std::vector<int> m_pressedKeys;
    };

}