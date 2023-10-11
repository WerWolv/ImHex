#pragma once

#include <functional>
#include <string>
#include <vector>

namespace hex::init {

    struct Task {
        std::string name;
        std::function<bool()> function;
        bool async;
    };

    /**
     * @brief Runs the exit tasks and print them to console
     */
    void runExitTasks();

    std::vector<Task> getInitTasks();
    std::vector<Task> getExitTasks();
}