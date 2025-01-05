#pragma once

#include <condition_variable>
#include <filesystem>
#include <memory>
#include <string>
#include <list>
#include <vector>

#include <hex/ui/view.hpp>
#include <jthread.hpp>

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

        void frameBegin();
        void frame();
        void frameEnd();

        void initGLFW();
        void initImGui();
        void exitGLFW();
        void exitImGui();

        void registerEventHandlers();

        GLFWwindow *m_window = nullptr;

        std::string m_windowTitle, m_windowTitleFull;

        double m_lastStartFrameTime = 0;
        double m_lastFrameTime = 0;

        ImGuiExt::Texture m_logoTexture;

        std::mutex m_popupMutex;
        std::list<std::string> m_popupsToOpen;
        std::vector<int> m_pressedKeys;

        std::atomic<bool> m_unlockFrameRate = false;

        ImGuiExt::ImHexCustomData m_imguiCustomData;

        u32 m_searchBarPosition = 0;
        bool m_emergencyPopupOpen = false;

        std::jthread m_frameRateThread;
        std::atomic<bool> m_sleepFlag;
        std::condition_variable m_sleepCondVar;
        std::mutex m_sleepMutex;
    };

}