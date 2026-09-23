#include <hex/helpers/encoding_file.hpp>

#include <hex/helpers/unicode.hpp>

#include <hex/helpers/default_paths.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/helpers/utils.hpp>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <mutex>
#include <ranges>
#include <set>
#include <wolv/io/file.hpp>
#include <wolv/utils/string.hpp>

namespace hex {

    namespace {

        /**
         * @brief Reads the right hand side of a table line
         *
         * A `\uXXXX` escape names a code point, so a table can map a byte to a character with no
         * glyph of its own, such as a control code. `\\` is one literal backslash. Any other
         * backslash stands for itself, so a table that already holds one keeps working.
         */
        std::string decodeTableValue(std::string_view value) {
            std::string result;

            for (size_t i = 0; i < value.size(); ) {
                const bool isEscape = value[i] == '\\' && (i + 1) < value.size();

                if (isEscape && value[i + 1] == '\\') {
                    result += '\\';
                    i += 2;
                    continue;
                }

                std::optional<std::string> encoded;
                if (isEscape && value[i + 1] == 'u' && (i + 6) <= value.size()) {
                    u32 codepoint = 0;
                    bool valid = true;
                    for (size_t digit = 0; digit < 4; digit += 1) {
                        const auto hexValue = hexCharToValue(value[i + 2 + digit]);
                        if (!hexValue.has_value()) {
                            valid = false;
                            break;
                        }

                        codepoint = (codepoint << 4) | *hexValue;
                    }

                    // A surrogate half is not a scalar value, so it has no encoding.
                    if (valid && (codepoint < 0xD800 || codepoint > 0xDFFF))
                        encoded = wolv::util::utf32ToUtf8(std::u32string(1, char32_t(codepoint)));
                }

                // Anything unreadable stands for itself, so a stray backslash survives.
                if (!encoded.has_value()) {
                    result += value[i];
                    i += 1;
                    continue;
                }

                result += *encoded;
                i += 6;
            }

            return result;
        }

        /**
         * @brief Lists the directories a table is looked up in by name, in the order they are tried
         *
         * Every encodings search path first, then the "builtin" folder inside each, which holds
         * the tables ImHex-Patterns generates. A table a person puts in an encodings folder
         * itself replaces a built-in table of the same name.
         */
        std::vector<std::fs::path> encodingSearchDirectories() {
            const auto basePaths = paths::Encodings.read();

            std::vector<std::fs::path> result(basePaths.begin(), basePaths.end());
            for (const auto &basePath : basePaths)
                result.push_back(basePath / "builtin");

            return result;
        }

        /**
         * @brief Finds the table file `name` names, if there is one, whatever case it is
         * written in
         *
         * For getEncodingByName(), which looks a table up by the name a person typed, such as
         * through `#pragma encoding`. A `-alias` line uses findEncodingFileByStem() instead,
         * since it names a file, not a name a person typed.
         *
         * Takes `name` as the person wrote it. It rejects a directory part itself, so the
         * lookup stays in the encodings directory whatever encodingFileName() does.
         */
        std::optional<std::fs::path> findEncodingFile(std::string_view name) {
            // A script reaches this with no sandbox prompt, so a directory part reaches no file.
            if (name.find_first_of("/\\") != std::string_view::npos)
                return std::nullopt;

            const auto fileName = encodingFileName(name) + ".tbl";

            for (const auto &directory : encodingSearchDirectories()) {
                auto path = directory / fileName;
                if (std::fs::is_regular_file(path))
                    return path;
            }

            return std::nullopt;
        }

        /**
         * @brief Finds the table file whose stem is exactly `stem`, if there is one
         *
         * A `-alias` line names the file it links to directly, the way a symbolic link names a
         * file, so it takes the stem as written. findEncodingFile() folds case first, since it
         * takes a name a person typed.
         */
        std::optional<std::fs::path> findEncodingFileByStem(std::string_view stem) {
            // A directory part reaches no file, since a stem holds none.
            if (stem.find_first_of("/\\") != std::string_view::npos)
                return std::nullopt;

            const auto fileName = std::string(stem) + ".tbl";

            for (const auto &directory : encodingSearchDirectories()) {
                auto path = directory / fileName;
                if (std::fs::is_regular_file(path))
                    return path;
            }

            return std::nullopt;
        }

        /**
         * @brief Checks whether `path` is still under one of the encodings search paths
         *
         * A "-include ../foo.tbl" line can leave its own folder, but must not leave every
         * encodings folder. A table file can name this path with no sandbox prompt, so this stays
         * as strict as the by-name lookup findEncodingFile() already does.
         */
        bool isWithinEncodingsPaths(const std::fs::path &path) {
            std::error_code error;
            const auto resolved = std::fs::weakly_canonical(path, error);
            if (error)
                return false;

            for (const auto &basePath : paths::Encodings.read()) {
                const auto resolvedBase = std::fs::weakly_canonical(basePath, error);
                if (error)
                    continue;

                const auto relative = resolved.lexically_relative(resolvedBase);
                if (!relative.empty() && !relative.is_absolute() &&
                    std::ranges::none_of(relative, [](const auto &part) { return part == ".."; }))
                    return true;
            }

            return false;
        }

        /**
         * @brief Finds the table file a -include line's relative path names, next to `fromDir`
         *
         * Unlike findEncodingFile(), `relativePath` is used as-is: it names its own extension,
         * instead of always getting ".tbl" appended, and is read relative to the table that
         * includes it, the way one source file includes another.
         */
        std::optional<std::fs::path> findIncludedFile(const std::fs::path &fromDir, std::string_view relativePath) {
            const auto path = (fromDir / std::fs::path(std::string(relativePath))).lexically_normal();
            if (!isWithinEncodingsPaths(path))
                return std::nullopt;

            if (!std::fs::is_regular_file(path))
                return std::nullopt;

            return path;
        }

        /**
         * @brief Reads the table file a -include or -alias line names, from disk
         *
         * A -include line names a relative path with its own extension, such as
         * "includes/box_drawing.tbl", read relative to `context`. Empty `context` instead tries
         * every encodings search path in turn, for a table with no file of its own to be relative
         * to. A -alias line names the stem of the file it links to, such as "iso8859_1", and is
         * looked up the same way regardless of `context`. The two never look alike: only a path
         * has a "." in it.
         */
        std::optional<ResolvedTable> readEncodingFile(const std::fs::path &context, std::string_view name) {
            if (!std::fs::path(std::string(name)).has_extension()) {
                const auto path = findEncodingFileByStem(name);
                if (!path.has_value())
                    return std::nullopt;

                return ResolvedTable{
                    wolv::io::File(*path, wolv::io::File::Mode::Read).readString(),
                    path->parent_path()
                };
            }

            std::optional<std::fs::path> path;
            if (!context.empty()) {
                path = findIncludedFile(context, name);
            } else {
                for (const auto &directory : encodingSearchDirectories()) {
                    if (path = findIncludedFile(directory, name); path.has_value())
                        break;
                }
            }
            if (!path.has_value())
                return std::nullopt;

            return ResolvedTable{
                wolv::io::File(*path, wolv::io::File::Mode::Read).readString(),
                path->parent_path()
            };
        }

        /**
         * @brief Maps the encodingFileName() of every table file's name to that file
         *
         * Read once, on the first name that no file answers to by its own spelling. This finds a
         * table file whose own name is not in lower case, which findEncodingFile() misses on a
         * file system that tells case apart. A base path earlier in the list wins, the same way
         * findEncodingFile() takes the first it finds.
         */
        const std::map<std::string, std::fs::path>& encodingFilesByName() {
            static const auto files = [] {
                std::map<std::string, std::fs::path> result;

                for (const auto &directory : encodingSearchDirectories()) {
                    std::error_code error;
                    for (const auto &entry : std::fs::directory_iterator(directory, error)) {
                        if (entry.path().extension() != ".tbl")
                            continue;

                        result.emplace(encodingFileName(entry.path().stem().string()), entry.path());
                    }
                }

                return result;
            }();

            return files;
        }

        /**
         * @brief Reads what follows a directive, when `line` is that directive
         * @param directive The directive's name, without its leading "-"
         */
        std::optional<std::string_view> directiveArgument(std::string_view line, std::string_view directive) {
            line = wolv::util::trim(line);
            if (!line.starts_with('-'))
                return std::nullopt;
            line.remove_prefix(1);

            if (!line.starts_with(directive))
                return std::nullopt;

            line.remove_prefix(directive.size());

            // A keyword the line only starts with, such as "-includes", is not the directive.
            if (!line.empty() && std::isspace(u8(line.front())) == 0)
                return std::nullopt;

            return wolv::util::trim(line);
        }

        /**
         * @brief Checks whether a line gives a byte sequence a value
         *
         * A directive comes before the first of these, so this line ends the header.
         */
        bool isEntryLine(std::string_view line) {
            const auto delimiterPos = line.find('=');
            if (delimiterPos == std::string_view::npos || delimiterPos == 0)
                return false;

            return !hex::parseByteString(std::string(line.substr(0, delimiterPos))).empty();
        }

        /**
         * @brief Makes a name for a table that gives itself none, from its file's name
         */
        std::string deriveEncodingName(const std::fs::path &path) {
            auto name = wolv::util::replaceStrings(path.stem().string(), "_", " ");

            if (!name.empty())
                name[0] = std::toupper(name[0]);

            return name;
        }

        /**
         * @brief Reads the lines above a table file's first entry, and no more of it
         *
         * Reads a fixed amount of the start of the file. A header is far shorter than that, so
         * a line the read cuts in half belongs to the entries, which this drops anyway.
         */
        std::vector<std::string> readHeaderLines(const std::fs::path &path) {
            constexpr static size_t HeaderReadSize = 4096;

            auto file = wolv::io::File(path, wolv::io::File::Mode::Read);
            const auto content = file.readString(HeaderReadSize);

            std::vector<std::string> result;
            for (const auto &line : wolv::util::splitString(content, "\n")) {
                if (isEntryLine(line))
                    break;

                result.push_back(line);
            }

            return result;
        }

        /**
         * @brief Checks whether a line is a directive, which only the header holds
         */
        bool isDirectiveLine(std::string_view line) {
            return wolv::util::trim(line).starts_with('-');
        }

        /**
         * @brief Checks that a table holds nothing but comments and the one -alias line
         *
         * A line the parser cannot read is a comment. An entry, a second -alias line, or any
         * other directive gives the table contents of its own, which a link must not have.
         */
        bool holdsOnlyAlias(const std::vector<std::string> &lines) {
            size_t aliasCount = 0;

            for (const auto &line : lines) {
                if (directiveArgument(line, "alias").has_value()) {
                    aliasCount += 1;
                    continue;
                }

                if (isEntryLine(line) || isDirectiveLine(line))
                    return false;
            }

            return aliasCount == 1;
        }

        /**
         * @brief Reads the table a -alias line links to, in place of the table that holds it
         *
         * A table with a -alias line holds nothing else. It is only another name for the table
         * it names, the way a symbolic link is another name for a file. A link that breaks or
         * loops, or that a table with contents of its own holds, gives nothing back.
         */
        std::optional<ResolvedTable> followAliases(std::string content, std::fs::path context, const IncludeResolver &resolveInclude) {
            std::set<std::string> visited;

            while (true) {
                const auto lines = wolv::util::splitString(content, "\n");

                std::optional<std::string> target;
                for (const auto &line : lines) {
                    if (isEntryLine(line))
                        break;

                    if (const auto name = directiveArgument(line, "alias"); name.has_value()) {
                        target = std::string(*name);
                        break;
                    }
                }

                if (!target.has_value())
                    return ResolvedTable{ std::move(content), std::move(context) };

                if (!holdsOnlyAlias(lines))
                    return std::nullopt;

                if (!visited.emplace(*target).second)
                    return std::nullopt;

                auto linked = resolveInclude(context, *target);
                if (!linked.has_value())
                    return std::nullopt;

                content = std::move(linked->content);
                context = std::move(linked->context);
            }
        }

    }

    namespace impl {

        void appendEncodingLineStartAddress(std::vector<u64> &lineStartAddresses, size_t line, u64 nextLineStartAddress) {
            if (line + 1 == lineStartAddresses.size())
                lineStartAddresses.push_back(nextLineStartAddress);
        }

    }

    EncodingFile::EncodingFile() :
        m_mapping(std::make_unique<std::map<size_t, std::map<std::vector<u8>, std::string>>>()),
        m_reverseMapping(std::make_unique<std::map<size_t, std::map<std::string, std::vector<u8>, std::less<>>>>()) {

    }

    EncodingFile::EncodingFile(const hex::EncodingFile &other) {
        m_mapping = std::make_unique<std::map<size_t, std::map<std::vector<u8>, std::string>>>(*other.m_mapping);
        m_reverseMapping = std::make_unique<std::map<size_t, std::map<std::string, std::vector<u8>, std::less<>>>>(*other.m_reverseMapping);
        m_longestSequence = other.m_longestSequence;
        m_shortestSequence = other.m_shortestSequence;
        m_ambiguousEncoding = other.m_ambiguousEncoding;
        m_valid = other.m_valid;
        m_name = other.m_name;
        m_description = other.m_description;
    }

    EncodingFile::EncodingFile(EncodingFile &&other) noexcept {
        m_mapping = std::move(other.m_mapping);
        m_reverseMapping = std::move(other.m_reverseMapping);
        m_longestSequence = other.m_longestSequence;
        m_shortestSequence = other.m_shortestSequence;
        m_ambiguousEncoding = other.m_ambiguousEncoding;
        m_valid = other.m_valid;
        m_name = std::move(other.m_name);
        m_description = std::move(other.m_description);
    }

    EncodingFile::EncodingFile(Type type, const std::fs::path &path) : EncodingFile() {
        auto file = wolv::io::File(path, wolv::io::File::Mode::Read);
        switch (type) {
            case Type::Thingy:
                if (!parse(file.readString(), path.parent_path(), readEncodingFile))
                    return;
                break;
            default:
                return;
        }

        // A -name line already named the table. Otherwise the file's own name has to do.
        if (m_name.empty())
            m_name = deriveEncodingName(path);

        m_valid = true;
    }

    EncodingFile::EncodingFile(Type type, const std::string &content, IncludeResolver resolveInclude) : EncodingFile() {
        if (!resolveInclude)
            resolveInclude = readEncodingFile;

        switch (type) {
            case Type::Thingy:
                if (!parse(content, {}, resolveInclude))
                    return;
                break;
            default:
                return;
        }

        if (m_name.empty())
            m_name = "Unknown";
        m_valid = true;
    }


    EncodingFile &EncodingFile::operator=(const hex::EncodingFile &other) {
        if(this == &other) {
            return *this;
        }
        m_mapping = std::make_unique<std::map<size_t, std::map<std::vector<u8>, std::string>>>(*other.m_mapping);
        m_reverseMapping = std::make_unique<std::map<size_t, std::map<std::string, std::vector<u8>, std::less<>>>>(*other.m_reverseMapping);
        m_longestSequence = other.m_longestSequence;
        m_shortestSequence = other.m_shortestSequence;
        m_ambiguousEncoding = other.m_ambiguousEncoding;
        m_valid = other.m_valid;
        m_name = other.m_name;
        m_description = other.m_description;

        return *this;
    }

    EncodingFile &EncodingFile::operator=(EncodingFile &&other) noexcept {
        m_mapping = std::move(other.m_mapping);
        m_reverseMapping = std::move(other.m_reverseMapping);
        m_longestSequence = other.m_longestSequence;
        m_shortestSequence = other.m_shortestSequence;
        m_ambiguousEncoding = other.m_ambiguousEncoding;
        m_valid = other.m_valid;
        m_name = std::move(other.m_name);
        m_description = std::move(other.m_description);

        return *this;
    }



    std::optional<std::pair<std::string_view, size_t>> EncodingFile::lookup(std::span<const u8> buffer) const {
        for (const auto &[size, mapping] : std::ranges::reverse_view(*m_mapping)) {
            if (size > buffer.size()) continue;

            std::vector key(buffer.begin(), buffer.begin() + size);
            if (const auto entry = mapping.find(key); entry != mapping.end())
                return std::pair<std::string_view, size_t>{ entry->second, size };
        }

        return std::nullopt;
    }

    std::pair<std::string_view, size_t> EncodingFile::getEncodingFor(std::span<const u8> buffer) const {
        return this->lookup(buffer).value_or(std::pair<std::string_view, size_t>{ ".", 1 });
    }

    u64 EncodingFile::getEncodingLengthFor(std::span<u8> buffer) const {
        for (const auto& [size, mapping] : std::ranges::reverse_view(*m_mapping)) {
            if (size > buffer.size()) continue;

            std::vector key(buffer.begin(), buffer.begin() + size);
            if (mapping.contains(key))
                return size;
        }

        return 1;
    }

    std::string EncodingFile::decodeAll(std::span<const u8> buffer) const {
        std::string result;

        while (!buffer.empty()) {
            const auto [character, size] = getEncodingFor(buffer);
            result += character;
            buffer = buffer.subspan(size);
        }

        return result;
    }

    pl::core::DecodeResult EncodingFile::decodeBounded(std::span<const u8> buffer, std::optional<size_t> maxCodepoints) const {
        pl::core::DecodeResult result;

        while (!buffer.empty()) {
            if (maxCodepoints.has_value() && result.codepointCount >= *maxCodepoints) {
                result.stopReason = pl::core::DecodeStop::CodepointLimit;
                return result;
            }

            // Too short for even the shortest mapped sequence.
            if (buffer.size() < m_shortestSequence) {
                result.stopReason = pl::core::DecodeStop::EndOfInput;
                return result;
            }

            const auto entry = lookup(buffer);
            if (!entry.has_value()) {
                result.stopReason = pl::core::DecodeStop::MalformedBytes;
                return result;
            }

            result.text += entry->first;
            result.bytesConsumed += entry->second;
            result.codepointCount += 1;
            buffer = buffer.subspan(entry->second);
        }

        result.stopReason = pl::core::DecodeStop::EndOfInput;
        return result;
    }

    bool EncodingFile::isFullyMapped(std::span<const u8> buffer) const {
        while (!buffer.empty()) {
            const auto entry = lookup(buffer);
            if (!entry.has_value())
                return false;

            buffer = buffer.subspan(entry->second);
        }

        return true;
    }

    std::optional<std::pair<std::vector<u8>, size_t>> EncodingFile::getBytesFor(std::string_view sequence) const {
        for (const auto &[size, mapping] : std::ranges::reverse_view(*m_reverseMapping)) {
            if (size > sequence.size()) continue;

            auto key = sequence.substr(0, size);
            auto iter = mapping.find(key);
            if (iter != mapping.end())
                return std::pair { iter->second, size };
        }

        return std::nullopt;
    }

    std::optional<std::vector<u8>> EncodingFile::encodeAll(std::string_view sequence) const {
        if (!canEncode())
            return std::nullopt;

        std::vector<u8> result;

        while (!sequence.empty()) {
            auto match = getBytesFor(sequence);
            if (!match.has_value())
                return std::nullopt;

            const auto &[bytes, size] = match.value();
            result.insert(result.end(), bytes.begin(), bytes.end());
            sequence = sequence.substr(size);
        }

        return result;
    }


    bool EncodingFile::parse(const std::string &content, const std::fs::path &context, const IncludeResolver &resolveInclude) {
        const auto linked = followAliases(content, context, resolveInclude);
        if (!linked.has_value())
            return false;

        std::set<std::string> includedNames;

        // Every decoded value so far. A repeat makes the encoding ambiguous.
        std::vector<std::string_view> encodedValues;

        if (!parseTable(linked->content, linked->context, resolveInclude, includedNames, encodedValues, true))
            return false;

        // Prefix-free is sufficient, not necessary; the full test is Sardinas-Patterson.
        if (!m_ambiguousEncoding) {
            std::ranges::sort(encodedValues);
            for (size_t i = 1; i < encodedValues.size(); i++) {
                if (encodedValues[i].starts_with(encodedValues[i - 1])) {
                    m_ambiguousEncoding = true;
                    break;
                }
            }
        }

        return true;
    }

    bool EncodingFile::parseTable(std::string_view content, const std::fs::path &context, const IncludeResolver &resolveInclude,
        std::set<std::string> &includedNames, std::vector<std::string_view> &encodedValues, bool topLevel) {
        std::vector<std::string> includeDirectives;
        bool inHeader = true;

        for (const auto &line : wolv::util::splitString(std::string(content), "\n")) {
            if (inHeader && isEntryLine(line))
                inHeader = false;

            if (inHeader) {
                if (const auto name = directiveArgument(line, "include"); name.has_value()) {
                    includeDirectives.emplace_back(*name);
                    continue;
                }

                if (topLevel) {
                    if (const auto name = directiveArgument(line, "name"); name.has_value() && m_name.empty())
                        m_name = *name;

                    if (const auto description = directiveArgument(line, "description"); description.has_value() && m_description.empty())
                        m_description = *description;
                }

                continue;
            }

            // Only the header holds a directive.
            if (isDirectiveLine(line))
                return false;

            std::string from, to;
            {
                auto delimiterPos = line.find('=');

                if (delimiterPos >= line.length())
                    continue;

                from = line.substr(0, delimiterPos);
                to   = line.substr(delimiterPos + 1);

                if (from.empty()) continue;
            }

            auto fromBytes = hex::parseByteString(from);
            if (fromBytes.empty()) continue;

            // The first line for a byte sequence wins, so an include cannot replace an entry.
            if ((*m_mapping)[fromBytes.size()].contains(fromBytes))
                continue;

            if (to.length() > 1)
                to = wolv::util::trim(to);
            to = decodeTableValue(to);
            if (to.empty())
                to = " ";

            if (!m_mapping->contains(fromBytes.size()))
                m_mapping->insert({ fromBytes.size(), {} });

            u64 keySize = fromBytes.size();
            u64 valueSize = to.size();

            if (!m_reverseMapping->contains(valueSize))
                m_reverseMapping->insert({ valueSize, {} });

            auto &reverseBucket = (*m_reverseMapping)[valueSize];
            auto existingEntry = reverseBucket.find(to);
            if (existingEntry == reverseBucket.end()) {
                auto iter = reverseBucket.emplace(to, fromBytes).first;
                encodedValues.emplace_back(iter->first);
            } else if (existingEntry->second != fromBytes) {
                // Two byte sequences that give one value conflict. An identical repeated line does not.
                m_ambiguousEncoding = true;
            }

            (*m_mapping)[keySize].insert({ std::move(fromBytes), to });

            m_longestSequence = std::max(m_longestSequence, keySize);
            m_shortestSequence = std::min(m_shortestSequence, keySize);
        }

        // This table's own entries go in first, so an include cannot replace one of its own.
        for (const auto &name : includeDirectives) {
            if (!includedNames.emplace(name).second)
                continue;

            const auto includedTable = resolveInclude(context, name);
            if (!includedTable.has_value())
                continue;

            if (!parseTable(includedTable->content, includedTable->context, resolveInclude, includedNames, encodedValues, false))
                return false;
        }

        return true;
    }


    std::string encodingFileName(std::string_view name) {
        std::string result(name);

        for (auto &character : result)
            character = char(std::tolower(u8(character)));

        return result;
    }

    const EncodingFile* getEncodingByName(const std::string &name) {
        static std::mutex mutex;
        static std::map<std::string, EncodingFile> encodings;

        std::scoped_lock lock(mutex);

        // Every way of writing one name reaches one file, so one entry serves them all.
        const auto fileName = encodingFileName(name);

        if (const auto entry = encodings.find(fileName); entry != encodings.end())
            return entry->second.valid() ? &entry->second : nullptr;

        auto path = findEncodingFile(name);

        // A file named in any other way answers too, which costs one read of the directory.
        if (!path.has_value()) {
            const auto &files = encodingFilesByName();
            if (const auto file = files.find(fileName); file != files.end())
                path = file->second;
        }

        EncodingFile encoding;
        if (path.has_value())
            encoding = EncodingFile(EncodingFile::Type::Thingy, *path);

        // A failed lookup is cached too, so a bad name hits the file system once.
        const auto &result = encodings.emplace(fileName, std::move(encoding)).first->second;
        return result.valid() ? &result : nullptr;
    }

    EncodingHeader readEncodingHeader(const std::fs::path &path) {
        std::set<std::string> visited;
        auto current = path;
        bool isAlias = false;

        while (true) {
            EncodingHeader header;
            header.isAlias = isAlias;

            std::optional<std::string> alias;

            for (const auto &line : readHeaderLines(current)) {
                if (const auto target = directiveArgument(line, "alias"); target.has_value() && !alias.has_value())
                    alias = std::string(*target);

                if (const auto name = directiveArgument(line, "name"); name.has_value() && header.name.empty())
                    header.name = *name;

                if (const auto description = directiveArgument(line, "description"); description.has_value() && header.description.empty())
                    header.description = *description;
            }

            // A table with a -alias line holds nothing else, so the table it links to answers.
            if (!alias.has_value()) {
                if (header.name.empty())
                    header.name = deriveEncodingName(path);

                header.path = current;
                return header;
            }

            // A link that breaks or loops reaches no table, so it keeps an empty path.
            isAlias = true;

            EncodingHeader broken;
            broken.isAlias = true;

            if (!visited.emplace(*alias).second)
                return broken;

            const auto next = findEncodingFileByStem(*alias);
            if (!next.has_value())
                return broken;

            current = *next;
        }
    }

}
