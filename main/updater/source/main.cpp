#include <hex/api_urls.hpp>
#include <hex/api/imhex_api.hpp>

#include <hex/helpers/http_requests.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/helpers/default_paths.hpp>

using namespace std::literals::string_literals;

std::string getUpdateUrl(std::string_view versionType, std::string_view operatingSystem) {
    // Get the latest version info from the ImHex API
    const auto response = hex::HttpRequest("GET",
                                     ImHexApiURL + fmt::format("/update/{}/{}",
                                                               versionType,
                                                               operatingSystem
                                                               )
                                     ).execute().get();

    const auto &data = response.getData();

    // Make sure we got a valid response
    if (!response.isSuccess()) {
        hex::log::error("Failed to get latest version info: ({}) {}", response.getStatusCode(), data);

        return { };
    }

    return data;
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

    // Write the update to a file
    std::fs::path filePath;
    {
        constexpr static auto UpdateFileName = "update.hexupd";

        // Loop over all available paths
        wolv::io::File file;
        for (const auto &path : hex::paths::Config.write()) {
            // Remove any existing update files
            wolv::io::fs::remove(path / UpdateFileName);

            // If a valid location hasn't been found already, try to create a new file
            if (!file.isValid())
                file = wolv::io::File(path / UpdateFileName, wolv::io::File::Mode::Create);
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

std::string getUpdateType() {
    #if defined (OS_WINDOWS)
        if (!hex::ImHexApi::System::isPortableVersion())
            return "win-msi";
    #elif defined (OS_MACOS)
        return "mac-dmg";
    #elif defined (OS_LINUX)
        if (hex::executeCommand("lsb_release -a | grep Ubuntu") == 0) {
            if (hex::executeCommand("lsb_release -a | grep 22.") == 0)
                return "linux-deb-22.04";
            else if (hex::executeCommand("lsb_release -a | grep 23.") == 0)
                return "linux-deb-23.04";
        }
    #endif

    return "";
}

int installUpdate(const std::string &type, std::fs::path updatePath) {
    struct UpdateHandler {
        const char *type;
        const char *extension;
        const char *command;
    };

    constexpr static auto UpdateHandlers = {
        UpdateHandler { "win-msi",          ".msi",  "msiexec /passive /package {}"                             },
        UpdateHandler { "macos-dmg",        ".dmg",  "hdiutil attach {}"                                        },
        UpdateHandler { "linux-deb-22.04",  ".deb",  "sudo apt update && sudo apt install -y --fix-broken {}"   },
        UpdateHandler { "linux-deb-23.04",  ".deb",  "sudo apt update && sudo apt install -y --fix-broken {}"   },
    };

    for (const auto &handler : UpdateHandlers) {
        if (type == handler.type) {
            // Rename the update file to the correct extension
            const auto originalPath = updatePath;
            updatePath.replace_extension(handler.extension);

            hex::log::info("Moving update package from {} to {}", originalPath.string(), updatePath.string());
            std::fs::rename(originalPath, updatePath);

            // Install the update using the correct command
            const auto command = hex::format(handler.command, updatePath.string());
            hex::log::info("Starting update process with command: '{}'", command);
            hex::startProgram(command);

            return EXIT_SUCCESS;
        }
    }

    // If the installation type isn't handled here, the detected installation type doesn't support updates through the updater
    hex::log::error("Install type cannot be updated");

    // Open the latest release page in the default browser to allow the user to manually update
    hex::openWebpage("https://github.com/WerWolv/ImHex/releases/latest");

    return EXIT_FAILURE;
}

int main(int argc, char **argv) {
    hex::log::impl::enableColorPrinting();

    // Check we have the correct number of arguments
    if (argc != 2) {
        hex::log::error("Failed to start updater: Invalid arguments");
        return EXIT_FAILURE;
    }

    // Read the version type from the arguments
    const auto versionType = argv[1];
    hex::log::info("Updater started with version type: {}", versionType);

    // Query the update type
    const auto updateType = getUpdateType();
    hex::log::info("Detected OS String: {}", updateType);

    // Make sure we got a valid update type
    if (updateType.empty()) {
        hex::log::error("Failed to detect installation type");
        return EXIT_FAILURE;
    }

    // Get the url to the requested update from the ImHex API
    const auto updateUrl = getUpdateUrl(versionType, updateType);
    if (updateUrl.empty()) {
        hex::log::error("Failed to get update URL");
        return EXIT_FAILURE;
    }

    hex::log::info("Update URL found: {}", updateUrl);

    // Download the update file
    const auto updatePath = downloadUpdate(updateUrl);
    if (!updatePath.has_value())
        return EXIT_FAILURE;

    hex::log::info("Downloaded update successfully");

    // Install the update
    return installUpdate(updateType, *updatePath);
}