#pragma once

#include <functional>
#include <future>
#include <vector>

#include <mutex>

struct GLFWwindow;

namespace hex::init {

    using TaskFunction = std::function<bool()>;

    enum FrameResult{ success, failure, wait };

    struct Highlight {
        ImVec2 start;
        size_t count;
        ImColor color;
    };

    class WindowSplash {
    public:
        WindowSplash();
        ~WindowSplash();

        bool loop();

        FrameResult fullFrame();
        void startStartupTasks();

        void addStartupTask(const std::string &taskName, const TaskFunction &task, bool async) {
            this->m_tasks.emplace_back(taskName, task, async);
        }

    private:
        GLFWwindow *m_window;
        std::mutex m_progressMutex;
        std::atomic<float> m_progress = 0;
        std::list<std::string> m_currTaskNames;

        void initGLFW();
        void initImGui();
        void initMyself();

        void exitGLFW();
        void exitImGui();

        std::future<bool> processTasksAsync();

        std::vector<std::tuple<std::string, TaskFunction, bool>> m_tasks;

        std::string m_gpuVendor;
    
        ImGui::Texture splashBackgroundTexture;
        ImGui::Texture splashTextTexture;
        std::future<bool> tasksSucceeded;
        std::array<Highlight, 3> highlights;
        float progressLerp = 0.0F;
    };

}
