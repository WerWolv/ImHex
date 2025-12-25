#include <hex/api/task_manager.hpp>

#include <hex/api/localization_manager.hpp>
#include <hex/helpers/logger.hpp>

#include <algorithm>
#include <ranges>

#include <jthread.hpp>
#include <hex/helpers/debugging.hpp>
#include <hex/trace/exceptions.hpp>
#include <utility>

#if defined(OS_WINDOWS)
    #include <windows.h>
    #include <processthreadsapi.h>
    #include <hex/helpers/utils.hpp>
#else
    #include <pthread.h>
#endif

namespace {

    struct SourceLocationWrapper {
        std::source_location location;

        bool operator==(const SourceLocationWrapper &other) const {
            return location.file_name() == other.location.file_name() &&
                   location.function_name() == other.location.function_name() &&
                   location.column() == other.location.column() &&
                   location.line() == other.location.line();
        }
    };

}

template<>
struct std::hash<SourceLocationWrapper> {
    std::size_t operator()(const SourceLocationWrapper& s) const noexcept {
        auto h1 = std::hash<std::string>{}(s.location.file_name());
        auto h2 = std::hash<std::string>{}(s.location.function_name());
        auto h3 = std::hash<u32>{}(s.location.column());
        auto h4 = std::hash<u32>{}(s.location.line());
        return (h1 << 0) ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3);
    }
};

namespace hex {

    namespace {

        std::recursive_mutex s_deferredCallsMutex, s_tasksFinishedMutex;

        std::list<std::shared_ptr<Task>> s_tasks, s_taskQueue;
        std::list<std::function<void()>> s_deferredCalls;
        std::unordered_map<SourceLocationWrapper, std::function<void()>> s_onceDeferredCalls;
        std::list<std::function<void()>> s_tasksFinishedCallbacks;
        std::list<std::function<void(Task&)>> s_taskCompletionCallbacks;

        std::mutex s_queueMutex;
        std::condition_variable s_jobCondVar;
        std::vector<std::jthread> s_workers;

        thread_local std::array<char, 256> s_currentThreadName;
        thread_local Task* s_currentTask = nullptr;
        std::thread::id s_mainThreadId;

    }


    Task::Task(UnlocalizedString unlocalizedName, u64 maxValue, bool background, bool blocking, std::function<void(Task &)> function)
    : m_unlocalizedName(std::move(unlocalizedName)),
      m_maxValue(maxValue),
      m_function(std::move(function)),
      m_background(background), m_blocking(blocking) { }

    Task::Task(hex::Task &&other) noexcept {
        {
            std::scoped_lock thisLock(m_mutex);
            std::scoped_lock otherLock(other.m_mutex);

            m_function = std::move(other.m_function);
            m_unlocalizedName = std::move(other.m_unlocalizedName);
        }

        m_maxValue    = u64(other.m_maxValue);
        m_currValue   = u64(other.m_currValue);

        if (other.m_finished.test())
            m_finished.test_and_set();
        if (other.m_hadException.test())
            m_hadException.test_and_set();
        if (other.m_interrupted.test())
            m_interrupted.test_and_set();

        m_finished.notify_all();
        m_hadException.notify_all();
        m_interrupted.notify_all();

        m_shouldInterrupt = bool(other.m_shouldInterrupt);
    }

    Task::~Task() {
        if (!this->isFinished())
            this->interrupt();
    }

    void Task::update(u64 value) {
        // Update the current progress value of the task
        m_currValue.store(value, std::memory_order_relaxed);

        // Check if the task has been interrupted by the main thread and if yes,
        // throw an exception that is generally not caught by the task
        if (m_shouldInterrupt.load(std::memory_order_relaxed)) [[unlikely]]
            throw TaskInterruptor();
    }

    void Task::update() const {
        if (m_shouldInterrupt.load(std::memory_order_relaxed)) [[unlikely]]
            throw TaskInterruptor();
    }

    void Task::increment() {
        m_currValue.fetch_add(1, std::memory_order_relaxed);

        if (m_shouldInterrupt.load(std::memory_order_relaxed)) [[unlikely]]
            throw TaskInterruptor();
    }


    void Task::setMaxValue(u64 value) {
        m_maxValue = value;
    }


    void Task::interrupt() {
        m_shouldInterrupt = true;

        // Call the interrupt callback on the current thread if one is set
        if (m_interruptCallback)
            m_interruptCallback();
    }

    void Task::setInterruptCallback(std::function<void()> callback) {
        m_interruptCallback = std::move(callback);
    }

    bool Task::isBackgroundTask() const {
        return m_background;
    }

    bool Task::isBlocking() const {
        return m_blocking;
    }


    bool Task::isFinished() const {
        return m_finished.test();
    }

    bool Task::hadException() const {
        return m_hadException.test();
    }

    bool Task::shouldInterrupt() const {
        return m_shouldInterrupt;
    }

    bool Task::wasInterrupted() const {
        return m_interrupted.test();
    }

    void Task::clearException() {
        m_hadException.clear();
    }

    std::string Task::getExceptionMessage() const {
        std::scoped_lock lock(m_mutex);

        return m_exceptionMessage;
    }

    const UnlocalizedString &Task::getUnlocalizedName() {
        return m_unlocalizedName;
    }

    u64 Task::getValue() const {
        return m_currValue;
    }

    u64 Task::getMaxValue() const {
        return m_maxValue;
    }

    void Task::wait() const {
        m_finished.wait(false);
    }

    void Task::finish() {
        m_finished.test_and_set();
        m_finished.notify_all();
    }

    void Task::interruption() {
        m_interrupted.test_and_set();
        m_interrupted.notify_all();
    }

    void Task::exception(const char *message) {
        std::scoped_lock lock(m_mutex);

        // Store information about the caught exception
        m_exceptionMessage = message;
        m_hadException.test_and_set();
        m_hadException.notify_all();

        // Call the interrupt callback on the current thread if one is set
        if (m_interruptCallback)
            m_interruptCallback();
    }


    bool TaskHolder::isRunning() const {
        const auto &task = m_task.lock();
        if (!task)
            return false;

        return !task->isFinished();
    }

    bool TaskHolder::hadException() const {
        const auto &task = m_task.lock();
        if (!task)
            return false;

        return task->hadException();
    }

    bool TaskHolder::shouldInterrupt() const {
        const auto &task = m_task.lock();
        if (!task)
            return false;

        return task->shouldInterrupt();
    }

    bool TaskHolder::wasInterrupted() const {
        const auto &task = m_task.lock();
        if (!task)
            return false;

        return task->wasInterrupted();
    }

    void TaskHolder::interrupt() const {
        const auto &task = m_task.lock();
        if (!task)
            return;

        task->interrupt();
    }

    void TaskHolder::wait() const {
        const auto &task = m_task.lock();
        if (!task)
            return;

        task->wait();
    }

    u32 TaskHolder::getProgress() const {
        const auto &task = m_task.lock();
        if (!task)
            return 0;

        // If the max value is 0, the task has no progress
        if (task->getMaxValue() == 0)
            return 0;

        // Calculate the progress of the task from 0 to 100
        return u32((task->getValue() * 100) / task->getMaxValue());
    }

    void TaskManager::init() {
        const auto threadCount = std::thread::hardware_concurrency();

        log::debug("Initializing task manager thread pool with {} workers.", threadCount);

        // Create worker threads
        for (u32 i = 0; i < threadCount; i++) {
            s_workers.emplace_back([](const std::stop_token &stopToken) {
                while (true) {
                    std::shared_ptr<Task> task;

                    // Set the thread name to "Idle Task" while waiting for a task
                    TaskManager::setCurrentThreadName("Idle Task");

                    {
                        // Wait for a task to be added to the queue
                        std::unique_lock lock(s_queueMutex);
                        s_jobCondVar.wait(lock, [&] {
                            return !s_taskQueue.empty() || stopToken.stop_requested();
                        });

                        // Check if the thread should exit
                        if (stopToken.stop_requested())
                            break;

                        // Grab the next task from the queue
                        task = std::move(s_taskQueue.front());
                        s_taskQueue.pop_front();

                        s_currentTask = task.get();
                    }

                    try {
                        trace::enableExceptionCaptureForCurrentThread();

                        // Set the thread name to the name of the task
                        TaskManager::setCurrentThreadName(Lang(task->m_unlocalizedName));

                        // Execute the task
                        task->m_function(*task);

                        log::debug("Task '{}' finished", task->m_unlocalizedName.get());

                        {
                            std::scoped_lock lock(s_tasksFinishedMutex);

                            for (const auto &callback : s_taskCompletionCallbacks)
                                callback(*task);
                        }
                    } catch (const Task::TaskInterruptor &) {
                        // Handle the task being interrupted by user request
                        task->interruption();
                    } catch (const std::exception &e) {
                        log::error("Exception in task '{}': {}", task->m_unlocalizedName.get(), e.what());

                        dbg::printStackTrace(trace::getStackTrace());

                        // Handle the task throwing an uncaught exception
                        task->exception(e.what());
                    } catch (...) {
                        log::error("Exception in task '{}'", task->m_unlocalizedName.get());

                        dbg::printStackTrace(trace::getStackTrace());

                        // Handle the task throwing an uncaught exception of unknown type
                        task->exception("Unknown Exception");
                    }

                    trace::disableExceptionCaptureForCurrentThread();

                    s_currentTask = nullptr;
                    task->finish();
                }
            });
        }
    }

    void TaskManager::exit() {
        // Interrupt all tasks
        for (const auto &task : s_tasks) {
            task->interrupt();
        }

        // Ask worker threads to exit after finishing their task
        for (auto &thread : s_workers)
            thread.request_stop();

        // Wake up all the idle worker threads so they can exit
        s_jobCondVar.notify_all();

        // Wait for all worker threads to exit
        s_workers.clear();

        s_tasks.clear();
        s_taskQueue.clear();

        s_deferredCalls.clear();
        s_onceDeferredCalls.clear();
        s_tasksFinishedCallbacks.clear();
        s_taskCompletionCallbacks.clear();
    }

    TaskHolder TaskManager::createTask(const UnlocalizedString &unlocalizedName, u64 maxValue, bool background, bool blocking, std::function<void(Task&)> function) {
        std::scoped_lock lock(s_queueMutex);

        // Construct new task
        auto task = std::make_shared<Task>(unlocalizedName, maxValue, background, blocking, std::move(function));

        s_tasks.emplace_back(task);

        // Add task to the queue for the worker to pick up
        s_taskQueue.emplace_back(std::move(task));

        s_jobCondVar.notify_one();

        return TaskHolder(s_tasks.back());
    }


    TaskHolder TaskManager::createTask(const UnlocalizedString &unlocalizedName, u64 maxValue, std::function<void(Task &)> function) {
        log::debug("Creating task {}", unlocalizedName.get());
        return createTask(unlocalizedName, maxValue, false, false, std::move(function));
    }

    TaskHolder TaskManager::createTask(const UnlocalizedString &unlocalizedName, u64 maxValue, std::function<void()> function) {
        log::debug("Creating task {}", unlocalizedName.get());
        return createTask(unlocalizedName, maxValue, false, false,
            [function = std::move(function)](Task&) {
                function();
            }
        );
    }

    TaskHolder TaskManager::createBackgroundTask(const UnlocalizedString &unlocalizedName, std::function<void(Task &)> function) {
        log::debug("Creating background task {}", unlocalizedName.get());
        return createTask(unlocalizedName, 0, true, false, std::move(function));
    }

    TaskHolder TaskManager::createBackgroundTask(const UnlocalizedString &unlocalizedName, std::function<void()> function) {
        log::debug("Creating background task {}", unlocalizedName.get());
        return createTask(unlocalizedName, 0, true, false,
            [function = std::move(function)](Task&) {
                function();
            }
        );
    }

    TaskHolder TaskManager::createBlockingTask(const UnlocalizedString &unlocalizedName, u64 maxValue, std::function<void(Task &)> function) {
        log::debug("Creating blocking task {}", unlocalizedName.get());
        return createTask(unlocalizedName, maxValue, true, true, std::move(function));
    }

    TaskHolder TaskManager::createBlockingTask(const UnlocalizedString &unlocalizedName, u64 maxValue, std::function<void()> function) {
        log::debug("Creating blocking task {}", unlocalizedName.get());
        return createTask(unlocalizedName, maxValue, true, true,
            [function = std::move(function)](Task&) {
                function();
            }
        );
    }

    void TaskManager::collectGarbage() {
        {
            std::scoped_lock lock(s_queueMutex);
            std::erase_if(s_tasks, [](const auto &task) {
                return task->isFinished() && !task->hadException();
            });
        }

        if (s_tasks.empty()) {
            std::scoped_lock lock(s_deferredCallsMutex);
            for (const auto &call : s_tasksFinishedCallbacks)
                call();
            s_tasksFinishedCallbacks.clear();
        }

    }

    Task& TaskManager::getCurrentTask() {
        return *s_currentTask;
    }


    const std::list<std::shared_ptr<Task>>& TaskManager::getRunningTasks() {
        return s_tasks;
    }

    size_t TaskManager::getRunningTaskCount() {
        std::scoped_lock lock(s_queueMutex);

        return std::ranges::count_if(s_tasks, [](const auto &task){
            return !task->isBackgroundTask();
        });
    }

    size_t TaskManager::getRunningBackgroundTaskCount() {
        std::scoped_lock lock(s_queueMutex);

        return std::ranges::count_if(s_tasks, [](const auto &task){
            return task->isBackgroundTask();
        });
    }

    size_t TaskManager::getRunningBlockingTaskCount() {
        std::scoped_lock lock(s_queueMutex);

        return std::ranges::count_if(s_tasks, [](const auto &task){
            return task->isBlocking();
        });
    }


    void TaskManager::doLater(const std::function<void()> &function) {
        std::scoped_lock lock(s_deferredCallsMutex);

        s_deferredCalls.push_back(function);
    }

    void TaskManager::doLaterOnce(const std::function<void()> &function, std::source_location location) {
        std::scoped_lock lock(s_deferredCallsMutex);

        s_onceDeferredCalls[SourceLocationWrapper{ location }] = function;
    }

    void TaskManager::runDeferredCalls() {
        std::scoped_lock lock(s_deferredCallsMutex);

        while (!s_deferredCalls.empty()) {
            auto callback = s_deferredCalls.front();
            s_deferredCalls.pop_front();
            callback();
        }
        while (!s_onceDeferredCalls.empty()) {
            auto node = s_onceDeferredCalls.extract(s_onceDeferredCalls.begin());
            node.mapped()();
        }
    }

    void TaskManager::runWhenTasksFinished(const std::function<void()> &function) {
        std::scoped_lock lock(s_tasksFinishedMutex);

        for (const auto &task : s_tasks) {
            task->interrupt();
        }

        s_tasksFinishedCallbacks.push_back(function);
    }

    void TaskManager::setCurrentThreadName(const std::string &name) {
        std::ranges::fill(s_currentThreadName, '\0');
        std::ranges::copy(name | std::views::take(255), s_currentThreadName.begin());

        #if defined(OS_WINDOWS)
            using SetThreadDescriptionFunc =  HRESULT(WINAPI*)(HANDLE hThread, PCWSTR lpThreadDescription);

            static auto setThreadDescription = reinterpret_cast<SetThreadDescriptionFunc>(
                reinterpret_cast<uintptr_t>(
                    ::GetProcAddress(
                        ::GetModuleHandleW(L"Kernel32.dll"),
                        "SetThreadDescription"
                    )
                )
            );

            if (setThreadDescription != nullptr) {
                const auto longName = hex::utf8ToUtf16(name);
                setThreadDescription(::GetCurrentThread(), longName.c_str());
            } else {
                struct THREADNAME_INFO {
                    DWORD dwType;
                    LPCSTR szName;
                    DWORD dwThreadID;
                    DWORD dwFlags;
                };

                THREADNAME_INFO info = { };
                info.dwType = 0x1000;
                info.szName = name.c_str();
                info.dwThreadID = ::GetCurrentThreadId();
                info.dwFlags = 0;

                constexpr static DWORD MS_VC_EXCEPTION = 0x406D1388;
                RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), reinterpret_cast<ULONG_PTR*>(&info));
            }
        #elif defined(OS_LINUX)
                pthread_setname_np(pthread_self(), name.c_str());
        #elif defined(OS_WEB)
                std::ignore = name;
        #elif defined(OS_MACOS)
                pthread_setname_np(name.c_str());
        #endif
    }

    std::string_view TaskManager::getCurrentThreadName() {
        if (TaskManager::isMainThread())
            return "Main";
        else
            return s_currentThreadName.data();
    }

    void TaskManager::setMainThreadId(std::thread::id threadId) {
        s_mainThreadId = threadId;
    }

    bool TaskManager::isMainThread() {
        return s_mainThreadId == std::this_thread::get_id();
    }

    void TaskManager::addTaskCompletionCallback(const std::function<void(Task &)> &function) {
        std::scoped_lock lock(s_tasksFinishedMutex);

        for (const auto &task : s_tasks) {
            task->interrupt();
        }

        s_taskCompletionCallbacks.push_back(function);
    }
}
