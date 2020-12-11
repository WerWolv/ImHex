#pragma once

#include <concepts>
#include <filesystem>
#include <memory>
#include <vector>

#include "views/view.hpp"

struct GLFWwindow;
struct ImGuiSettingsHandler;

namespace hex {

    class Window {
    public:
        Window();
        ~Window();

        void loop();

        template<std::derived_from<View> T, typename ... Args>
        T* addView(Args&& ... args) {
            this->m_views.emplace_back(new T(std::forward<Args>(args)...));

            return static_cast<T*>(this->m_views.back());
        }

        friend void *ImHexSettingsHandler_ReadOpenFn(ImGuiContext *ctx, ImGuiSettingsHandler *, const char *);
        friend void ImHexSettingsHandler_ReadLine(ImGuiContext*, ImGuiSettingsHandler *handler, void *, const char* line);
        friend void ImHexSettingsHandler_ApplyAll(ImGuiContext *ctx, ImGuiSettingsHandler *handler);
        friend void ImHexSettingsHandler_WriteAll(ImGuiContext* ctx, ImGuiSettingsHandler *handler, ImGuiTextBuffer *buf);

        bool setFont(const std::filesystem::path &font_path);

    private:
        void frameBegin();
        void frameEnd();

        void initGLFW();
        void initImGui();
        void deinitGLFW();
        void deinitImGui();

        GLFWwindow* m_window;
        std::vector<View*> m_views;

        float m_globalScale = 1.0f, m_fontScale = 1.0f;
        bool m_fpsVisible = false;
        bool m_demoWindowOpen = false;

        static inline std::tuple<int, int> s_currShortcut = { -1, -1 };
    };

}