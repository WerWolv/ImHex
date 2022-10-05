#pragma once

#include <hex.hpp>

#include <cstdio>
#include <thread>
#include <functional>
#include <cstdint>
#include <mutex>
#include <chrono>
#include <memory>
#include <list>
#include <condition_variable>

namespace hex {

    class TaskHolder;
    class TaskManager;

    class Task {
    public:
        Task() = default;
        Task(std::string unlocalizedName, u64 maxValue, bool background, std::function<void(Task &)> function);

        Task(const Task&) = delete;
        Task(Task &&other) noexcept;
        ~Task();

        void update(u64 value = 0);
        void setMaxValue(u64 value);

        [[nodiscard]] bool isBackgroundTask() const;
        [[nodiscard]] bool isFinished() const;
        [[nodiscard]] bool hadException() const;
        [[nodiscard]] bool wasInterrupted() const;
        void clearException();
        [[nodiscard]] std::string getExceptionMessage() const;

        [[nodiscard]] const std::string &getUnlocalizedName();
        [[nodiscard]] u64 getValue() const;
        [[nodiscard]] u64 getMaxValue() const;

        void interrupt();

        void setInterruptCallback(std::function<void()> callback);

    private:
        void finish();
        void interruption();
        void exception(const char *message);

    private:
        mutable std::mutex m_mutex;

        std::string m_unlocalizedName;
        std::atomic<u64> m_currValue = 0, m_maxValue = 0;
        std::function<void()> m_interruptCallback;
        std::function<void(Task &)> m_function;

        std::atomic<bool> m_shouldInterrupt = false;
        std::atomic<bool> m_background = true;

        std::atomic<bool> m_interrupted = false;
        std::atomic<bool> m_finished = false;
        std::atomic<bool> m_hadException = false;
        std::string m_exceptionMessage;

        struct TaskInterruptor { virtual ~TaskInterruptor() = default; };

        friend class TaskHolder;
        friend class TaskManager;
    };

    class TaskHolder {
    public:
        TaskHolder() = default;
        explicit TaskHolder(std::weak_ptr<Task> task) : m_task(std::move(task)) { }

        [[nodiscard]] bool isRunning() const;
        [[nodiscard]] bool hadException() const;
        [[nodiscard]] bool wasInterrupted() const;

        void interrupt();
    private:
        std::weak_ptr<Task> m_task;
    };

    class TaskManager {
    public:
        TaskManager() = delete;

        static void init();
        static void exit();

        constexpr static auto NoProgress = 0;

        static TaskHolder createTask(std::string name, u64 maxValue, std::function<void(Task &)> function);
        static TaskHolder createBackgroundTask(std::string name, std::function<void(Task &)> function);

        static void collectGarbage();

        static size_t getRunningTaskCount();
        static std::list<std::shared_ptr<Task>> &getRunningTasks();

        static void doLater(const std::function<void()> &function);
        static void runDeferredCalls();
    private:
        static std::mutex s_deferredCallsMutex;

        static std::list<std::shared_ptr<Task>> s_tasks;
        static std::list<std::shared_ptr<Task>> s_taskQueue;
        static std::list<std::function<void()>> s_deferredCalls;

        static std::mutex s_queueMutex;
        static std::condition_variable s_jobCondVar;
        static std::vector<std::jthread> s_workers;

        static void runner(const std::stop_token &stopToken);
    };

}