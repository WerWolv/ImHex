#pragma once

#include <functional>
#include <future>
#include <list>
#include <memory>
#include <ranges>
#include <string>

#include <mutex>

#include <imgui.h>
#include <hex/ui/imgui_imhex_extensions.h>
#include <hex/api/imhex_api/system.hpp>

namespace hex::init {

    using TaskFunction = std::function<bool()>;

    struct Task {
        std::string name;
        std::function<bool()> callback;
        bool async;
        bool running;
    };

    struct Highlight {
        ImVec2 start;
        size_t count;
        ImColor color;
    };

    class WindowSplash {
    public:
        WindowSplash();
        ~WindowSplash();

        std::optional<bool> loop();
        void startStartupTaskExecution();
        void addStartupTask(const std::string &taskName, const TaskFunction &function, bool async);

    private:
        void createTask(const Task &task);
        void fullFrame();

    private:
        std::unique_ptr<ImHexApi::System::WindowBackend> m_backend;
        std::mutex m_progressMutex;
        std::atomic<float> m_progress = 0;
        std::list<std::string> m_currTaskNames;

        void initWindow();
        void initImGui();
        void loadAssets();

        void exitWindow();
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
