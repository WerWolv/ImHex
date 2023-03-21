#include <hex/api/task.hpp>

#include <hex/api/localization.hpp>
#include <hex/helpers/logger.hpp>

#include <algorithm>
#include <ranges>

namespace hex {

    std::mutex TaskManager::s_deferredCallsMutex, TaskManager::s_tasksFinishedMutex;

    std::list<std::shared_ptr<Task>> TaskManager::s_tasks, TaskManager::s_taskQueue;
    std::list<std::function<void()>> TaskManager::s_deferredCalls;
    std::list<std::function<void()>> TaskManager::s_tasksFinishedCallbacks;

    std::mutex TaskManager::s_queueMutex;
    std::condition_variable TaskManager::s_jobCondVar;
    std::vector<std::jthread> TaskManager::s_workers;

    Task::Task(std::string unlocalizedName, u64 maxValue, bool background, std::function<void(Task &)> function)
    : m_unlocalizedName(std::move(unlocalizedName)), m_currValue(0), m_maxValue(maxValue), m_function(std::move(function)), m_background(background) { }

    Task::Task(hex::Task &&other) noexcept {
        {
            std::scoped_lock thisLock(this->m_mutex);
            std::scoped_lock otherLock(other.m_mutex);

            this->m_function = std::move(other.m_function);
            this->m_unlocalizedName = std::move(other.m_unlocalizedName);
        }

        this->m_maxValue    = u64(other.m_maxValue);
        this->m_currValue   = u64(other.m_currValue);

        this->m_finished        = bool(other.m_finished);
        this->m_hadException    = bool(other.m_hadException);
        this->m_interrupted     = bool(other.m_interrupted);
        this->m_shouldInterrupt = bool(other.m_shouldInterrupt);
    }

    Task::~Task() {
        if (!this->isFinished())
            this->interrupt();
    }

    void Task::update(u64 value) {
        this->m_currValue = value;

        if (this->m_shouldInterrupt) [[unlikely]]
            throw TaskInterruptor();
    }

    void Task::setMaxValue(u64 value) {
        this->m_maxValue = value;
    }


    void Task::interrupt() {
        this->m_shouldInterrupt = true;

        if (this->m_interruptCallback)
            this->m_interruptCallback();
    }

    void Task::setInterruptCallback(std::function<void()> callback) {
        this->m_interruptCallback = std::move(callback);
    }

    bool Task::isBackgroundTask() const {
        return this->m_background;
    }

    bool Task::isFinished() const {
        return this->m_finished;
    }

    bool Task::hadException() const {
        return this->m_hadException;
    }

    bool Task::shouldInterrupt() const {
        return this->m_shouldInterrupt;
    }

    bool Task::wasInterrupted() const {
        return this->m_interrupted;
    }

    void Task::clearException() {
        this->m_hadException = false;
    }

    std::string Task::getExceptionMessage() const {
        std::scoped_lock lock(this->m_mutex);

        return this->m_exceptionMessage;
    }

    const std::string &Task::getUnlocalizedName() {
        return this->m_unlocalizedName;
    }

    u64 Task::getValue() const {
        return this->m_currValue;
    }

    u64 Task::getMaxValue() const {
        return this->m_maxValue;
    }

    void Task::finish() {
        this->m_finished = true;
    }

    void Task::interruption() {
        this->m_interrupted = true;
    }

    void Task::exception(const char *message) {
        std::scoped_lock lock(this->m_mutex);

        this->m_exceptionMessage = message;
        this->m_hadException = true;
    }


    bool TaskHolder::isRunning() const {
        if (this->m_task.expired())
            return false;

        auto task = this->m_task.lock();
        return !task->isFinished();
    }

    bool TaskHolder::hadException() const {
        if (this->m_task.expired())
            return false;

        auto task = this->m_task.lock();
        return !task->hadException();
    }

    bool TaskHolder::shouldInterrupt() const {
        if (this->m_task.expired())
            return false;

        auto task = this->m_task.lock();
        return !task->shouldInterrupt();
    }

    bool TaskHolder::wasInterrupted() const {
        if (this->m_task.expired())
            return false;

        auto task = this->m_task.lock();
        return !task->wasInterrupted();
    }

    void TaskHolder::interrupt() {
        if (this->m_task.expired())
            return;

        auto task = this->m_task.lock();
        task->interrupt();
    }


    void TaskManager::init() {
        for (u32 i = 0; i < std::thread::hardware_concurrency(); i++)
            TaskManager::s_workers.emplace_back(TaskManager::runner);
    }

    void TaskManager::exit() {
        for (auto &task : TaskManager::s_tasks)
            task->interrupt();

        for (auto &thread : TaskManager::s_workers)
            thread.request_stop();

        s_jobCondVar.notify_all();

        TaskManager::s_workers.clear();
    }

    void TaskManager::runner(const std::stop_token &stopToken) {
        std::mutex mutex;
        while (true) {
            std::shared_ptr<Task> task;
            {
                std::unique_lock lock(s_queueMutex);
                s_jobCondVar.wait(lock, [&] {
                    return !s_taskQueue.empty() || stopToken.stop_requested();
                });
                if (stopToken.stop_requested())
                    break;

                task = std::move(s_taskQueue.front());
                s_taskQueue.pop_front();
            }

            try {
                task->m_function(*task);
            } catch (const Task::TaskInterruptor &) {
                task->interruption();
            } catch (const std::exception &e) {
                log::error("Exception in task {}: {}", task->m_unlocalizedName, e.what());
                task->exception(e.what());
            } catch (...) {
                log::error("Exception in task {}", task->m_unlocalizedName);
                task->exception("Unknown Exception");
            }

            task->finish();
        }
    }

    TaskHolder TaskManager::createTask(std::string name, u64 maxValue, std::function<void(Task &)> function) {
        std::unique_lock lock(s_queueMutex);
        auto task = std::make_shared<Task>(std::move(name), maxValue, false, std::move(function));
        s_tasks.emplace_back(task);
        s_taskQueue.emplace_back(task);

        s_jobCondVar.notify_one();

        return TaskHolder(s_tasks.back());
    }

    TaskHolder TaskManager::createBackgroundTask(std::string name, std::function<void(Task &)> function) {
        std::unique_lock lock(s_queueMutex);

        auto task = std::make_shared<Task>(std::move(name), 0, true, std::move(function));
        s_tasks.emplace_back(task);
        s_taskQueue.emplace_back(task);

        s_jobCondVar.notify_one();

        return TaskHolder(s_tasks.back());
    }

    void TaskManager::collectGarbage() {
        std::unique_lock lock1(s_queueMutex);
        std::unique_lock lock2(s_deferredCallsMutex);

        std::erase_if(s_tasks, [](const auto &task) { return task->isFinished() && !task->hadException(); });

        if (s_tasks.empty()) {
            for (auto &call : s_tasksFinishedCallbacks)
                call();
            s_tasksFinishedCallbacks.clear();
        }
    }

    std::list<std::shared_ptr<Task>> &TaskManager::getRunningTasks() {
        return s_tasks;
    }

    size_t TaskManager::getRunningTaskCount() {
        std::unique_lock lock(s_queueMutex);

        return std::count_if(s_tasks.begin(), s_tasks.end(), [](const auto &task){
            return !task->isBackgroundTask();
        });
    }

    size_t TaskManager::getRunningBackgroundTaskCount() {
        std::unique_lock lock(s_queueMutex);

        return std::count_if(s_tasks.begin(), s_tasks.end(), [](const auto &task){
            return task->isBackgroundTask();
        });
    }


    void TaskManager::doLater(const std::function<void()> &function) {
        std::scoped_lock lock(s_deferredCallsMutex);

        s_deferredCalls.push_back(function);
    }

    void TaskManager::runDeferredCalls() {
        std::scoped_lock lock(s_deferredCallsMutex);

        for (const auto &call : s_deferredCalls)
            call();

        s_deferredCalls.clear();
    }

    void TaskManager::runWhenTasksFinished(const std::function<void()> &function) {
        std::scoped_lock lock(s_tasksFinishedMutex);

        s_tasksFinishedCallbacks.push_back(function);
    }

}