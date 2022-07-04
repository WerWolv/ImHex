#include <hex/helpers/magic.hpp>

#include <hex/helpers/utils.hpp>
#include <hex/helpers/fs.hpp>

#include <hex/providers/provider.hpp>

#include <filesystem>
#include <optional>
#include <string>

#include <magic.h>

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
                if (entry.is_regular_file() && ((sourceFiles && entry.path().extension().empty()) || (!sourceFiles && entry.path().extension() == ".mgc"))) {
                    magicFiles += fs::toShortPath(entry.path()).string() + MAGIC_PATH_SEPARATOR;
                }
            }
        }

        if (error)
            return std::nullopt;
        else
            return magicFiles;
    }

    bool compile() {
        magic_t ctx = magic_open(MAGIC_NONE);
        ON_SCOPE_EXIT { magic_close(ctx); };

        auto magicFiles = getMagicFiles(true);

        if (!magicFiles.has_value())
            return false;

        return magic_compile(ctx, magicFiles->c_str()) == 0;
    }

    std::string getDescription(const std::vector<u8> &data) {
        auto magicFiles = getMagicFiles();

        if (magicFiles.has_value()) {
            magic_t ctx = magic_open(MAGIC_NONE);
            ON_SCOPE_EXIT { magic_close(ctx); };

            if (magic_load(ctx, magicFiles->c_str()) == 0)
                return magic_buffer(ctx, data.data(), data.size()) ?: "";
        }

        return "";
    }

    std::string getDescription(prv::Provider *provider, size_t size) {
        std::vector<u8> buffer(std::min(provider->getSize(), size), 0x00);
        provider->read(provider->getBaseAddress(), buffer.data(), buffer.size());

        return getDescription(buffer);
    }

    std::string getMIMEType(const std::vector<u8> &data) {
        auto magicFiles = getMagicFiles();

        if (magicFiles.has_value()) {
            magic_t ctx = magic_open(MAGIC_MIME_TYPE);
            ON_SCOPE_EXIT { magic_close(ctx); };

            if (magic_load(ctx, magicFiles->c_str()) == 0)
                return magic_buffer(ctx, data.data(), data.size()) ?: "";
        }

        return "";
    }

    std::string getMIMEType(prv::Provider *provider, size_t size) {
        std::vector<u8> buffer(std::min(provider->getSize(), size), 0x00);
        provider->read(provider->getBaseAddress(), buffer.data(), buffer.size());

        return getMIMEType(buffer);
    }

}