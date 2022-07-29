#pragma once

#include <hex.hpp>

#include <list>
#include <mutex>
#include <string>

namespace hex {

    class Task {
    public:
        Task() = default;
        Task(const std::string &unlocalizedName, u64 maxValue);
        ~Task();

        Task(Task &&other) noexcept;

        void setMaxValue(u64 maxValue);
        void update(u64 currValue);
        void finish();

        [[nodiscard]] double getProgress() const;

        [[nodiscard]] const std::string &getName() const;

        [[nodiscard]] bool isPending() const;

        static size_t getRunningTaskCount();
        static std::list<Task *> &getRunningTasks() { return Task::s_runningTasks; }
        static std::mutex &getTaskMutex() { return Task::s_taskMutex; }

    private:
        std::string m_name;
        u64 m_maxValue = 0, m_currValue = 0;

        static std::list<Task *> s_runningTasks;
        static std::mutex s_taskMutex;
    };

}