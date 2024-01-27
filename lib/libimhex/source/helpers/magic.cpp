#include <hex/helpers/magic.hpp>

#include <hex/helpers/utils.hpp>
#include <hex/helpers/fs.hpp>
#include <hex/helpers/logger.hpp>

#include <wolv/utils/guards.hpp>
#include <wolv/utils/string.hpp>

#include <hex/providers/provider.hpp>

#include <filesystem>
#include <optional>
#include <string>

#include <magic.h>
#include <unistd.h>

#if defined(OS_WINDOWS)
    #define MAGIC_PATH_SEPARATOR ";"
#else
    #define MAGIC_PATH_SEPARATOR ":"
#endif


namespace hex::magic {

    static std::optional<std::string> getMagicFiles(bool sourceFiles = false) {
        std::string magicFiles;

        std::error_code error;
        for (const auto &dir : fs::getDefaultPaths(fs::ImHexPath::Magic)) {
            for (const auto &entry : std::fs::directory_iterator(dir, error)) {
                auto path = std::fs::absolute(entry.path());

                if (sourceFiles) {
                    if (path.extension().empty() || entry.is_directory())
                        magicFiles += wolv::util::toUTF8String(path) + MAGIC_PATH_SEPARATOR;
                } else {
                    if (path.extension() == ".mgc")
                        magicFiles += wolv::util::toUTF8String(path) + MAGIC_PATH_SEPARATOR;
                }
            }
        }

        if (!magicFiles.empty())
            magicFiles.pop_back();

        if (error)
            return std::nullopt;
        else
            return magicFiles;
    }

    bool compile() {
        magic_t ctx = magic_open(MAGIC_CHECK);
        ON_SCOPE_EXIT { magic_close(ctx); };

        auto magicFiles = getMagicFiles(true);

        if (!magicFiles.has_value())
            return false;

        if (magicFiles->empty())
            return true;

        std::array<char, 1024> cwd = { 0x00 };
        if (getcwd(cwd.data(), cwd.size()) == nullptr)
            return false;

        std::optional<std::fs::path> magicFolder;
        for (const auto &dir : fs::getDefaultPaths(fs::ImHexPath::Magic)) {
            if (std::fs::exists(dir) && fs::isPathWritable(dir)) {
                magicFolder = dir;
                break;
            }
        }

        if (!magicFolder.has_value()) {
            log::error("Could not find a writable magic folder");
            return false;
        }

        if (chdir(wolv::util::toUTF8String(*magicFolder).c_str()) != 0)
            return false;

        auto result = magic_compile(ctx, magicFiles->c_str()) == 0;
        if (!result) {
            log::error("Failed to compile magic files \"{}\": {}", *magicFiles, magic_error(ctx));
        }

        if (chdir(cwd.data()) != 0)
            return false;

        return result;
    }

    std::string getDescription(const std::vector<u8> &data, bool firstEntryOnly) {
        auto magicFiles = getMagicFiles();

        if (magicFiles.has_value()) {
            magic_t ctx = magic_open(MAGIC_COMPRESS | (firstEntryOnly ? MAGIC_NONE : MAGIC_CONTINUE));
            ON_SCOPE_EXIT { magic_close(ctx); };

            if (magic_load(ctx, magicFiles->c_str()) == 0) {
                if (auto result = magic_buffer(ctx, data.data(), data.size()); result != nullptr)
                    return wolv::util::replaceStrings(result, "\\012-", "\n-");
            }
        }

        return "";
    }

    std::string getDescription(prv::Provider *provider, size_t size, bool firstEntryOnly) {
        std::vector<u8> buffer(std::min<u64>(provider->getSize(), size), 0x00);
        provider->read(provider->getBaseAddress(), buffer.data(), buffer.size());

        return getDescription(buffer, firstEntryOnly);
    }

    std::string getMIMEType(const std::vector<u8> &data, bool firstEntryOnly) {
        auto magicFiles = getMagicFiles();

        if (magicFiles.has_value()) {
            magic_t ctx = magic_open(MAGIC_COMPRESS | MAGIC_MIME_TYPE | (firstEntryOnly ? MAGIC_NONE : MAGIC_CONTINUE));
            ON_SCOPE_EXIT { magic_close(ctx); };

            if (magic_load(ctx, magicFiles->c_str()) == 0) {
                if (auto result = magic_buffer(ctx, data.data(), data.size()); result != nullptr)
                    return wolv::util::replaceStrings(result, "\\012-", "\n-");
            }
        }

        return "";
    }

    std::string getExtensions(prv::Provider *provider, size_t size, bool firstEntryOnly) {
        std::vector<u8> buffer(std::min<u64>(provider->getSize(), size), 0x00);
        provider->read(provider->getBaseAddress(), buffer.data(), buffer.size());

        return getExtensions(buffer, firstEntryOnly);
    }

    std::string getExtensions(const std::vector<u8> &data, bool firstEntryOnly) {
        auto magicFiles = getMagicFiles();

        if (magicFiles.has_value()) {
            magic_t ctx = magic_open(MAGIC_EXTENSION | (firstEntryOnly ? MAGIC_NONE : MAGIC_CONTINUE));
            ON_SCOPE_EXIT { magic_close(ctx); };

            if (magic_load(ctx, magicFiles->c_str()) == 0) {
                if (auto result = magic_buffer(ctx, data.data(), data.size()); result != nullptr)
                    return wolv::util::replaceStrings(result, "\\012-", "\n-");
            }
        }

        return "";
    }

    std::string getAppleCreatorType(prv::Provider *provider, size_t size, bool firstEntryOnly) {
        std::vector<u8> buffer(std::min<u64>(provider->getSize(), size), 0x00);
        provider->read(provider->getBaseAddress(), buffer.data(), buffer.size());

        return getAppleCreatorType(buffer, firstEntryOnly);
    }

    std::string getAppleCreatorType(const std::vector<u8> &data, bool firstEntryOnly) {
        auto magicFiles = getMagicFiles();

        if (magicFiles.has_value()) {
            magic_t ctx = magic_open(MAGIC_APPLE | (firstEntryOnly ? MAGIC_NONE : MAGIC_CONTINUE));
            ON_SCOPE_EXIT { magic_close(ctx); };

            if (magic_load(ctx, magicFiles->c_str()) == 0) {
                if (auto result = magic_buffer(ctx, data.data(), data.size()); result != nullptr)
                    return wolv::util::replaceStrings(result, "\\012-", "\n-");
            }
        }

        return {};
    }

    std::string getMIMEType(prv::Provider *provider, size_t size, bool firstEntryOnly) {
        std::vector<u8> buffer(std::min<u64>(provider->getSize(), size), 0x00);
        provider->read(provider->getBaseAddress(), buffer.data(), buffer.size());

        return getMIMEType(buffer, firstEntryOnly);
    }

    bool isValidMIMEType(const std::string &mimeType) {
        // MIME types always contain a slash
        if (!mimeType.contains("/"))
            return false;

        // The MIME type "application/octet-stream" is a fallback type for arbitrary binary data.
        // Specifying this in a pattern would make it get suggested for every single unknown binary that's being loaded.
        // We don't want that, so we ignore it here
        if (mimeType == "application/octet-stream")
            return false;

        return true;
    }

}