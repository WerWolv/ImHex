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

#include "hex/api/http/store_api.hpp"

#include <hex/api/content_registry/pattern_language.hpp>
#include <hex/api/task_manager.hpp>

#include <hex/providers/matchers/base_matcher.hpp>

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

    static std::optional<FoundPattern> findViablePattern(const std::fs::path &path, prv::Provider *provider, const std::multimap<std::string, std::string> &pragmaValues, Task *task) {
        std::string author, description;
        for (auto [start, end] = pragmaValues.equal_range("author"); start != end; ++start) {
            author = start->second;
        }
        for (auto [start, end] = pragmaValues.equal_range("description"); start != end; ++start) {
            description = start->second;
        }

        if (auto matcherStrategies = dynamic_cast<prv::ProviderMatchStrategiesBase*>(provider)) {
            const auto strategies = matcherStrategies->createMatchers(provider);

            for (const auto &strategy : strategies) {
                for (auto [it, itEnd] = pragmaValues.equal_range(std::string(strategy->getPragma())); it != itEnd; ++it) {
                    if (task != nullptr)
                        task->update();

                    if (strategy->match(it->second)) {
                        return FoundPattern {
                            .patternFilePath = path,
                            .author = std::move(author),
                            .description = std::move(description),
                            .matcher = strategy,
                            .downloadUrl = { },
                            .remote = false
                        };
                    }
                }
            }
        }

        return std::nullopt;
    }

    std::vector<FoundPattern> findViablePatterns(prv::Provider *provider, bool searchOnline, Task *task) {
        std::set<FoundPattern> patterns;

        // Search local patterns
        {
            pl::PatternLanguage runtime;
            ContentRegistry::PatternLanguage::configureRuntime(runtime, provider);

            std::error_code errorCode;
            for (const auto &dir : paths::Patterns.read()) {
                for (auto &entry : std::fs::recursive_directory_iterator(dir, errorCode)) {
                    if (task != nullptr)
                        task->update();

                    if (!entry.is_regular_file())
                        continue;

                    wolv::io::File file(entry.path(), wolv::io::File::Mode::Read);
                    if (!file.isValid())
                        continue;

                    const auto pragmaValues = runtime.getPragmaValues(file.readString());

                    if (auto foundPattern = findViablePattern(entry.path(), provider, pragmaValues, task); foundPattern.has_value()) {
                        patterns.insert(std::move(*foundPattern));
                    }

                    runtime.reset();
                }
            }
        }

        // Search remote patterns if allowed
        if (searchOnline) {
            const auto request = StoreApi::get();
            const auto &response = request.get();
            if (!response.isSuccess())
                return { patterns.begin(), patterns.end() };

            for (auto [start, end] = response.getData().categories.equal_range("patterns"); start != end; ++start) {
                if (task != nullptr)
                    task->update();

                for (const auto &patternEntry : start->second) {
                    if (auto foundPattern = findViablePattern(patternEntry.fileName, provider, patternEntry.pragmas, task); foundPattern.has_value()) {
                        foundPattern->downloadUrl = patternEntry.link;
                        foundPattern->remote = true;


                        patterns.insert(std::move(*foundPattern));
                    }
                }

            }
        }

        return { patterns.begin(), patterns.end() };
    }

}
