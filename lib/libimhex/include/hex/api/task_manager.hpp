#pragma once

#include <hex.hpp>
#include <hex/api/localization_manager.hpp>

#include <cstdio>
#include <functional>
#include <mutex>
#include <memory>
#include <list>
#include <condition_variable>
#include <source_location>

namespace hex {

    class TaskHolder;
    class TaskManager;

    /**
     * @brief A type representing a running asynchronous task
     */
    class Task {
    public:
        Task() = default;
        Task(const UnlocalizedString &unlocalizedName, u64 maxValue, bool background, std::function<void(Task &)> function);

        Task(const Task&) = delete;
        Task(Task &&other) noexcept;
        ~Task();

        /**
         * @brief Updates the current process value of the task
         * @param value Current value
         */
        void update(u64 value);
        void update() const;
        void increment();

        /**
         * @brief Sets the maximum value of the task
         * @param value Maximum value of the task
         */
        void setMaxValue(u64 value);


        /**
         * @brief Interrupts the task
         * For regular Tasks, this just throws an exception to stop the task.
         * If a custom interrupt callback is set, an exception is thrown and the callback is called.
         */
        void interrupt();

        /**
         * @brief Sets a callback that is called when the task is interrupted
         * @param callback Callback to be called
         */
        void setInterruptCallback(std::function<void()> callback);

        [[nodiscard]] bool isBackgroundTask() const;
        [[nodiscard]] bool isFinished() const;
        [[nodiscard]] bool hadException() const;
        [[nodiscard]] bool wasInterrupted() const;
        [[nodiscard]] bool shouldInterrupt() const;

        void clearException();
        [[nodiscard]] std::string getExceptionMessage() const;

        [[nodiscard]] const UnlocalizedString &getUnlocalizedName();
        [[nodiscard]] u64 getValue() const;
        [[nodiscard]] u64 getMaxValue() const;

    private:
        void finish();
        void interruption();
        void exception(const char *message);

    private:
        mutable std::mutex m_mutex;

        UnlocalizedString m_unlocalizedName;
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

    /**
     * @brief A type holding a weak reference to a Task
     */
    class TaskHolder {
    public:
        TaskHolder() = default;
        explicit TaskHolder(std::weak_ptr<Task> task) : m_task(std::move(task)) { }

        [[nodiscard]] bool isRunning() const;
        [[nodiscard]] bool hadException() const;
        [[nodiscard]] bool wasInterrupted() const;
        [[nodiscard]] bool shouldInterrupt() const;

        [[nodiscard]] u32 getProgress() const;

        void interrupt() const;
    private:
        std::weak_ptr<Task> m_task;
    };

    /**
     * @brief The Task Manager is responsible for running and managing asynchronous tasks
     */
    class TaskManager {
    public:
        TaskManager() = delete;

        static void init();
        static void exit();

        constexpr static auto NoProgress = 0;

        /**
         * @brief Creates a new asynchronous task that gets displayed in the Task Manager in the footer
         * @param unlocalizedName Name of the task
         * @param maxValue Maximum value of the task
         * @param function Function to be executed
         * @return A TaskHolder holding a weak reference to the task
         */
        static TaskHolder createTask(const UnlocalizedString &unlocalizedName, u64 maxValue, std::function<void(Task &)> function);

        /**
        * @brief Creates a new asynchronous task that gets displayed in the Task Manager in the footer
        * @param unlocalizedName Name of the task
        * @param maxValue Maximum value of the task
        * @param function Function to be executed
        * @return A TaskHolder holding a weak reference to the task
        */
        static TaskHolder createTask(const UnlocalizedString &unlocalizedName, u64 maxValue, std::function<void()> function);

        /**
         * @brief Creates a new asynchronous task that does not get displayed in the Task Manager
         * @param unlocalizedName Name of the task
         * @param function Function to be executed
         * @return A TaskHolder holding a weak reference to the task
         */
        static TaskHolder createBackgroundTask(const UnlocalizedString &unlocalizedName, std::function<void(Task &)> function);

        /**
         * @brief Creates a new asynchronous task that does not get displayed in the Task Manager
         * @param unlocalizedName Name of the task
         * @param function Function to be executed
         * @return A TaskHolder holding a weak reference to the task
         */
        static TaskHolder createBackgroundTask(const UnlocalizedString &unlocalizedName, std::function<void()> function);

        /**
         * @brief Creates a new synchronous task that will execute the given function at the start of the next frame
         * @param function Function to be executed
         */
        static void doLater(const std::function<void()> &function);

        /**
         * @brief Creates a new synchronous task that will execute the given function at the start of the next frame
         * @param function Function to be executed
         * @param location Source location of the function call. This is used to make sure repeated calls to the function at the same location are only executed once
         */
        static void doLaterOnce(const std::function<void()> &function, std::source_location location = std::source_location::current());

        /**
         * @brief Creates a callback that will be executed when all tasks are finished
         * @param function Function to be executed
         */
        static void runWhenTasksFinished(const std::function<void()> &function);

        /**
         * @brief Sets the name of the current thread
         * @param name Name of the thread
         */
        static void setCurrentThreadName(const std::string &name);

        /**
         * @brief Gets the name of the current thread
         * @return Name of the thread
         */
        static std::string getCurrentThreadName();

        /**
         * @brief Cleans up finished tasks
         */
        static void collectGarbage();

        static Task& getCurrentTask();

        static size_t getRunningTaskCount();
        static size_t getRunningBackgroundTaskCount();

        static const std::list<std::shared_ptr<Task>>& getRunningTasks();
        static void runDeferredCalls();

    private:
        static TaskHolder createTask(const UnlocalizedString &unlocalizedName, u64 maxValue, bool background, std::function<void(Task &)> function);
    };

}