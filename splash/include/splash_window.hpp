#pragma once

#include <functional>
#include <future>
#include <vector>

#include <mutex>

struct GLFWwindow;

namespace hex::pre {

    class WindowSplash {
    public:
        WindowSplash();
        ~WindowSplash();

        void loop();

        void addStartupTask(const std::function<bool()> &task) {
            this->m_tasks.push_back(task);
        }

    private:
        GLFWwindow *m_window;
        std::mutex m_progressMutex;
        float m_progress;

        void initGLFW();
        void initImGui();

        void deinitGLFW();
        void deinitImGui();

        std::future<bool> processTasksAsync();

        std::vector<std::function<bool()>> m_tasks;
    };

}