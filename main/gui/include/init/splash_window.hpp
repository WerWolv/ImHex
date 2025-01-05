#pragma once

#include <functional>
#include <future>
#include <list>
#include <ranges>
#include <string>

#include <mutex>

#include <imgui.h>
#include <hex/ui/imgui_imhex_extensions.h>

struct GLFWwindow;

namespace hex::init {

    using TaskFunction = std::function<bool()>;

    struct Task {
        std::string name;
        std::function<bool()> callback;
        bool async;
        bool running;
    };

    enum FrameResult{ Success, Failure, Running };

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

        void createTask(const Task &task);

        void addStartupTask(const std::string &taskName, const TaskFunction &function, bool async) {
            std::scoped_lock lock(m_tasksMutex);

            m_tasks.emplace_back(taskName, function, async);
        }

    private:
        GLFWwindow *m_window;
        std::mutex m_progressMutex;
        std::atomic<float> m_progress = 0;
        std::list<std::string> m_currTaskNames;

        void initGLFW();
        void initImGui();
        void loadAssets();

        void exitGLFW() const;
        void exitImGui() const;

        std::future<bool> processTasksAsync();

        std::atomic<u32> m_totalTaskCount, m_completedTaskCount;
        std::atomic<bool> m_taskStatus = true;
        std::list<Task> m_tasks;
        std::mutex m_tasksMutex;

        std::string m_gpuVendor;
    
        ImGuiExt::Texture m_splashBackgroundTexture;
        ImGuiExt::Texture m_splashTextTexture;
        std::future<bool> m_tasksSucceeded;
        std::array<Highlight, 4> m_highlights;
        float m_progressLerp = 0.0F;
    };

}
