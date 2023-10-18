#include <hex/api_urls.hpp>
#include <hex/api/imhex_api.hpp>

#include <hex/helpers/http_requests.hpp>
#include <hex/helpers/utils.hpp>

using namespace std::literals::string_literals;

std::string getUpdateUrl(std::string_view versionType, std::string_view operatingSystem) {
    // Get the latest version info from the ImHex API
    auto response = hex::HttpRequest("GET",
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

std::string getUpdateType() {
    #if defined (OS_WINDOWS)
        if (!hex::ImHexApi::System::isPortableVersion())
            return "win-msi";
    #elif defined (OS_MACOS)
        return "mac-dmg";
    #endif

    return "";
}

std::optional<std::fs::path> downloadUpdate(const std::string &url, const std::string &type) {
    // Download the update
    auto response = hex::HttpRequest("GET", url).downloadFile().get();

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
        for (const auto &path : hex::fs::getDefaultPaths(hex::fs::ImHexPath::Config)) {
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

        // Write the downloaded update data to the file
        file.writeVector(data);

        // Save the path to the update file
        filePath = file.getPath();
    }

    return filePath;
}

int installUpdate(const std::string &type, const std::fs::path &updatePath) {
    auto newUpdatePath = updatePath;

    // Install the update based on the installation type
    if (type == "win-msi") {
        // Rename the update file to the correct extension
        newUpdatePath.replace_extension(".msi");
        std::fs::rename(updatePath, newUpdatePath);

        // Install the update using msiexec
        hex::runCommand(hex::format("msiexec /i {} /passive", updatePath.string()));

        return EXIT_SUCCESS;
    } else if (type == "macos-dmg") {
        // Rename the update file to the correct extension
        newUpdatePath.replace_extension(".dmg");
        std::fs::rename(updatePath, newUpdatePath);

        // Mount the update file for the user to drag ImHex to the Applications folder
        hex::runCommand(hex::format("hdiutil attach {}", updatePath.string()));

        return EXIT_SUCCESS;
    } else {
        // If the installation type isn't handled here, the detected installation type doesn't support updates through the updater
        hex::log::error("Install type cannot be updated");
        return EXIT_FAILURE;
    }
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
        return EXIT_FAILURE;
    }

    hex::log::info("Update URL found: {}", updateUrl);

    // Download the update file
    auto updatePath = downloadUpdate(updateUrl, updateType);
    if (!updatePath.has_value())
        return EXIT_FAILURE;

    // Install the update
    return installUpdate(updateType, *updatePath);
}