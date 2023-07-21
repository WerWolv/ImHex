#include "content/command_line_interface.hpp"

#include <hex/api/imhex_api.hpp>
#include <hex/api/event.hpp>

#include <hex/helpers/fmt.hpp>

#include <hex/helpers/magic.hpp>
#include <hex/helpers/crypto.hpp>
#include <hex/helpers/literals.hpp>
#include <romfs/romfs.hpp>

#include <hex/api/plugin_manager.hpp>
#include <hex/subcommands/subcommands.hpp>

#include <wolv/utils/string.hpp>

#include "content/helpers/math_evaluator.hpp"

#include <pl/cli/cli.hpp>

namespace hex::plugin::builtin {

    using namespace hex::literals;

    void handleVersionCommand(const std::vector<std::string> &args) {
        hex::unused(args);

        hex::print(romfs::get("logo.ans").string(),
                   ImHexApi::System::getImHexVersion(),
                   ImHexApi::System::getCommitBranch(), ImHexApi::System::getCommitHash(),
                   __DATE__, __TIME__,
                   ImHexApi::System::isPortableVersion() ? "Portable" : "Installed");

        std::exit(EXIT_SUCCESS);
    }

    void handleHelpCommand(const std::vector<std::string> &args) {
        hex::unused(args);

        hex::print(
                "ImHex - A Hex Editor for Reverse Engineers, Programmers and people who value their retinas when working at 3 AM.\n"
                "\n"
                "usage: imhex [subcommand] [options]\n"
                "Available subcommands:\n"
        );

        size_t longestCommand = 0;
        for (const auto &plugin : PluginManager::getPlugins()) {
            for (const auto &subCommand : plugin.getSubCommands()) {
                longestCommand = std::max(longestCommand, subCommand.commandKey.size());
            }
        }

        for (const auto &plugin : PluginManager::getPlugins()) {
            for (const auto &subCommand : plugin.getSubCommands()) {
                hex::println("    --{}{: <{}}        {}", subCommand.commandKey, "", longestCommand - subCommand.commandKey.size(), subCommand.commandDesc);
            }
        }

        std::exit(EXIT_SUCCESS);
    }

    void handleOpenCommand(const std::vector<std::string> &args) {
        if (args.empty()) {
            hex::println("No files provided to open.");
            std::exit(EXIT_FAILURE);
        }

        std::vector<std::string> fullPaths;
        bool doubleDashFound = false;
        for (auto &arg : args) {

            // Skip the first argument named `--`
            if (arg == "--" && !doubleDashFound) {
                doubleDashFound = true;
            } else {
                auto path = std::filesystem::weakly_canonical(arg);
                fullPaths.push_back(wolv::util::toUTF8String(path));
            }
        }

        hex::subcommands::forwardSubCommand("open", fullPaths);
    }

    void handleCalcCommand(const std::vector<std::string> &args) {
        if (args.empty()) {
            hex::println("No expression provided!");
            hex::println("Example: imhex --calc \"5 * 7\"");
            std::exit(EXIT_FAILURE);
        }

        MathEvaluator<long double> evaluator;

        auto input = hex::format("{}", fmt::join(args, " "));
        auto result = evaluator.evaluate(input);

        if (!result.has_value())
            hex::println("{}\n> '{}'", evaluator.getLastError().value(), input);
        else
            hex::println("{}", result.value());

        std::exit(EXIT_SUCCESS);
    }

    void handlePluginsCommand(const std::vector<std::string> &args) {
        hex::unused(args);

        hex::println("Loaded plugins:");

        for (const auto &plugin : PluginManager::getPlugins()) {
            hex::print("- ");

            if (plugin.isBuiltinPlugin())
                hex::print("\033[1;43m{}\033[0m", plugin.getPluginName());
            else
                hex::print("\033[1m{}\033[0m", plugin.getPluginName());

            hex::println(" by {}", plugin.getPluginAuthor());

            hex::println("  \033[2;3m{}\033[0m", plugin.getPluginDescription());
        }

        std::exit(EXIT_SUCCESS);
    }

    void handleHashCommand(const std::vector<std::string> &args) {
        if (args.size() != 2) {
            hex::println("usage: imhex --hash <algorithm> <file>");
            hex::println("Available algorithms: md5, sha1, sha224, sha256, sha384, sha512");
            std::exit(EXIT_FAILURE);
        }

        const auto &algorithm  = args[0];
        const auto &filePath   = std::fs::path(args[1]);

        wolv::io::File file(filePath, wolv::io::File::Mode::Read);
        if (!file.isValid()) {
            hex::println("Failed to open file: {}", wolv::util::toUTF8String(filePath));
            std::exit(EXIT_FAILURE);
        }

        constexpr static auto toVector = [](const auto &data) {
            return std::vector<u8>(data.begin(), data.end());
        };

        std::vector<u8> result;
        if (algorithm == "md5") {
            result = toVector(hex::crypt::md5(file.readVector()));
        } else if (algorithm == "sha1") {
            result = toVector(hex::crypt::sha1(file.readVector()));
        } else if (algorithm == "sha224") {
            result = toVector(hex::crypt::sha224(file.readVector()));
        } else if (algorithm == "sha256") {
            result = toVector(hex::crypt::sha256(file.readVector()));
        } else if (algorithm == "sha384") {
            result = toVector(hex::crypt::sha384(file.readVector()));
        } else if (algorithm == "sha512") {
            result = toVector(hex::crypt::sha512(file.readVector()));
        } else {
            hex::println("Unknown algorithm: {}", algorithm);
            hex::println("Available algorithms: md5, sha1, sha224, sha256, sha384, sha512");
            std::exit(EXIT_FAILURE);
        }

        hex::println("{}({}) = {}", algorithm, wolv::util::toUTF8String(filePath.filename()), hex::crypt::encode16(result));

        std::exit(EXIT_SUCCESS);
    }

    void handleEncodeCommand(const std::vector<std::string> &args) {
        if (args.size() != 2) {
            hex::println("usage: imhex --encode <algorithm> <string>");
            hex::println("Available algorithms: base64, hex");
            std::exit(EXIT_FAILURE);
        }

        const auto &algorithm  = args[0];
        const auto &data       = std::vector<u8>(args[1].begin(), args[1].end());

        std::string result;
        if (algorithm == "base64") {
            auto base64 = hex::crypt::encode64(data);
            result = std::string(base64.begin(), base64.end());
        } else if (algorithm == "hex") {
            result = hex::crypt::encode16(data);
        } else {
            hex::println("Unknown algorithm: {}", algorithm);
            hex::println("Available algorithms: base64, hex");
            std::exit(EXIT_FAILURE);
        }

        hex::println("encode_{}({}) = {}", algorithm, args[1], result);
        std::exit(EXIT_SUCCESS);
    }

    void handleDecodeCommand(const std::vector<std::string> &args) {
        if (args.size() != 2) {
            hex::println("usage: imhex --decode <algorithm> <string>");
            hex::println("Available algorithms: base64, hex");
            std::exit(EXIT_FAILURE);
        }

        const auto &algorithm  = args[0];
        const auto &data       = std::vector<u8>(args[1].begin(), args[1].end());

        std::string result;
        if (algorithm == "base64") {
            auto base64 = hex::crypt::decode64(data);
            result = std::string(base64.begin(), base64.end());
        } else if (algorithm == "hex") {
            auto base16 = hex::crypt::decode16(std::string(data.begin(), data.end()));
            result = std::string(base16.begin(), base16.end());
        } else {
            hex::println("Unknown algorithm: {}", algorithm);
            hex::println("Available algorithms: base64, hex");
            std::exit(EXIT_FAILURE);
        }

        hex::print("decode_{}({}) = {}", algorithm, args[1], result);
        std::exit(EXIT_SUCCESS);
    }

    void handleMagicCommand(const std::vector<std::string> &args) {
        if (args.size() != 2) {
            hex::println("usage: imhex --magic <operation> <file>");
            hex::println("Available operations: mime, desc");
            std::exit(EXIT_FAILURE);
        }

        if (!magic::compile()) {
            hex::print("Failed to compile magic database!");
            std::exit(EXIT_FAILURE);
        }

        const auto &operation  = args[0];
        const auto &filePath   = std::fs::path(args[1]);

        wolv::io::File file(filePath, wolv::io::File::Mode::Read);
        if (!file.isValid()) {
            hex::println("Failed to open file: {}", wolv::util::toUTF8String(filePath));
            std::exit(EXIT_FAILURE);
        }

        auto data = file.readVector(std::min<size_t>(file.getSize(), 100_KiB));

        if (operation == "mime") {
            auto result = magic::getMIMEType(data);
            hex::println("{}", result);
        } else if (operation == "desc") {
            auto result = magic::getDescription(data);
            hex::println("{}", result);
        } else {
            hex::println("Unknown operation: {}", operation);
            hex::println("Available operations: mime, desc");
            std::exit(EXIT_FAILURE);
        }

        std::exit(EXIT_SUCCESS);
    }

    void handlePatternLanguageCommand(const std::vector<std::string> &args) {
        auto processedArgs = args;
        processedArgs.insert(processedArgs.begin(), "imhex");

        std::exit(pl::cli::executeCommandLineInterface(processedArgs));
    }


    void registerCommandForwarders() {
        hex::subcommands::registerSubCommand("open", [](const std::vector<std::string> &args){
            for (auto &arg : args) {
                EventManager::post<RequestOpenFile>(arg);
            }
        });
    }

}