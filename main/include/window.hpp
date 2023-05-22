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

    void nativeErrorMessage(const std::string&);

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

        void drawTitleBarBorderless();
        void drawTitleBarBorder();

        void frameBegin();
        void frame();
        void frameEnd();

        void processEvent() { this->m_hadEvent = true; }

        void initGLFW();
        void initImGui();
        void exitGLFW();
        void exitImGui();

        void registerEventHandlers();

        GLFWwindow *m_window = nullptr;

        std::string m_windowTitle;

        double m_lastFrameTime = 0;

        ImGui::Texture m_logoTexture;

        std::mutex m_popupMutex;
        std::list<std::string> m_popupsToOpen;
        std::vector<int> m_pressedKeys;

        std::fs::path m_imguiSettingsPath;

        bool m_mouseButtonDown = false;

        bool m_hadEvent = false;
        bool m_frameRateTemporarilyUnlocked = false;
        double m_frameRateUnlockTime = 0;
    };

}