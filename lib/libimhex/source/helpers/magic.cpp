#include <hex/helpers/magic.hpp>

#include <hex/helpers/utils.hpp>
#include <hex/helpers/fs.hpp>
#include <hex/helpers/logger.hpp>
#include <hex/helpers/default_paths.hpp>

#include <wolv/utils/guards.hpp>
#include <wolv/utils/string.hpp>

#include <hex/providers/provider.hpp>

#include <filesystem>
#include <optional>
#include <string>

#include <magic.h>
#include <hex/api/task_manager.hpp>
#include <hex/api/content_registry/pattern_language.hpp>
#include <hex/helpers/binary_pattern.hpp>

#if defined(_MSC_VER)
    #include <direct.h>
#else
    #include <unistd.h>
#endif

#if defined(OS_WINDOWS)
    #define MAGIC_PATH_SEPARATOR ";"
#else
    #define MAGIC_PATH_SEPARATOR ":"
#endif


namespace hex::magic {

    static std::optional<std::string> getMagicFiles(bool sourceFiles = false) {
        std::string magicFiles;

        std::error_code error;
        for (const auto &dir : paths::Magic.read()) {
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

        std::array<char, 1024> cwd = { };
        if (getcwd(cwd.data(), cwd.size()) == nullptr)
            return false;

        std::optional<std::fs::path> magicFolder;
        for (const auto &dir : paths::Magic.write()) {
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
        if (data.empty()) return "";

        auto magicFiles = getMagicFiles();

        if (magicFiles.has_value()) {
            magic_t ctx = magic_open(firstEntryOnly ? MAGIC_NONE : MAGIC_CONTINUE);
            ON_SCOPE_EXIT { magic_close(ctx); };

            if (magic_load(ctx, magicFiles->c_str()) == 0) {
                if (auto description = magic_buffer(ctx, data.data(), data.size()); description != nullptr) {
                    auto result = wolv::util::replaceStrings(description, "\\012-", "\n-");
                    if (result.ends_with("- data"))
                        result = result.substr(0, result.size() - 6);

                    return result;
                }
            }
        }

        return "";
    }

    std::string getDescription(prv::Provider *provider, u64 address, size_t size, bool firstEntryOnly) {
        std::vector<u8> buffer(std::min<u64>(provider->getSize(), size), 0x00);
        provider->read(address, buffer.data(), buffer.size());

        return getDescription(buffer, firstEntryOnly);
    }

    std::string getMIMEType(const std::vector<u8> &data, bool firstEntryOnly) {
        if (data.empty()) return "";

        auto magicFiles = getMagicFiles();

        if (magicFiles.has_value()) {
            magic_t ctx = magic_open(MAGIC_MIME_TYPE | (firstEntryOnly ? MAGIC_NONE : MAGIC_CONTINUE));
            ON_SCOPE_EXIT { magic_close(ctx); };

            if (magic_load(ctx, magicFiles->c_str()) == 0) {
                if (auto mimeType = magic_buffer(ctx, data.data(), data.size()); mimeType != nullptr) {
                    auto result = wolv::util::replaceStrings(mimeType, "\\012-", "\n-");
                    if (result.ends_with("- application/octet-stream"))
                        result = result.substr(0, result.size() - 26);

                    return result;
                }
            }
        }

        return "";
    }

    std::string getMIMEType(prv::Provider *provider, u64 address, size_t size, bool firstEntryOnly) {
        std::vector<u8> buffer(std::min<u64>(provider->getSize(), size), 0x00);
        provider->read(address, buffer.data(), buffer.size());

        return getMIMEType(buffer, firstEntryOnly);
    }

    std::string getExtensions(prv::Provider *provider, u64 address, size_t size, bool firstEntryOnly) {
        std::vector<u8> buffer(std::min<u64>(provider->getSize(), size), 0x00);
        provider->read(address, buffer.data(), buffer.size());

        return getExtensions(buffer, firstEntryOnly);
    }

    std::string getExtensions(const std::vector<u8> &data, bool firstEntryOnly) {
        if (data.empty()) return "";

        auto magicFiles = getMagicFiles();

        if (magicFiles.has_value()) {
            magic_t ctx = magic_open(MAGIC_EXTENSION | (firstEntryOnly ? MAGIC_NONE : MAGIC_CONTINUE));
            ON_SCOPE_EXIT { magic_close(ctx); };

            if (magic_load(ctx, magicFiles->c_str()) == 0) {
                if (auto extension = magic_buffer(ctx, data.data(), data.size()); extension != nullptr) {
                    auto result = wolv::util::replaceStrings(extension, "\\012-", "\n-");
                    if (result.ends_with("- ???"))
                        result = result.substr(0, result.size() - 5);

                    return result;
                }
            }
        }

        return "";
    }

    std::string getAppleCreatorType(prv::Provider *provider, u64 address, size_t size, bool firstEntryOnly) {
        std::vector<u8> buffer(std::min<u64>(provider->getSize(), size), 0x00);
        provider->read(address, buffer.data(), buffer.size());

        return getAppleCreatorType(buffer, firstEntryOnly);
    }

    std::string getAppleCreatorType(const std::vector<u8> &data, bool firstEntryOnly) {
        if (data.empty()) return "";

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


    std::vector<FoundPattern> findViablePatterns(prv::Provider *provider, Task* task) {
        std::vector<FoundPattern> result;

        pl::PatternLanguage runtime;
        ContentRegistry::PatternLanguage::configureRuntime(runtime, provider);

        bool foundCorrectType = false;

        auto mimeType = getMIMEType(provider, 0, 4_KiB, true);

        std::error_code errorCode;
        for (const auto &dir : paths::Patterns.read()) {
            for (auto &entry : std::fs::recursive_directory_iterator(dir, errorCode)) {
                if (task != nullptr)
                    task->update();

                foundCorrectType = false;
                if (!entry.is_regular_file())
                    continue;

                wolv::io::File file(entry.path(), wolv::io::File::Mode::Read);
                if (!file.isValid())
                    continue;

                std::string author, description;
                bool matchedMimeType = false;
                std::optional<u64> magicOffset;


                const auto pragmaValues = runtime.getPragmaValues(file.readString());
                if (auto it = pragmaValues.find("author"); it != pragmaValues.end())
                    author = it->second;
                if (auto it = pragmaValues.find("description"); it != pragmaValues.end())
                    description = it->second;

                // Format: #pragma MIME type/subtype
                for (auto [it, itEnd] = pragmaValues.equal_range("MIME"); it != itEnd; ++it) {
                    if (isValidMIMEType(it->second) && it->second == mimeType) {
                        foundCorrectType = true;
                        matchedMimeType = true;
                    }
                }
                // Format: #pragma magic [ AA BB CC DD ] @ 0x12345678
                for (auto [it, itEnd] = pragmaValues.equal_range("magic"); it != itEnd; ++it) {
                    const auto pattern = [value = it->second]() mutable -> std::optional<BinaryPattern> {
                        value = wolv::util::trim(value);

                        if (value.empty())
                            return std::nullopt;

                        if (!value.starts_with('['))
                            return std::nullopt;

                        value = value.substr(1);

                        const auto end = value.find(']');
                        if (end == std::string::npos)
                            return std::nullopt;
                        value.resize(end);

                        value = wolv::util::trim(value);

                        return BinaryPattern(value);
                    }();

                    const auto address = [provider, value = it->second]() mutable -> std::optional<u64> {
                        value = wolv::util::trim(value);

                        if (value.empty())
                            return std::nullopt;

                        const auto start = value.find('@');
                        if (start == std::string::npos)
                            return std::nullopt;

                        value = value.substr(start + 1);
                        value = wolv::util::trim(value);

                        size_t end = 0;
                        auto result = std::stoll(value, &end, 0);
                        if (end != value.length())
                            return std::nullopt;

                        if (result < 0) {
                            const auto size = provider->getActualSize();
                            if (u64(-result) > size) {
                                return std::nullopt;
                            }

                            return size + result;
                        } else {
                            return result;
                        }
                    }();

                    if (address && pattern) {
                        std::vector<u8> bytes(pattern->getSize());
                        if (!bytes.empty()) {
                            provider->read(*address, bytes.data(), bytes.size());

                            if (pattern->matches(bytes)) {
                                foundCorrectType = true;
                                magicOffset = address;
                            }
                        }
                    }
                }

                if (foundCorrectType) {
                    result.emplace_back(
                        entry.path(),
                        std::move(author),
                        std::move(description),
                        matchedMimeType ? std::make_optional(mimeType) : std::nullopt,
                        magicOffset
                    );
                }

                runtime.reset();
            }
        }

        return result;
    }

}