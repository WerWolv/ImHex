#pragma once

#include <functional>
#include <string>
#include <vector>

namespace hex::init {

    struct Task {
        std::string name;
        std::function<bool()> function;
    };

    std::vector<Task> getInitTasks();
    std::vector<Task> getExitTasks();
}