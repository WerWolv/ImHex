#include <hex/api/task.hpp>

#include <hex/helpers/shared_data.hpp>

namespace hex {

    Task::Task(const std::string &unlocalizedName, u64 maxValue) : m_name(LangEntry(unlocalizedName)), m_maxValue(maxValue), m_currValue(0) {
        SharedData::runningTasks.push_back(this);
    }

    Task::~Task() {
        this->finish();
    }

    void Task::finish() {
        SharedData::runningTasks.remove(this);
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

}