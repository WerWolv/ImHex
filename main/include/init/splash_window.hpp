#pragma once

#include <functional>
#include <future>
#include <vector>

#include <mutex>

struct GLFWwindow;

namespace hex::init {

    using TaskFunction = std::function<bool()>;

    class WindowSplash {
    public:
        WindowSplash();
        ~WindowSplash();

        bool loop();

        void addStartupTask(const std::string &taskName, const TaskFunction &task, bool async) {
            this->m_tasks.emplace_back(taskName, task, async);
        }

    private:
        GLFWwindow *m_window;
        std::mutex m_progressMutex;
        std::atomic<float> m_progress = 0;
        std::string m_currTaskName;

        void initGLFW();
        void initImGui();

        void exitGLFW();
        void exitImGui();

        std::future<bool> processTasksAsync();

        std::vector<std::tuple<std::string, TaskFunction, bool>> m_tasks;

        std::string m_gpuVendor;
    };

}
