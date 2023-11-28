#pragma once

#include <vector>

#include <init/splash_window.hpp>

namespace hex::init {

    /**
     * @brief Runs the exit tasks and print them to console
     */
    void runExitTasks();

    std::vector<Task> getInitTasks();
    std::vector<Task> getExitTasks();
}