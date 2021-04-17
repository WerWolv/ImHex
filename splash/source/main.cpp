#include <hex.hpp>

#include "splash_window.hpp"

#include <chrono>

#include <unistd.h>

using namespace std::literals::chrono_literals;

constexpr auto ImHexPath = IMHEX_FILE_NAME;

int main(int argc, char **argv) {
    const char *filePath = nullptr;
    bool skipSplash = false;

    // Handle command line arguments
    {
        if (argc == 1) {
            // No optional arguments used, use default
        } else if (argc == 2) {
            if (argv[1] == std::string("--no-splash")) skipSplash = true;
            else filePath = argv[1];
        } else if (argc == 3) {
            filePath = argv[1];

            if (argv[2] == std::string("--no-splash")) skipSplash = true;
            else {
                printf("Invalid argument '%s'\n", argv[2]);
                return EXIT_FAILURE;
            }
        } else {
            printf("Usage: imhex [file_path] [--no-splash] ");
            return EXIT_FAILURE;
        }
    }


    if (!skipSplash) {
        hex::pre::WindowSplash window;

        /* Dummy task */
        window.addStartupTask([]{ std::this_thread::sleep_for(1s); return true; });

        window.loop();
    }

    // Launch ImHex
    if (filePath == nullptr)
        return execl(ImHexPath, ImHexPath, GIT_COMMIT_HASH, nullptr);
    else
        return execl(ImHexPath, ImHexPath, filePath, GIT_COMMIT_HASH, nullptr);
}