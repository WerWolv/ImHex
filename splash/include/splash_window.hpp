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

        bool loop();

        void addStartupTask(std::string_view taskName, const std::function<bool()> &task) {
            this->m_tasks.emplace_back(taskName, task);
        }

    private:
        GLFWwindow *m_window;
        std::mutex m_progressMutex;
        float m_progress;
        std::string m_currTaskName;

        void initGLFW();
        void initImGui();

        void deinitGLFW();
        void deinitImGui();

        std::future<bool> processTasksAsync();

        std::vector<std::pair<std::string, std::function<bool()>>> m_tasks;
    };

}