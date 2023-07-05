#pragma once

#if defined(OS_LINUX)

namespace hex {
    void executeCmd(const std::vector<std::string> &argsVector);
}

#endif
