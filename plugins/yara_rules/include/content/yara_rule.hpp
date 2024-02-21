#pragma once

#include <hex/providers/provider.hpp>

#include <string>
#include <vector>
#include <wolv/utils/expected.hpp>

namespace hex::plugin::yara {

    class YaraRule {
    public:
        YaraRule() = default;
        explicit YaraRule(const std::string& content);
        explicit YaraRule(const std::fs::path& path);

        static void init();
        static void cleanup();

        struct Match {
            std::string identifier;
            std::string variable;
            Region region;
            bool wholeDataMatch;
        };

        struct Result {
            std::vector<Match> matches;
            std::vector<std::string> consoleMessages;
        };

        struct Error {
            enum class Type {
                CompileError,
                RuntimeError,
                Interrupted
            } type;
            std::string message;
        };

        wolv::util::Expected<Result, Error> match(prv::Provider *provider, u64 address, size_t size);
        void interrupt();
        [[nodiscard]] bool isInterrupted() const;

    private:
        std::string m_content;
        std::fs::path m_filePath;

        std::atomic<bool> m_interrupted = false;
    };

}
