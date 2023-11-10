#if defined(OS_LINUX)

#include <hex/helpers/logger.hpp>

#include <vector>
#include <string>
#include <unistd.h>

namespace hex {

    void executeCmd(const std::vector<std::string> &argsVector) {
        std::vector<char*> cArgsVector;
        for (const auto &str : argsVector) {
            cArgsVector.push_back(const_cast<char*>(str.c_str()));
        }
        cArgsVector.push_back(nullptr);
        
        if (fork() == 0) {
            execvp(cArgsVector[0], &cArgsVector[0]);
            log::error("execvp() failed: {}", strerror(errno));
            exit(EXIT_FAILURE);
        }
    }

}

#endif
