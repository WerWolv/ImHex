#pragma once

#include <hex/helpers/literals.hpp>

namespace hex {

auto searchInterruptable(const auto &haystackBegin, const auto &haystackEnd, const auto &needleBegin, const auto &needleEnd, auto predicate, Task &task) {
  if (needleBegin == needleEnd)
    return haystackBegin;

  for (auto current = haystackBegin; current != haystackEnd; ++current) {
    auto haystackIt = current;
    auto needleIt = needleBegin;

    while (haystackIt != haystackEnd && needleIt != needleEnd && predicate(*haystackIt, *needleIt)) {
      ++haystackIt;
      ++needleIt;
    }

    if (needleIt == needleEnd)
      return current;

    if (haystackIt == haystackEnd)
      return haystackEnd;

    using namespace hex::literals;
    // Important: Keep this aligned to a power of 2 so it gets optimized nicely
    if (const auto address = current.getAddress(); (address % 16_MiB) == 0) [[unlikely]] {
      task.update(address);
    }
  }

  return haystackEnd;
}

auto searchInterruptable(const auto &haystackBegin, const auto &haystackEnd, const auto &needleBegin, const auto &needleEnd, Task &task) {
  return searchInterruptable(haystackBegin, haystackEnd, needleBegin, needleEnd, [](auto left, auto right) {
    return left == right;
  }, task);
}

}