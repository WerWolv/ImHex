#include <hex/api_urls.hpp>
#include <hex/api/imhex_api.hpp>
#include <hex/api/task_manager.hpp>

#include <hex/helpers/http_requests.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/helpers/default_paths.hpp>
#include <hex/helpers/logger.hpp>
#include <hex/helpers/fs.hpp>

#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <optional>

using namespace std::literals::string_literals;

std::string getArtifactUrl(std::string_view artifactEnding, hex::ImHexApi::System::UpdateType updateType) {
    // Get the latest version info from the ImHex API
    const auto response = hex::HttpRequest("GET",
        GitHubApiURL + "/releases"s
    ).execute().get();

    const auto &data = response.getData();

    // Make sure we got a valid response
    if (!response.isSuccess()) {
        hex::log::error("Failed to get latest version info: ({}) {}", response.getStatusCode(), data);

        return { };
    }

    try {
        const auto json = nlohmann::json::parse(data);

        for (const auto &release : json) {
            if (updateType == hex::ImHexApi::System::UpdateType::Stable && !release["target_commitish"].get<std::string>().starts_with("releases/v"))
                continue;
            if (updateType == hex::ImHexApi::System::UpdateType::Nightly && release["tag_name"].get<std::string>() != "nightly")
                continue;

            // Loop over all assets in the release
            for (const auto &asset : release["assets"]) {
                // Check if the asset name ends with the specified artifact ending
                if (asset["name"].get<std::string>().ends_with(artifactEnding)) {
                    return asset["browser_download_url"].get<std::string>();
                }
            }
        }

        hex::log::error("No suitable artifact found for ending: {}", artifactEnding);
        return { };
    } catch (const std::exception &e) {
        hex::log::error("Failed to parse latest version info: {}", e.what());
        return { };
    }
}

std::optional<std::fs::path> downloadUpdate(const std::string &url) {
    // Download the update
    const auto response = hex::HttpRequest("GET", url).downloadFile().get();

    // Make sure we got a valid response
    if (!response.isSuccess()) {
        hex::log::error("Failed to download update");
        return std::nullopt;
    }

    const auto &data = response.getData();

    const auto updateFileName = wolv::util::splitString(url, "/").back();

    // Write the update to a file
    std::fs::path filePath;
    {
        // Loop over all available paths
        wolv::io::File file;
        for (const auto &path : hex::paths::Config.write()) {
            // Remove any existing update files
            wolv::io::fs::remove(path / updateFileName);

            // If a valid location hasn't been found already, try to create a new file
            if (!file.isValid())
                file = wolv::io::File(path / updateFileName, wolv::io::File::Mode::Create);
        }

        // If the update data can't be written to any of the default paths, the update cannot continue
        if (!file.isValid()) {
            hex::log::error("Failed to create update file");
            return std::nullopt;
        }

        hex::log::info("Writing update to file: {}", file.getPath().string());

        // Write the downloaded update data to the file
        file.writeVector(data);

        // Save the path to the update file
        filePath = file.getPath();
    }

    return filePath;
}

#if defined(__x86_64__)
    #define ARCH_DEPENDENT(x86_64, arm64) x86_64
#elif defined(__arm64__)
    #define ARCH_DEPENDENT(x86_64, arm64) arm64
#else
    #define ARCH_DEPENDENT(x86_64, arm64) ""
#endif

std::string_view getUpdateArtifactEnding() {
    #if defined (OS_WINDOWS)
        if (!hex::ImHexApi::System::isPortableVersion()) {
            return ARCH_DEPENDENT("Windows-x86_64.msi", "Windows-arm64.msi");
        }
    #elif defined (OS_MACOS)
        return ARCH_DEPENDENT("macOS-x86_64.dmg", "macOS-arm64.dmg");
    #elif defined (OS_LINUX)
        if (hex::executeCommand("grep 'ID=ubuntu' /etc/os-release") == 0) {
            if (hex::executeCommand("grep 'VERSION_ID=\"24.04\"' /etc/os-release") == 0)
                return ARCH_DEPENDENT("Ubuntu-24.04-x86_64.deb", "");
            else if (hex::executeCommand("grep 'VERSION_ID=\"24.10\"' /etc/os-release") == 0)
                return ARCH_DEPENDENT("Ubuntu-24.10-x86_64.deb", "");
            else if (hex::executeCommand("grep 'VERSION_ID=\"25.04\"' /etc/os-release") == 0)
                return ARCH_DEPENDENT("Ubuntu-25.04-x86_64.deb", "");
        } else if (hex::executeCommand("grep 'ID=fedora' /etc/os-release") == 0) {
            if (hex::executeCommand("grep 'VERSION_ID=\"41\"' /etc/os-release") == 0)
                return ARCH_DEPENDENT("Fedora-41-x86_64.rpm", "");
            else if (hex::executeCommand("grep 'VERSION_ID=\"42\"' /etc/os-release") == 0)
                return ARCH_DEPENDENT("Fedora-42-x86_64.rpm", "");
            else if (hex::executeCommand("grep 'VERSION_ID=\"rawhide\"' /etc/os-release") == 0)
                return ARCH_DEPENDENT("Fedora-rawhide-x86_64.rpm", "");
        } else if (hex::executeCommand("grep '^NAME=\"Arch Linux\"' /etc/os-release") == 0) {
            return ARCH_DEPENDENT("ArchLinux-x86_64.pkg.tar.zst", "");
        }
    #endif

    return "";
}

bool installUpdate(const std::fs::path &updatePath) {
    struct UpdateHandler {
        const char *ending;
        const char *command;
    };

    constexpr static auto UpdateHandlers = {
        UpdateHandler { ".msi",             "msiexec /i \"{}\" /qb"                                        },
        UpdateHandler { ".dmg",             "hdiutil attach -autoopen \"{}\""                              },
        UpdateHandler { ".deb",             "sudo apt update && sudo apt install -y --fix-broken \"{}\""   },
        UpdateHandler { ".rpm",             "sudo rpm -i \"{}\""                                           },
        UpdateHandler { ".pkg.tar.zst",     "sudo pacman -Syy && sudo pacman -U --noconfirm \"{}\""        }
    };

    const auto updateFileName = wolv::util::toUTF8String(updatePath.filename());
    for (const auto &handler : UpdateHandlers) {
        if (updateFileName.ends_with(handler.ending)) {
            // Install the update using the correct command
            const auto command = fmt::format(fmt::runtime(handler.command), updatePath.string());

            hex::log::info("Starting update process with command: '{}'", command);
            hex::startProgram(command);

            return true;
        }
    }

    // If the installation type isn't handled here, the detected installation type doesn't support updates through the updater
    hex::log::error("Install type cannot be updated");

    return false;
}

int main(int argc, char **argv) {
    hex::TaskManager::setCurrentThreadName("ImHex Updater");
    hex::TaskManager::setMainThreadId(std::this_thread::get_id());
    hex::log::impl::enableColorPrinting();

    // Check we have the correct number of arguments
    if (argc != 2) {
        hex::log::error("Failed to start updater: Invalid arguments");
        return EXIT_FAILURE;
    }

    // Read the version type from the arguments
    const std::string_view versionTypeString = argv[1];
    hex::log::info("Updater started with version type: {}", versionTypeString);

    // Convert the version type string to the enum value
    hex::ImHexApi::System::UpdateType updateType;
    std::string releaseUrl;
    if (versionTypeString == "stable") {
        updateType = hex::ImHexApi::System::UpdateType::Stable;
        releaseUrl = "https://github.com/WerWolv/ImHex/releases/latest";
    } else if (versionTypeString == "nightly") {
        updateType = hex::ImHexApi::System::UpdateType::Nightly;
        releaseUrl = "https://github.com/WerWolv/ImHex/releases/tag/nightly";
    } else {
        hex::log::error("Invalid version type: {}", versionTypeString);

        // Wait for user input before exiting so logs can be read
        std::getchar();

        return EXIT_FAILURE;
    }

    // Get the artifact name ending based on the current platform and architecture
    const auto artifactEnding = getUpdateArtifactEnding();
    if (artifactEnding.empty()) {
        hex::log::error("Updater artifact ending is empty");

        // Wait for user input before exiting so logs can be read
        std::getchar();

        return EXIT_FAILURE;
    }

    // Get the URL for the correct update artifact
    const auto updateArtifactUrl = getArtifactUrl(artifactEnding, updateType);
    if (updateArtifactUrl.empty()) {
        // If the current artifact cannot be updated, open the latest release page in the browser

        hex::log::warn("Failed to get update artifact URL for ending: {}", artifactEnding);
        hex::log::info("Opening release page in browser to allow manual update");

        hex::openWebpage(releaseUrl);

        return EXIT_FAILURE;
    }

    // Download the update artifact
    const auto updatePath = downloadUpdate(updateArtifactUrl);

    // Install the update
    if (!installUpdate(*updatePath)) {
        // Open the latest release page in the default browser to allow the user to manually update
        hex::openWebpage(releaseUrl);

        // Wait for user input before exiting so logs can be read
        std::getchar();

        return EXIT_FAILURE;
    } else {
        return EXIT_SUCCESS;
    }
}