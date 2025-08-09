#pragma once

#include <hex.hpp>

#include <condition_variable>
#include <filesystem>
#include <memory>
#include <string>
#include <list>
#include <vector>

#include <hex/ui/view.hpp>
#include <jthread.hpp>
#include <hex/helpers/opengl.hpp>

struct ImGuiTestEngine;
struct GLFWwindow;
struct ImGuiSettingsHandler;


namespace hex {

    void nativeErrorMessage(const std::string &message);

    class Window {
    public:
        Window();
        ~Window();

        void loop();
        void fullFrame();

        static void initNative();

        void resize(i32 width, i32 height);

    private:
        void configureGLFW();
        void setupNativeWindow();
        void beginNativeWindowFrame();
        void endNativeWindowFrame();
        void drawTitleBar();
        void drawView(const std::string &name, const std::unique_ptr<View> &view);

        void frameBegin();
        void frame();
        void frameEnd();

        void initGLFW();
        void initImGui();
        void exitGLFW();
        void exitImGui();

        void registerEventHandlers();
        void loadPostProcessingShader();

        void drawImGui();
        void drawWithShader();
        void unlockFrameRate();

        GLFWwindow *m_window = nullptr;
        ImGuiTestEngine *m_testEngine = nullptr;

        std::string m_windowTitle, m_windowTitleFull;

        double m_lastStartFrameTime = 0;
        double m_lastFrameTime = 0;

        std::mutex m_popupMutex;
        std::list<std::string> m_popupsToOpen;
        std::set<int> m_pressedKeys;

        ImGuiExt::ImHexCustomData m_imguiCustomData;

        u32 m_searchBarPosition = 0;
        bool m_emergencyPopupOpen = false;
        bool m_shouldUnlockFrameRate = false;
        double m_fpsUnlockedEndTime = 0.0;
        bool m_waitEventsBlocked = false;

        gl::Shader m_postProcessingShader;
    };

}