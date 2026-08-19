#include <content/command_line_interface.hpp>

#include <hex/api/imhex_api/system.hpp>
#include <hex/api/imhex_api/hex_editor.hpp>
#include <hex/api/http/github_api.hpp>
#include <hex/api/content_registry/settings.hpp>
#include <hex/api/content_registry/views.hpp>
#include <hex/api/content_registry/pattern_language.hpp>
#include <hex/api/events/requests_interaction.hpp>
#include <hex/api/events/requests_gui.hpp>
#include <hex/api/events/events_gui.hpp>
#include <hex/api/plugin_manager.hpp>
#include <hex/api/task_manager.hpp>

#include <hex/helpers/fmt.hpp>
#include <hex/helpers/magic.hpp>
#include <hex/helpers/crypto.hpp>
#include <hex/helpers/literals.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/helpers/default_paths.hpp>
#include <hex/helpers/debugging.hpp>

#include <hex/subcommands/subcommands.hpp>
#include <hex/trace/stacktrace.hpp>
#include <hex/mcp/client.hpp>

#include <romfs/romfs.hpp>
#include <wolv/utils/string.hpp>
#include <wolv/math_eval/math_evaluator.hpp>

#include <pl/cli/cli.hpp>

#include <content/providers/file_provider.hpp>
#include <content/views/fullscreen/view_fullscreen_save_editor.hpp>
#include <content/views/fullscreen/view_fullscreen_file_info.hpp>

#include <iostream>
#include <span>

namespace hex::plugin::builtin {
    using namespace hex::literals;

    int handleVersionCommand(std::span<const std::string> args) {
        std::ignore = args;

        hex::log::print(fmt::runtime(romfs::get("logo.ans").string()),
                   ImHexApi::System::getImHexVersion().get(),
                   ImHexApi::System::getCommitBranch(), ImHexApi::System::getCommitHash(),
                   __DATE__, __TIME__,
                   ImHexApi::System::isPortableVersion() ? "Portable" : "Installed");

        return EXIT_SUCCESS;
    }

    int handleVersionShortCommand(std::span<const std::string> args) {
        std::ignore = args;
        hex::log::println("{}", ImHexApi::System::getImHexVersion().get());
        return EXIT_SUCCESS;
    }

    int handleHelpCommand(std::span<const std::string> args) {
        std::ignore = args;

        hex::log::print(
                "ImHex - A Hex Editor for Reverse Engineers, Programmers and people who value their retinas when working at 3 AM.\n"
                "\n"
                "usage: imhex [subcommand] [options]\n"
                "Available subcommands:\n"
        );

        size_t longestLongCommand = 0, longestShortCommand = 0;
        for (const auto &plugin : PluginManager::getPlugins()) {
            for (const auto &subCommand : plugin.getSubCommands()) {
                longestLongCommand = std::max(longestLongCommand, subCommand.commandLong.size());
                longestShortCommand = std::max(longestShortCommand, subCommand.commandShort.size());
            }
        }

        for (const auto &plugin : PluginManager::getPlugins()) {
            for (const auto &subCommand : plugin.getSubCommands()) {
                hex::log::println("    "
                        "{}"
                        "{: <{}}"
                        "{}"
                        "{}"
                        "{: <{}}"
                        "{}",
                        subCommand.commandShort.empty() ? " " : "-",
                        subCommand.commandShort, longestShortCommand,
                        subCommand.commandShort.empty() ? "  " : ", ",
                        subCommand.commandLong.empty() ? " " : "--",
                        subCommand.commandLong, longestLongCommand + 5,
                        subCommand.commandDescription
                );
            }
        }

        return EXIT_SUCCESS;
    }

    int handleOpenCommand(std::span<const std::string> args) {
        if (args.empty()) {
            hex::log::println("No files provided to open.");
            return EXIT_FAILURE;
        }

        std::vector<std::string> fullPaths;
        for (const auto &arg : args) {
            try {
                std::fs::path path;

                try {
                    path = std::fs::absolute(arg);
                } catch(std::fs::filesystem_error &) {
                    path = arg;
                }

                if (path.empty())
                    continue;

                fullPaths.push_back(wolv::util::toUTF8String(path));
            } catch (std::exception &e) {
                log::error("Failed to open file '{}'\n    {}", arg, e.what());
            }
        }

        if (!fullPaths.empty())
            hex::subcommands::forwardSubCommand("open", fullPaths);

        return EXIT_CONTINUE;
    }

    int handleNewCommand(std::span<const std::string> ) {
        hex::subcommands::forwardSubCommand("new", {});
        return EXIT_CONTINUE;
    }

    int handleSelectCommand(std::span<const std::string> args) {
        if (args.size() == 1) {
            hex::subcommands::forwardSubCommand("select", { args[0] });
        } else if (args.size() == 2) {
            hex::subcommands::forwardSubCommand("select", { args[0], args[1] });
        } else {
            hex::log::println("Usage: imhex --select <start> [<end>]");
            return EXIT_FAILURE;
        }
        return EXIT_CONTINUE;
    }

    int handlePatternCommand(std::span<const std::string> args) {
        if (args.size() == 1) {
            hex::subcommands::forwardSubCommand("pattern", { args[0] });
        } else {
            hex::log::println("Usage: imhex --pattern <pattern source code>");
            hex::log::println("Usage: imhex --pattern <pattern file path>");
            return EXIT_FAILURE;
        }
        return EXIT_CONTINUE;
    }

    int handleCalcCommand(std::span<const std::string> args) {
        if (args.empty()) {
            hex::log::println("No expression provided!");
            hex::log::println("Usage:   imhex --calc <math expression>");
            hex::log::println("Example: imhex --calc \"5 * 7\"");
            return EXIT_FAILURE;
        }

        wolv::math_eval::MathEvaluator<long double> evaluator;

        auto input = fmt::format("{}", fmt::join(args, " "));
        auto result = evaluator.evaluate(input);

        if (!result.has_value())
            hex::log::println("{}\n> '{}'", evaluator.getLastError().value(), input);
        else
            hex::log::println("{}", result.value());

        return EXIT_SUCCESS;
    }

    int handlePluginsCommand(std::span<const std::string> args) {

        if (args.empty()) {
            hex::log::println("Loaded plugins:");

            for (const auto &plugin : PluginManager::getPlugins()) {
                if (plugin.isLibraryPlugin()) continue;

                hex::log::print("- \033[1m{}\033[0m", plugin.getPluginName());
                hex::log::println(" by {}", plugin.getPluginAuthor());
                hex::log::println("  \033[2;3m{}\033[0m", plugin.getPluginDescription());
            }

            return EXIT_SUCCESS;
        } else {
            for (const auto &arg : args) {
                PluginManager::addLoadPath(reinterpret_cast<const char8_t*>(arg.c_str()));
            }
            return EXIT_CONTINUE;
        }
    }

    int handleLanguageCommand(std::span<const std::string> args) {
        if (args.empty()) {
            hex::log::println("usage: imhex --language <language>");
            return EXIT_FAILURE;
        }

        ImHexApi::System::impl::addInitArgument("language", args[0]);
        return EXIT_CONTINUE;
    }

    int handleVerboseCommand(std::span<const std::string> ) {
        hex::log::enableDebugLogging();
        return EXIT_CONTINUE;
    }

    int handleHashCommand(std::span<const std::string> args) {
        if (args.size() != 2) {
            hex::log::println("usage: imhex --hash <algorithm> <file>");
            hex::log::println("Available algorithms: md5, sha1, sha224, sha256, sha384, sha512");
            return EXIT_FAILURE;
        }

        const auto &algorithm  = args[0];
        const auto &filePath   = std::fs::path(args[1]);

        wolv::io::File file(filePath, wolv::io::File::Mode::Read);
        if (!file.isValid()) {
            hex::log::println("Failed to open file: {}", wolv::util::toUTF8String(filePath));
            return EXIT_FAILURE;
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
            hex::log::println("Unknown algorithm: {}", algorithm);
            hex::log::println("Available algorithms: md5, sha1, sha224, sha256, sha384, sha512");
            return EXIT_FAILURE;
        }

        hex::log::println("{}({}) = {}", algorithm, wolv::util::toUTF8String(filePath.filename()), hex::crypt::encode16(result));

        return EXIT_SUCCESS;
    }

    int handleEncodeCommand(std::span<const std::string> args) {
        if (args.size() != 2) {
            hex::log::println("usage: imhex --encode <algorithm> <string>");
            hex::log::println("Available algorithms: base64, hex");
            return EXIT_FAILURE;
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
            hex::log::println("Unknown algorithm: {}", algorithm);
            hex::log::println("Available algorithms: base64, hex");
            return EXIT_FAILURE;
        }

        hex::log::println("encode_{}({}) = {}", algorithm, args[1], result);
        return EXIT_SUCCESS;
    }

    int handleDecodeCommand(std::span<const std::string> args) {
        if (args.size() != 2) {
            hex::log::println("usage: imhex --decode <algorithm> <string>");
            hex::log::println("Available algorithms: base64, hex");
            return EXIT_FAILURE;
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
            hex::log::println("Unknown algorithm: {}", algorithm);
            hex::log::println("Available algorithms: base64, hex");
            return EXIT_FAILURE;
        }

        hex::log::print("decode_{}({}) = {}", algorithm, args[1], result);
        return EXIT_SUCCESS;
    }

    int handleMagicCommand(std::span<const std::string> args) {
        if (args.size() != 2) {
            hex::log::println("usage: imhex --magic <operation> <file>");
            hex::log::println("Available operations: mime, desc");
            return EXIT_FAILURE;
        }

        if (!magic::compile()) {
            hex::log::print("Failed to compile magic database!");
            return EXIT_FAILURE;
        }

        const auto &operation  = args[0];
        const auto &filePath   = std::fs::path(args[1]);

        wolv::io::File file(filePath, wolv::io::File::Mode::Read);
        if (!file.isValid()) {
            hex::log::println("Failed to open file: {}", wolv::util::toUTF8String(filePath));
            return EXIT_FAILURE;
        }

        auto data = file.readVector(std::min<size_t>(file.getSize(), 100_KiB));

        if (operation == "mime") {
            auto result = magic::getMIMEType(data);
            hex::log::println("{}", result);
        } else if (operation == "desc") {
            auto result = magic::getDescription(data);
            hex::log::println("{}", result);
        } else {
            hex::log::println("Unknown operation: {}", operation);
            hex::log::println("Available operations: mime, desc");
            return EXIT_FAILURE;
        }

        return EXIT_SUCCESS;
    }

    int handlePatternLanguageCommand(std::span<const std::string> args) {
        std::vector<std::string> processedArgs = { args.begin(), args.end() };
        if (processedArgs.empty()) {
            processedArgs.emplace_back("--help");
        } else {
            for (const auto &path : paths::PatternsInclude.read())
                processedArgs.emplace_back(fmt::format("--includes={}", wolv::util::toUTF8String(path)));
        }

        pl::PatternLanguage runtime;
        ContentRegistry::PatternLanguage::configureRuntime(runtime, nullptr);

        return pl::cli::executeCommandLineInterface(processedArgs, runtime);
    }

    int handleHexdumpCommand(std::span<const std::string> args) {
        if (args.empty() || args.size() > 3) {
            log::println("usage: imhex --hexdump <file> <offset> <size>");
            return EXIT_FAILURE;
        }

        std::fs::path filePath = reinterpret_cast<const char8_t*>(args[0].data());

        FileProvider provider;
        provider.setPickedPath(filePath);
        auto result = provider.open();
        if (result.isFailure()) {
            log::println("Failed to open file '{}': {}", args[0], result.getErrorMessage());
            return EXIT_FAILURE;
        }

        u64 startAddress = args.size() >= 2 ? std::stoull(args[1], nullptr, 0) : 0x00;
        u64 size         = args.size() >= 3 ? std::stoull(args[2], nullptr, 0) : provider.getActualSize();

        size = std::min<u64>(size, provider.getActualSize());

        log::print("{}", hex::generateHexView(startAddress, size - startAddress, &provider));

        return EXIT_SUCCESS;
    }

    int handleDemangleCommand(std::span<const std::string> args) {
        if (args.size() != 1) {
            log::println("usage: imhex --demangle <identifier>");
            return EXIT_FAILURE;
        }

        log::println("{}", trace::demangle(args[0]));
        return EXIT_SUCCESS;
    }

    int handleSettingsResetCommand(std::span<const std::string> ) {
        constexpr static auto ConfirmationString = "YES I AM ABSOLUTELY SURE";

        log::println("You're about to reset all settings back to their default. Are you sure you want to continue?");
        log::println("Type \"{}\" to continue.", ConfirmationString);

        std::string input(128, '\x00');

        log::print("> ");
        if (std::fgets(input.data(), input.size() - 1, stdin) == nullptr)
            return EXIT_FAILURE;

        input = input.c_str(); // Stop at first null byte
        input = wolv::util::trim(input);

        if (input == ConfirmationString) {
            log::println("Resetting all settings!");
            ContentRegistry::Settings::impl::clear();
            ContentRegistry::Settings::impl::store();

            return EXIT_SUCCESS;
        } else {
            log::println("Wrong confirmation string. Settings will not be reset.");
            return EXIT_FAILURE;
        }
    }

    int handleDebugModeCommand(std::span<const std::string> ) {
        dbg::setDebugModeEnabled(true);
        return EXIT_CONTINUE;
    }

    int handleValidatePluginCommand(std::span<const std::string> args) {
        if (args.size() != 1) {
            log::println("usage: imhex --validate-plugin <plugin path>");
            return EXIT_FAILURE;
        }

        log::resumeLogging();

        const auto plugin = Plugin(args[0]);

        if (!plugin.isLoaded()) {
            log::println("Plugin couldn't be loaded. Make sure the plugin was built using the SDK of this ImHex version!");
            return EXIT_FAILURE;
        }

        if (!plugin.isValid()) {
            log::println("Plugin is missing required init function! Make sure your plugin has a IMHEX_PLUGIN_SETUP or IMHEX_LIBRARY_SETUP block!");
            return EXIT_FAILURE;
        }

        if (!plugin.initializePlugin()) {
            log::println("An error occurred while trying to initialize the plugin. Check the logs for more information.");
            return EXIT_FAILURE;
        }

        log::println("Plugin is valid!");

        return EXIT_SUCCESS;
    }

    int handleSaveEditorCommand(std::span<const std::string> args) {
        std::string type;
        std::string argument;
        if (args.size() == 1) {
            type = "file";
            argument = args[0];
        } else if (args.size() == 2) {
            type = args[0];
            argument = args[1];
        } else {
            log::println("usage: imhex --save-editor [file|gist] <file path|gist id>");
            return EXIT_FAILURE;
        }

        if (type == "file") {
            if (!wolv::io::fs::exists(argument)) {
                log::println("Save Editor file '{}' does not exist!", argument);
                return EXIT_FAILURE;
            }

            wolv::io::File file(argument, wolv::io::File::Mode::Read);
            if (!file.isValid()) {
                log::println("Failed to open Save Editor file '{}'", argument);
                return EXIT_FAILURE;
            }

            ContentRegistry::Views::setFullScreenView<ViewFullScreenSaveEditor>(file.readString());
        } else if (type == "gist") {
            const auto &response = GitHubApi::getGist(argument).get();

            if (!response.isSuccess()) {
                const auto statusCode = response.getStatusCode();
                if (statusCode == 0) {
                    log::println("{}", response.getErrorMessage());
                } else {
                    switch (statusCode) {
                    case 404:
                        log::println("Gist with ID '{}' not found!", argument);
                        break;
                    case 403:
                        log::println("Gist with ID '{}' is private or you have exceeded the rate limit!", argument);
                        break;
                    default:
                        log::println("Failed to fetch Gist with ID '{}': {}", argument, statusCode);
                        break;
                    }
                }

                std::exit(EXIT_FAILURE);
            }

            const auto &gist = response.getData();
            if (gist.files.size() != 1) {
                log::println("Gist with ID '{}' does not have exactly one file!", argument);
                return EXIT_FAILURE;
            }

            TaskManager::doLater([sourceCode = gist.files.front().content] {
                ContentRegistry::Views::setFullScreenView<ViewFullScreenSaveEditor>(sourceCode);
            });
        } else {
            log::println("Unknown source type '{}'. Use 'file' or 'gist'.", type);
            return EXIT_FAILURE;
        }

        return EXIT_CONTINUE;
    }

    int handleFileInfoCommand(std::span<const std::string> args) {
        if (args.size() != 1) {
            log::println("usage: imhex --file-info <file>");
            return EXIT_FAILURE;
        }

        const auto path = std::fs::path(args[0]);
        if (!wolv::io::fs::exists(path)) {
            log::println("File '{}' does not exist!", args[0]);
            return EXIT_FAILURE;
        }

        ContentRegistry::Views::setFullScreenView<ViewFullScreenFileInfo>(path);
        return EXIT_SUCCESS;
    }

    int handleMCPCommand(std::span<const std::string> ) {
        mcp::Client client;

        auto result = client.run(std::cin, std::cout);
        fmt::print(stderr, "MCP Client disconnected!\n");
        return result;
    }

    int handleScalingCommand(std::span<const std::string> args) {
        if (args.size() != 1) {
            log::println("usage: imhex --scaling <scale>");
            return EXIT_FAILURE;
        }

        auto scale = std::stod(args[0], nullptr);
        EventWindowOpening::subscribe([scale](auto) {
            ImHexApi::System::impl::setScaleMultiplier(scale);
        });

        return EXIT_CONTINUE;
    }


    void registerCommandForwarders() {
        hex::subcommands::registerSubCommand("open", [](std::span<const std::string> args){
            for (auto &arg : args) {
                RequestOpenFile::post(arg);
            }
        });

        hex::subcommands::registerSubCommand("new", [](std::span<const std::string> ){
            RequestOpenWindow::post("Create File");
        });

        hex::subcommands::registerSubCommand("select", [](std::span<const std::string> args){
            try {
                if (args.size() == 1) {
                    ImHexApi::HexEditor::setSelection(std::stoull(args[0], nullptr, 0), 1);
                } else if (args.size() == 2) {
                    const auto start = std::stoull(args[0], nullptr, 0);
                    const auto size = (std::stoull(args[1], nullptr, 0) - start) + 1;
                    ImHexApi::HexEditor::setSelection(start, size);
                } else {
                    log::error("Invalid number of arguments for select command!");
                }
            } catch (const std::exception &e) {
                log::error("Failed to set requested selection region! {}", e.what());
            }
        });

        hex::subcommands::registerSubCommand("pattern", [](std::span<const std::string> args){
            std::string patternSourceCode;
            if (std::fs::exists(args[0])) {
                wolv::io::File file(args[0], wolv::io::File::Mode::Read);
                if (!file.isValid()) {
                    patternSourceCode = args[0];
                } else {
                    patternSourceCode = file.readString();
                }
            } else {
                patternSourceCode = args[0];
            }

            RequestSetPatternLanguageCode::post(patternSourceCode);
            RequestTriggerPatternEvaluation::post();
        });
    }

}
