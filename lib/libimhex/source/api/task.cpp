#include <hex/api/task.hpp>

#include <hex/api/localization.hpp>
#include <hex/helpers/logger.hpp>

#include <algorithm>

namespace hex {

    std::mutex TaskManager::s_deferredCallsMutex;

    std::list<std::shared_ptr<Task>> TaskManager::s_tasks;
    std::list<std::function<void()>> TaskManager::s_deferredCalls;

    Task::Task(std::string unlocalizedName, u64 maxValue, std::function<void(Task &)> function)
    : m_unlocalizedName(std::move(unlocalizedName)), m_currValue(0), m_maxValue(maxValue) {
        this->m_thread = std::thread([this, func = std::move(function)] {
            try {
                func(*this);
            } catch (const TaskInterruptor &) {
                this->interruption();
            } catch (const std::exception &e) {
                log::error("Exception in task {}: {}", this->m_unlocalizedName, e.what());
                this->exception(e.what());
            } catch (...) {
                log::error("Exception in task {}", this->m_unlocalizedName);
                this->exception("Unknown Exception");
            }

            this->finish();
        });
    }

    Task::Task(hex::Task &&other) noexcept {
        std::scoped_lock thisLock(this->m_mutex);
        std::scoped_lock otherLock(other.m_mutex);

        this->m_thread = std::move(other.m_thread);
        this->m_unlocalizedName = std::move(other.m_unlocalizedName);

        this->m_maxValue = other.m_maxValue;
        this->m_currValue = other.m_currValue;

        this->m_finished = other.m_finished;
        this->m_hadException = other.m_hadException;
        this->m_interrupted = other.m_interrupted;
        this->m_shouldInterrupt = other.m_shouldInterrupt;
    }

    Task::~Task() {
        this->interrupt();
        this->m_thread.join();
    }

    void Task::update(u64 value) {
        std::scoped_lock lock(this->m_mutex);

        this->m_currValue = value;

        if (this->m_shouldInterrupt)
            throw TaskInterruptor();
    }

    void Task::setMaxValue(u64 value) {
        std::scoped_lock lock(this->m_mutex);

        this->m_maxValue = value;
    }


    void Task::interrupt() {
        std::scoped_lock lock(this->m_mutex);

        this->m_shouldInterrupt = true;
    }

    bool Task::isFinished() const {
        std::scoped_lock lock(this->m_mutex);

        return this->m_finished;
    }

    bool Task::hadException() const {
        std::scoped_lock lock(this->m_mutex);

        return this->m_hadException;
    }

    bool Task::wasInterrupted() const {
        std::scoped_lock lock(this->m_mutex);

        return this->m_interrupted;
    }

    void Task::clearException() {
        std::scoped_lock lock(this->m_mutex);

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
        std::scoped_lock lock(this->m_mutex);

        this->m_finished = true;
    }

    void Task::interruption() {
        std::scoped_lock lock(this->m_mutex);

        this->m_interrupted = true;
    }

    void Task::exception(const char *message) {
        std::scoped_lock lock(this->m_mutex);

        this->m_exceptionMessage = message;
        this->m_hadException = true;
    }


    bool TaskHolder::isRunning() const {
        return !m_task.expired() && !m_task.lock()->isFinished();
    }

    bool TaskHolder::hadException() const {
        return m_task.expired() || m_task.lock()->hadException();
    }

    bool TaskHolder::wasInterrupted() const {
        return m_task.expired() || m_task.lock()->wasInterrupted();
    }

    void TaskHolder::interrupt() {
        if (!this->m_task.expired())
            this->m_task.lock()->interrupt();
    }


    TaskHolder TaskManager::createTask(std::string name, u64 maxValue, std::function<void(Task &)> function) {
        s_tasks.emplace_back(std::make_shared<Task>(std::move(name), maxValue, std::move(function)));

        return TaskHolder(s_tasks.back());
    }

    void TaskManager::collectGarbage() {
        std::erase_if(s_tasks, [](const auto &task) { return task->isFinished() && !task->hadException(); });
    }

    std::list<std::shared_ptr<Task>> &TaskManager::getRunningTasks() {
        return s_tasks;
    }

    size_t TaskManager::getRunningTaskCount() {
        return s_tasks.size();
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

}