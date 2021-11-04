#include <hex/api/content_registry.hpp>

#include <hex/helpers/shared_data.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/helpers/net.hpp>
#include <hex/helpers/file.hpp>

#include <hex/pattern_language/token.hpp>
#include <hex/pattern_language/log_console.hpp>
#include <hex/pattern_language/evaluator.hpp>
#include <hex/pattern_language/pattern_data.hpp>

#include <vector>

#include <fmt/args.h>

namespace hex::plugin::builtin {

    std::string format(pl::Evaluator *ctx, auto params) {
        auto format = pl::Token::literalToString(params[0], true);
        std::string message;

        fmt::dynamic_format_arg_store<fmt::format_context> formatArgs;

        for (u32 i = 1; i < params.size(); i++) {
            auto &param = params[i];

            std::visit(overloaded {
                [&](pl::PatternData* value) {
                    formatArgs.push_back(value->toString(ctx->getProvider()));
                },
                [&](auto &&value) {
                    formatArgs.push_back(value);
                }
            }, param);
        }

        try {
            return fmt::vformat(format, formatArgs);
        } catch (fmt::format_error &error) {
            hex::pl::LogConsole::abortEvaluation(hex::format("format error: {}", error.what()));
        }
    }

    void registerPatternLanguageFunctions() {
        using namespace hex::pl;

        ContentRegistry::PatternLanguageFunctions::Namespace nsStd = { "std" };
        {

            /* assert(condition, message) */
            ContentRegistry::PatternLanguageFunctions::add(nsStd, "assert", 2, [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                auto condition = Token::literalToBoolean(params[0]);
                auto message = std::get<std::string>(params[1]);

                if (!condition)
                    LogConsole::abortEvaluation(hex::format("assertion failed \"{0}\"", message));

                return std::nullopt;
            });

            /* assert_warn(condition, message) */
            ContentRegistry::PatternLanguageFunctions::add(nsStd, "assert_warn", 2, [](auto *ctx, auto params) -> std::optional<Token::Literal> {
                auto condition = Token::literalToBoolean(params[0]);
                auto message = std::get<std::string>(params[1]);

                if (!condition)
                    ctx->getConsole().log(LogConsole::Level::Warning, hex::format("assertion failed \"{0}\"", message));

                return std::nullopt;
            });

            /* print(format, args...) */
            ContentRegistry::PatternLanguageFunctions::add(nsStd, "print", ContentRegistry::PatternLanguageFunctions::MoreParametersThan | 0, [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                ctx->getConsole().log(LogConsole::Level::Info, format(ctx, params));

                return std::nullopt;
            });

            /* format(format, args...) */
            ContentRegistry::PatternLanguageFunctions::add(nsStd, "format", ContentRegistry::PatternLanguageFunctions::MoreParametersThan | 0, [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                return format(ctx, params);
            });

        }

        ContentRegistry::PatternLanguageFunctions::Namespace nsStdMem = { "std", "mem" };
        {

            /* align_to(alignment, value) */
            ContentRegistry::PatternLanguageFunctions::add(nsStdMem, "align_to", 2, [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                auto alignment = Token::literalToUnsigned(params[0]);
                auto value = Token::literalToUnsigned(params[1]);

                u128 remainder = value % alignment;

                return remainder != 0 ? value + (alignment - remainder) : value;
            });

            /* base_address() */
            ContentRegistry::PatternLanguageFunctions::add(nsStdMem, "base_address", ContentRegistry::PatternLanguageFunctions::NoParameters, [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                return u128(ctx->getProvider()->getBaseAddress());
            });

            /* size() */
            ContentRegistry::PatternLanguageFunctions::add(nsStdMem, "size", ContentRegistry::PatternLanguageFunctions::NoParameters, [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                return u128(ctx->getProvider()->getActualSize());
            });

            /* find_sequence(occurrence_index, bytes...) */
            ContentRegistry::PatternLanguageFunctions::add(nsStdMem, "find_sequence", ContentRegistry::PatternLanguageFunctions::MoreParametersThan | 1, [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                auto occurrenceIndex = Token::literalToUnsigned(params[0]);

                std::vector<u8> sequence;
                for (u32 i = 1; i < params.size(); i++) {
                    auto byte = Token::literalToUnsigned(params[i]);

                    if (byte > 0xFF)
                        LogConsole::abortEvaluation(hex::format("byte #{} value out of range: {} > 0xFF", i, u64(byte)));

                    sequence.push_back(u8(byte & 0xFF));
                }

                std::vector<u8> bytes(sequence.size(), 0x00);
                u32 occurrences = 0;
                for (u64 offset = 0; offset < ctx->getProvider()->getSize() - sequence.size(); offset++) {
                    ctx->getProvider()->read(offset, bytes.data(), bytes.size());

                    if (bytes == sequence) {
                        if (occurrences < occurrenceIndex) {
                            occurrences++;
                            continue;
                        }

                        return u128(offset);
                    }
                }

                LogConsole::abortEvaluation("failed to find sequence");
            });

            /* read_unsigned(address, size) */
            ContentRegistry::PatternLanguageFunctions::add(nsStdMem, "read_unsigned", 2, [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                auto address = Token::literalToUnsigned(params[0]);
                auto size = Token::literalToUnsigned(params[1]);

                if (size > 16)
                    LogConsole::abortEvaluation("read size out of range");

                u128 result = 0;
                ctx->getProvider()->read(address, &result, size);

                return result;
            });

            /* read_signed(address, size) */
            ContentRegistry::PatternLanguageFunctions::add(nsStdMem, "read_signed", 2, [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                auto address = Token::literalToUnsigned(params[0]);
                auto size = Token::literalToUnsigned(params[1]);

                if (size > 16)
                    LogConsole::abortEvaluation("read size out of range");

                s128 value;
                ctx->getProvider()->read(address, &value, size);
                return hex::signExtend(size * 8, value);
            });

            /* read_string(address, size) */
            ContentRegistry::PatternLanguageFunctions::add(nsStdMem, "read_string", 2, [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                auto address = Token::literalToUnsigned(params[0]);
                auto size = Token::literalToUnsigned(params[1]);

                std::string result(size, '\x00');
                ctx->getProvider()->read(address, result.data(), size);

                return result;
            });

        }

        ContentRegistry::PatternLanguageFunctions::Namespace nsStdString = { "std", "string" };
        {
            /* length(string) */
            ContentRegistry::PatternLanguageFunctions::add(nsStdString, "length", 1, [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                auto string = Token::literalToString(params[0], false);

                return u128(string.length());
            });

            /* at(string, index) */
            ContentRegistry::PatternLanguageFunctions::add(nsStdString, "at", 2, [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                auto string = Token::literalToString(params[0], false);
                auto index = Token::literalToSigned(params[1]);

#if defined(OS_MACOS)
                const auto signIndex = index >> (sizeof(index) * 8 - 1);
                const auto absIndex = (index ^ signIndex) - signIndex;
#else
                const auto absIndex = std::abs(index);
#endif

                if (absIndex > string.length())
                    LogConsole::abortEvaluation("character index out of range");

                if (index >= 0)
                    return char(string[index]);
                else
                    return char(string[string.length() - -index]);
            });

            /* substr(string, pos, count) */
            ContentRegistry::PatternLanguageFunctions::add(nsStdString, "substr", 3, [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                auto string = Token::literalToString(params[0], false);
                auto pos = Token::literalToUnsigned(params[1]);
                auto size = Token::literalToUnsigned(params[2]);

                if (pos > string.length())
                    LogConsole::abortEvaluation("character index out of range");

                return string.substr(pos, size);
            });

            /* parse_int(string, base) */
            ContentRegistry::PatternLanguageFunctions::add(nsStdString, "parse_int", 2, [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                auto string = Token::literalToString(params[0], false);
                auto base = Token::literalToUnsigned(params[1]);

                return s128(std::strtoll(string.c_str(), nullptr, base));
            });

            /* parse_float(string) */
            ContentRegistry::PatternLanguageFunctions::add(nsStdString, "parse_float", 1, [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                auto string = Token::literalToString(params[0], false);

                return double(std::strtod(string.c_str(), nullptr));
            });

        }

        ContentRegistry::PatternLanguageFunctions::Namespace nsStdHttp = { "std", "http" };
        {
            /* get(url) */
            ContentRegistry::PatternLanguageFunctions::add(nsStdHttp, "get", 1, [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                const auto url = Token::literalToString(params[0], false);

                hex::Net net;
                return net.getString(url).get().body;
            });
        }


        ContentRegistry::PatternLanguageFunctions::Namespace nsStdFile = { "std", "file" };
        {

            static u32 fileCounter = 0;
            static std::map<u32, File> openFiles;

            /* open(path, mode) */
            ContentRegistry::PatternLanguageFunctions::add(nsStdFile, "open", 2, [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                const auto path = Token::literalToString(params[0], false);
                const auto modeEnum = Token::literalToUnsigned(params[1]);

                File::Mode mode;
                switch (modeEnum) {
                    case 1: mode = File::Mode::Read; break;
                    case 2: mode = File::Mode::Write; break;
                    case 3: mode = File::Mode::Create; break;
                    default:
                        LogConsole::abortEvaluation("invalid file open mode");
                }

                auto file = File(path, mode);

                if (!file.isValid())
                    LogConsole::abortEvaluation(hex::format("failed to open file {}", path));

                fileCounter++;
                openFiles.emplace(std::pair{ fileCounter, std::move(file) });

                return u128(fileCounter);
            });

            /* close(file) */
            ContentRegistry::PatternLanguageFunctions::add(nsStdFile, "close", 1, [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                const auto file = Token::literalToUnsigned(params[0]);

                if (!openFiles.contains(file))
                    LogConsole::abortEvaluation("failed to access invalid file");

                openFiles.erase(file);

                return { };
            });

            /* read(file, size) */
            ContentRegistry::PatternLanguageFunctions::add(nsStdFile, "read", 2, [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                const auto file = Token::literalToUnsigned(params[0]);
                const auto size = Token::literalToUnsigned(params[1]);

                if (!openFiles.contains(file))
                    LogConsole::abortEvaluation("failed to access invalid file");

                return openFiles[file].readString(size);
            });

            /* write(file, data) */
            ContentRegistry::PatternLanguageFunctions::add(nsStdFile, "write", 2, [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                const auto file = Token::literalToUnsigned(params[0]);
                const auto data = Token::literalToString(params[1], true);

                if (!openFiles.contains(file))
                    LogConsole::abortEvaluation("failed to access invalid file");

                openFiles[file].write(data);

                return { };
            });

            /* seek(file, offset) */
            ContentRegistry::PatternLanguageFunctions::add(nsStdFile, "seek", 2, [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                const auto file = Token::literalToUnsigned(params[0]);
                const auto offset = Token::literalToUnsigned(params[1]);

                if (!openFiles.contains(file))
                    LogConsole::abortEvaluation("failed to access invalid file");

                openFiles[file].seek(offset);

                return { };
            });

            /* size(file) */
            ContentRegistry::PatternLanguageFunctions::add(nsStdFile, "size", 1, [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                const auto file = Token::literalToUnsigned(params[0]);

                if (!openFiles.contains(file))
                    LogConsole::abortEvaluation("failed to access invalid file");

                return u128(openFiles[file].getSize());
            });

            /* resize(file, size) */
            ContentRegistry::PatternLanguageFunctions::add(nsStdFile, "resize", 2, [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                const auto file = Token::literalToUnsigned(params[0]);
                const auto size = Token::literalToUnsigned(params[1]);

                if (!openFiles.contains(file))
                    LogConsole::abortEvaluation("failed to access invalid file");

                openFiles[file].setSize(size);

                return { };
            });

            /* flush(file) */
            ContentRegistry::PatternLanguageFunctions::add(nsStdFile, "flush", 1, [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                const auto file = Token::literalToUnsigned(params[0]);

                if (!openFiles.contains(file))
                    LogConsole::abortEvaluation("failed to access invalid file");

                openFiles[file].flush();

                return { };
            });

            /* remove(file) */
            ContentRegistry::PatternLanguageFunctions::add(nsStdFile, "remove", 1, [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                const auto file = Token::literalToUnsigned(params[0]);

                if (!openFiles.contains(file))
                    LogConsole::abortEvaluation("failed to access invalid file");

                openFiles[file].remove();

                return { };
            });
        }
    }

}
