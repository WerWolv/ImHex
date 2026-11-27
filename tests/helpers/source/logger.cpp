#include <hex/helpers/logger.hpp>
#include <hex/test/tests.hpp>

#include <algorithm>
#include <array>
#include <barrier>
#include <thread>

// Exercise the append path used by debug() when debug printing is disabled (#2751).
TEST_SEQUENCE("ConcurrentLogEntryWrites") {
    using namespace hex::log::impl;

    constexpr int WriterCount = 16;
    constexpr int EntriesPerWriter = 15000;
    constexpr std::string_view Project = "ConcurrentLogEntryWrites";
    std::barrier start(WriterCount);
    std::array<std::thread, WriterCount> writers;

    for (auto &writer : writers) {
        writer = std::thread([&] {
            start.arrive_and_wait();
            for (int i = 0; i < EntriesPerWriter; i++)
                addLogEntry(Project, "[DEBUG]", "Test entry");
        });
    }

    for (auto &writer : writers)
        writer.join();

    lockLoggerMutex();
    ON_SCOPE_EXIT { unlockLoggerMutex(); };
    const auto count = std::ranges::count(getLogEntries(), Project, &LogEntry::project);
    TEST_ASSERT(count == WriterCount * EntriesPerWriter, "Found {} log entries", count);
    TEST_SUCCESS();
};
