#pragma once

#include <functional>
#include <string>
#include <vector>

namespace hex::init {

    struct Task {
        std::string name;
        std::function<bool()> function;
    };

    struct Argument {
        std::string name, value;
    };

    std::vector<Task> getInitTasks();
    std::vector<Task> getExitTasks();

    std::vector<Argument>& getInitArguments();
}