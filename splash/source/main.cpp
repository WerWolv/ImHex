#include <hex.hpp>

#include "splash_window.hpp"

#include <chrono>
#include <filesystem>
#include <vector>

#include <unistd.h>

#include <hex/helpers/net.hpp>
#include <hex/helpers/utils.hpp>

using namespace std::literals::chrono_literals;

constexpr auto ImHexPath = IMHEX_FILE_NAME;

int main(int argc, char **argv) {
    bool skipSplash = false;
    std::string fileToOpen;
    std::vector<std::string> imHexArgs;

    // Handle command line arguments
    {
        if (argc == 1) {
            // No optional arguments used, use default
        } else if (argc == 2) {
            if (argv[1] == std::string("--no-splash")) skipSplash = true;
            else fileToOpen = argv[1];
        } else if (argc == 3) {
            fileToOpen = argv[1];

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

    imHexArgs.emplace_back("git-hash=" GIT_COMMIT_HASH);
    imHexArgs.emplace_back("git-branch=" GIT_BRANCH);

    if (!skipSplash) {
        hex::pre::WindowSplash window;

        window.addStartupTask("Checking for updates...", [&]{
            hex::Net net;

            auto releases = net.getJson("https://api.github.com/repos/WerWolv/ImHex/releases/latest");
            if (releases.code != 200)
                return false;

            if (!releases.response.contains("tag_name") || !releases.response["tag_name"].is_string())
                return false;

            auto currVersion = "v" + std::string(IMHEX_VERSION).substr(0, 5);
            auto latestVersion = releases.response["tag_name"].get<std::string_view>();

            if (latestVersion != currVersion)
                imHexArgs.push_back(std::string("update=") + latestVersion.data());

            return true;
        });

        window.addStartupTask("Creating directories...", []{
            std::filesystem::create_directories(hex::getPath(hex::ImHexPath::Patterns)[0]);
            std::filesystem::create_directories(hex::getPath(hex::ImHexPath::PatternsInclude)[0]);
            std::filesystem::create_directories(hex::getPath(hex::ImHexPath::Magic)[0]);
            std::filesystem::create_directories(hex::getPath(hex::ImHexPath::Plugins)[0]);
            std::filesystem::create_directories(hex::getPath(hex::ImHexPath::Resources)[0]);
            std::filesystem::create_directories(hex::getPath(hex::ImHexPath::Config)[0]);

            return true;
        });

        if (!window.loop())
            imHexArgs.emplace_back("tasks-failed");

    } else {
        imHexArgs.emplace_back("splash-skipped");
    }

    // Launch ImHex
    std::string packedArgs = "--args=" + hex::combineStrings(imHexArgs, "|");

    if (fileToOpen.empty())
        return execl(ImHexPath, ImHexPath, packedArgs.c_str(), nullptr);
    else
        return execl(ImHexPath, ImHexPath, fileToOpen.c_str(), packedArgs.c_str(), nullptr);
}