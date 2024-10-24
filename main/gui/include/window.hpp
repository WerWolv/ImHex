#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <list>
#include <vector>
#include <mutex>  // Add this for std::mutex

#include <hex/ui/view.hpp>  // Ensure that this includes necessary types (ImGuiExt::Texture, etc.)

struct GLFWwindow;

namespace hex {

    void nativeErrorMessage(const std::string &message);

    class Window {
    public:
        Window();
        ~Window();

        // Delete copy constructor and copy assignment operator to prevent accidental copies
        Window(const Window&) = delete;
        Window& operator=(const Window&) = delete;

        // Implement move constructor and move assignment operator if needed
        Window(Window&&) noexcept = default;
        Window& operator=(Window&&) noexcept = default;

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

        std::unique_ptr<GLFWwindow> m_window = nullptr;  // Use std::unique_ptr for memory safety

        std::string m_windowTitle;

        double m_lastStartFrameTime = 0;
        double m_lastFrameTime = 0;

        ImGuiExt::Texture m_logoTexture;

        std::mutex m_popupMutex;
        std::list<std::string> m_popupsToOpen;
        std::vector<int> m_pressedKeys;

        bool m_unlockFrameRate = false;

        ImGuiExt::ImHexCustomData m_imguiCustomData;

        // Define constant for search bar position if needed
        static constexpr u32 kDefaultSearchBarPosition = 0;
        u32 m_searchBarPosition = kDefaultSearchBarPosition;

        bool m_emergencyPopupOpen = false;
    };

}
