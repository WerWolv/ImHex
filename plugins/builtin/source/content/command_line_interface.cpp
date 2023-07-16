#include "content/command_line_interface.hpp"

#include <hex/api/imhex_api.hpp>
#include <hex/api/event.hpp>

#include <hex/helpers/fmt.hpp>
#include <fmt/ranges.h>

#include <hex/helpers/crypto.hpp>
#include <romfs/romfs.hpp>

#include <hex/api/plugin_manager.hpp>
#include <hex/subcommands/subcommands.hpp>

#include <wolv/utils/string.hpp>

#include "content/helpers/math_evaluator.hpp"

namespace hex::plugin::builtin {

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
                hex::print("    --{}{: <{}}        {}\n", subCommand.commandKey, "", longestCommand - subCommand.commandKey.size(), subCommand.commandDesc);
            }
        }

        std::exit(EXIT_SUCCESS);
    }

    void handleOpenCommand(const std::vector<std::string> &args) {
        if (args.empty()) {
            hex::print("No files provided to open.");
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
            hex::print("No expression provided!");
            std::exit(EXIT_FAILURE);
        }

        MathEvaluator<long double> evaluator;

        auto input = hex::format("{}", fmt::join(args, " "));
        auto result = evaluator.evaluate(input);

        if (!result.has_value())
            hex::print("{}\n> '{}'", evaluator.getLastError().value(), input);
        else
            hex::print("{}", result.value());

        std::exit(EXIT_SUCCESS);
    }

    void handlePluginsCommand(const std::vector<std::string> &args) {
        hex::unused(args);

        hex::print("Loaded plugins:\n");

        for (const auto &plugin : PluginManager::getPlugins()) {
            hex::print("- ");

            if (plugin.isBuiltinPlugin())
                hex::print("{}", fmt::styled(plugin.getPluginName(), fmt::emphasis::bold | fmt::bg(fmt::terminal_color::yellow)));
            else
                hex::print("{}", fmt::styled(plugin.getPluginName(), fmt::emphasis::bold));

            hex::print(" by {}\n", plugin.getPluginAuthor());

            hex::print("  {}\n", fmt::styled(plugin.getPluginDescription(), fmt::emphasis::italic | fmt::emphasis::faint));
        }

        std::exit(EXIT_SUCCESS);
    }

    void handleHashCommand(const std::vector<std::string> &args) {
        if (args.size() != 2) {
            hex::print("usage: imhex --hash <algorithm> <file>");
            std::exit(EXIT_FAILURE);
        }

        const auto &algorithm  = args[0];
        const auto &filePath   = std::fs::path(args[1]);

        wolv::io::File file(filePath, wolv::io::File::Mode::Read);
        if (!file.isValid()) {
            hex::print("Failed to open file: {}", wolv::util::toUTF8String(filePath));
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
            hex::print("Unknown algorithm: {}", algorithm);
            hex::print("Available algorithms: md5, sha1, sha224, sha256, sha384, sha512");
            std::exit(EXIT_FAILURE);
        }

        hex::print("{}({}) = {}", algorithm, wolv::util::toUTF8String(filePath.filename()), hex::crypt::encode16(result));

        std::exit(EXIT_SUCCESS);
    }

    void handleEncodeCommand(const std::vector<std::string> &args) {
        if (args.size() != 2) {
            hex::print("usage: imhex --encode <algorithm> <string>");
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
            hex::print("Unknown algorithm: {}", algorithm);
            hex::print("Available algorithms: base64, hex");
            std::exit(EXIT_FAILURE);
        }

        hex::print("encode_{}({}) = {}", algorithm, args[1], result);
        std::exit(EXIT_SUCCESS);
    }

    void handleDecodeCommand(const std::vector<std::string> &args) {
        if (args.size() != 2) {
            hex::print("usage: imhex --decode <algorithm> <string>");
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
            hex::print("Unknown algorithm: {}", algorithm);
            hex::print("Available algorithms: base64, hex");
            std::exit(EXIT_FAILURE);
        }

        hex::print("decode_{}({}) = {}", algorithm, args[1], result);
        std::exit(EXIT_SUCCESS);
    }


    void registerCommandForwarders() {
        hex::subcommands::registerSubCommand("open", [](const std::vector<std::string> &args){
            for (auto &arg : args) {
                EventManager::post<RequestOpenFile>(arg);
            }
        });
    }

}