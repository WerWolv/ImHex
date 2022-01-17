#include <hex/helpers/logger.hpp>
#include <hex/helpers/file.hpp>
#include <hex/helpers/paths.hpp>
#include <hex/helpers/fmt.hpp>

#include <iostream>

namespace hex::log {

    static File g_loggerFile;

    FILE* getDestination() {
        if (g_loggerFile.isValid())
            return g_loggerFile.getHandle();
        else
            return stdout;
    }

    bool isRedirected() {
        return g_loggerFile.isValid();
    }

    void redirectToFile() {
        if (g_loggerFile.isValid()) return;

        for (const auto &path : hex::getPath(ImHexPath::Logs)) {
            g_loggerFile = File(path / hex::format("{0:%Y%m%d_%H%M%S}.log", fmt::localtime(std::chrono::system_clock::now())), File::Mode::Create);

            if (g_loggerFile.isValid()) break;
        }
    }

}