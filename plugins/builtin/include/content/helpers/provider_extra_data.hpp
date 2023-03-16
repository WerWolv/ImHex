#pragma once

#include <hex/api/content_registry.hpp>
#include <hex/providers/provider.hpp>

#include <pl/pattern_language.hpp>
#include <hex/data_processor/attribute.hpp>
#include <hex/data_processor/node.hpp>
#include <hex/data_processor/link.hpp>

#include <map>

#include <imnodes.h>
#include <imnodes_internal.h>

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
                struct Workspace {
                    std::unique_ptr<ImNodesContext, void(*)(ImNodesContext*)> context = { []{
                        ImNodesContext *ctx = ImNodes::CreateContext();
                        ctx->Style = ImNodes::GetStyle();
                        ctx->Io = ImNodes::GetIO();
                        ctx->AttributeFlagStack = GImNodes->AttributeFlagStack;

                        return ctx;
                    }(), ImNodes::DestroyContext };

                    std::list<std::unique_ptr<dp::Node>> nodes;
                    std::list<dp::Node*> endNodes;
                    std::list<dp::Link> links;
                    std::vector<hex::prv::Overlay *> dataOverlays;
                    std::optional<dp::Node::NodeError> currNodeError;
                };

                Workspace mainWorkspace;
                std::vector<Workspace*> workspaceStack;
            } dataProcessor;

            struct HexEditor {
                std::optional<u64> selectionStart, selectionEnd;
                float scrollPosition = 0.0F;
            } editor;

            struct Hashes {
                std::vector<ContentRegistry::Hashes::Hash::Function> hashFunctions;
            } hashes;

            struct Yara {
                struct YaraMatch {
                    std::string identifier;
                    std::string variable;
                    u64 address;
                    size_t size;
                    bool wholeDataMatch;

                    mutable u32 highlightId;
                    mutable u32 tooltipId;
                };

                std::vector<std::pair<std::fs::path, std::fs::path>> rules;
                std::vector<YaraMatch> matches;
                std::vector<YaraMatch*> sortedMatches;
            } yara;
        };

        static Data& getCurrent() {
            return get(ImHexApi::Provider::get());
        }

        static Data& get(const hex::prv::Provider *provider) {
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
        static inline std::map<const hex::prv::Provider*, Data> s_data = {};
    };

}