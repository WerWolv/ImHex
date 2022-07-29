#include <hex/api/task.hpp>

#include <hex/api/localization.hpp>

#include <algorithm>

namespace hex {

    std::list<Task *> Task::s_runningTasks;
    std::mutex Task::s_taskMutex;

    Task::Task(const std::string &unlocalizedName, u64 maxValue) : m_name(LangEntry(unlocalizedName)), m_maxValue(maxValue), m_currValue(0) {
        std::scoped_lock lock(Task::s_taskMutex);

        Task::s_runningTasks.push_back(this);
    }

    Task::~Task() {
        this->finish();
    }

    Task::Task(hex::Task &&other) noexcept {
        std::scoped_lock lock(Task::s_taskMutex);

        this->m_name = other.m_name;
        this->m_maxValue = other.m_maxValue;
        this->m_currValue = other.m_currValue;

        auto it = std::find(Task::s_runningTasks.begin(), Task::s_runningTasks.end(), &other);
        if (it != Task::s_runningTasks.end()) {
            *it = this;
        }
    }

    void Task::finish() {
        std::scoped_lock lock(Task::s_taskMutex);

        Task::s_runningTasks.remove(this);
    }

    void Task::setMaxValue(u64 maxValue) {
        this->m_maxValue = maxValue;
    }

    void Task::update(u64 currValue) {
        if (this->m_currValue < this->m_maxValue)
            this->m_currValue = currValue;
    }

    double Task::getProgress() const {
        if (this->m_maxValue == 0)
            return 100;

        return static_cast<double>(this->m_currValue) / static_cast<double>(this->m_maxValue);
    }

    bool Task::isPending() const {
        return this->m_maxValue == 0;
    }

    const std::string &Task::getName() const {
        return this->m_name;
    }

    size_t Task::getRunningTaskCount() {
        std::scoped_lock lock(Task::s_taskMutex);

        return Task::s_runningTasks.size();
    }

}