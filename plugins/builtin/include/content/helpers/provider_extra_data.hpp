#pragma once

#include <hex/api/content_registry.hpp>
#include <hex/providers/provider.hpp>

#include <pl/pattern_language.hpp>
#include <hex/data_processor/attribute.hpp>
#include <hex/data_processor/node.hpp>
#include <hex/data_processor/link.hpp>

#include <map>

namespace hex::plugin::builtin {

    class ProviderExtraData {
    public:
        struct Data {
            bool dataDirty = false;

            struct PatternLanguage {
                struct PatternVariable {
                    bool inVariable;
                    bool outVariable;

                    pl::core::Token::ValueType type;
                    pl::core::Token::Literal value;
                };

                enum class EnvVarType
                {
                    Integer,
                    Float,
                    String,
                    Bool
                };

                struct EnvVar {
                    u64 id;
                    std::string name;
                    pl::core::Token::Literal value;
                    EnvVarType type;

                    bool operator==(const EnvVar &other) const {
                        return this->id == other.id;
                    }
                };

                std::string sourceCode;
                std::mutex runtimeMutex;
                std::unique_ptr<pl::PatternLanguage> runtime = std::make_unique<pl::PatternLanguage>();
                std::vector<std::pair<pl::core::LogConsole::Level, std::string>> console;
                bool executionDone = true;

                std::optional<pl::core::err::PatternLanguageError> lastEvaluationError;
                std::vector<std::pair<pl::core::LogConsole::Level, std::string>> lastEvaluationLog;
                std::map<std::string, pl::core::Token::Literal> lastEvaluationOutVars;
                std::map<std::string, PatternVariable> patternVariables;
                std::map<u64, pl::api::Section> sections;

                std::list<EnvVar> envVarEntries;
            } patternLanguage;

            std::list<ImHexApi::Bookmarks::Entry> bookmarks;

            struct DataProcessor {
                std::list<dp::Node*> endNodes;
                std::list<std::unique_ptr<dp::Node>> nodes;
                std::list<dp::Link> links;

                std::vector<hex::prv::Overlay *> dataOverlays;
                std::optional<dp::Node::NodeError> currNodeError;
            } dataProcessor;

            struct HexEditor {
                std::optional<u64> selectionStart, selectionEnd;
                float scrollPosition = 0.0F;
            } editor;

            struct Hashes {
                std::vector<ContentRegistry::Hashes::Hash::Function> hashFunctions;
            } hashes;
        };

        static Data& getCurrent() {
            return get(ImHexApi::Provider::get());
        }

        static Data& get(hex::prv::Provider *provider) {
            return s_data[provider];
        }

        static void erase(hex::prv::Provider *provider) {
            s_data.erase(provider);
        }

        static bool markDirty() {
            return getCurrent().dataDirty = true;
        }

    private:
        ProviderExtraData() = default;
        static inline std::map<hex::prv::Provider*, Data> s_data = {};
    };

}