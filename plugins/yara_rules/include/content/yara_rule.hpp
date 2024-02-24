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
            std::string variable;
            Region region;
            bool wholeDataMatch;
        };

        struct Rule {
            std::string identifier;
            std::map<std::string, std::string> metadata;
            std::vector<std::string> tags;

            std::vector<Match> matches;
        };

        struct Result {
            std::vector<Rule> matchedRules;
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

        wolv::util::Expected<Result, Error> match(prv::Provider *provider, Region region);
        void interrupt();
        [[nodiscard]] bool isInterrupted() const;

    private:
        std::string m_content;
        std::fs::path m_filePath;

        std::atomic<bool> m_interrupted = false;
    };

}
