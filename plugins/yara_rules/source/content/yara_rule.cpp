#include <content/yara_rule.hpp>

#include <wolv/literals.hpp>
#include <wolv/utils/guards.hpp>
#include <wolv/utils/string.hpp>
#include <wolv/io/file.hpp>

// <yara/types.h>'s RE type has a zero-sized array, which is not allowed in ISO C++.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#include <yara.h>
#pragma GCC diagnostic pop

namespace hex::plugin::yara {

    using namespace wolv::literals;

    struct ResultContext {
        YaraRule *rule;
        std::vector<YaraRule::Rule> matchedRules;
        std::vector<std::string> consoleMessages;

        std::string includeBuffer;
    };

    void YaraRule::init() {
        yr_initialize();
    }

    void YaraRule::cleanup() {
        yr_finalize();
    }

    YaraRule::YaraRule(const std::string &content) : m_content(content) { }

    YaraRule::YaraRule(const std::fs::path &path) : m_filePath(path) {
        wolv::io::File file(path, wolv::io::File::Mode::Read);
        if (!file.isValid())
            return;

        m_content = file.readString();
    }

    static int scanFunction(YR_SCAN_CONTEXT *context, int message, void *data, void *userData) {
        auto &resultContext = *static_cast<ResultContext *>(userData);

        switch (message) {
            case CALLBACK_MSG_RULE_MATCHING: {
                const auto *rule = static_cast<const YR_RULE *>(data);

                if (rule->strings != nullptr) {
                    YR_STRING *string;
                    yr_rule_strings_foreach(rule, string) {
                        YaraRule::Rule newRule;
                        newRule.identifier = rule->identifier;

                        YR_MATCH *match;
                        yr_string_matches_foreach(context, string, match) {
                            newRule.matches.push_back({ string->identifier, Region { u64(match->offset), size_t(match->match_length) }, false });
                        }

                        YR_META *meta;
                        yr_rule_metas_foreach(rule, meta) {
                            newRule.metadata[meta->identifier] = meta->string;
                        }

                        const char *tag;
                        yr_rule_tags_foreach(rule, tag) {
                            newRule.tags.emplace_back(tag);
                        }

                        resultContext.matchedRules.emplace_back(std::move(newRule));
                    }
                } else {
                    YaraRule::Rule newRule;
                    newRule.identifier = rule->identifier;
                    newRule.matches.push_back({ "", Region::Invalid(), true });
                }

                break;
            }
            case CALLBACK_MSG_CONSOLE_LOG: {
                resultContext.consoleMessages.emplace_back(static_cast<const char *>(data));
                break;
            }
            default:
                break;
        }

        return resultContext.rule->isInterrupted() ? CALLBACK_ABORT : CALLBACK_CONTINUE;
    }

    wolv::util::Expected<YaraRule::Result, YaraRule::Error> YaraRule::match(prv::Provider *provider, Region region) {
        YR_COMPILER *compiler = nullptr;
        yr_compiler_create(&compiler);
        ON_SCOPE_EXIT {
            yr_compiler_destroy(compiler);
        };

        m_interrupted = false;

        ResultContext resultContext = {};
        resultContext.rule = this;

        yr_compiler_set_include_callback(
            compiler,
            [](const char *includeName, const char *, const char *, void *userData) -> const char * {
                auto context = static_cast<ResultContext *>(userData);

                wolv::io::File file(context->rule->m_filePath.parent_path() / includeName, wolv::io::File::Mode::Read);
                if (!file.isValid())
                    return nullptr;

                context->includeBuffer = file.readString();
                return context->includeBuffer.c_str();
            },
            [](const char *ptr, void *userData) {
                std::ignore = ptr;
                std::ignore = userData;
            },
            &resultContext
        );

        if (yr_compiler_add_string(compiler, m_content.c_str(), nullptr) != ERROR_SUCCESS) {
            std::string errorMessage(0xFFFF, '\x00');
            yr_compiler_get_error_message(compiler, errorMessage.data(), errorMessage.size());

            return wolv::util::Unexpected(Error { Error::Type::CompileError, errorMessage.c_str() });
        }

        YR_RULES *yaraRules;
        yr_compiler_get_rules(compiler, &yaraRules);
        ON_SCOPE_EXIT { yr_rules_destroy(yaraRules); };

        YR_MEMORY_BLOCK_ITERATOR iterator;

        struct ScanContext {
            prv::Provider *provider;
            Region region;
            std::vector<u8> buffer;
            YR_MEMORY_BLOCK currBlock = {};
        };

        ScanContext context;

        context.provider             = provider;
        context.region               = region;
        context.currBlock.base       = 0;
        context.currBlock.fetch_data = [](YR_MEMORY_BLOCK *block) -> const u8 * {
            auto &context = *static_cast<ScanContext *>(block->context);

            context.buffer.resize(context.currBlock.size);

            if (context.buffer.empty())
                return nullptr;

            block->size = context.currBlock.size;
            context.provider->read(context.provider->getBaseAddress() + context.currBlock.base, context.buffer.data(), context.buffer.size());

            return context.buffer.data();
        };
        iterator.file_size = [](YR_MEMORY_BLOCK_ITERATOR *iterator) -> u64 {
            auto &context = *static_cast<ScanContext *>(iterator->context);

            return context.region.size;
        };

        iterator.context = &context;
        iterator.first   = [](YR_MEMORY_BLOCK_ITERATOR *iterator) -> YR_MEMORY_BLOCK   *{
            auto &context = *static_cast<ScanContext *>(iterator->context);

            context.currBlock.base = context.region.address;
            context.currBlock.size = 0;
            context.buffer.clear();
            iterator->last_error = ERROR_SUCCESS;

            return iterator->next(iterator);
        };
        iterator.next = [](YR_MEMORY_BLOCK_ITERATOR *iterator) -> YR_MEMORY_BLOCK * {
            auto &context = *static_cast<ScanContext *>(iterator->context);

            u64 address = context.currBlock.base + context.currBlock.size;

            iterator->last_error      = ERROR_SUCCESS;
            context.currBlock.base    = address;
            context.currBlock.size    = std::min<size_t>(context.region.size - (address - context.region.address), 10_MiB);
            context.currBlock.context = &context;

            if (context.currBlock.size == 0) return nullptr;

            return &context.currBlock;
        };

        if (yr_rules_scan_mem_blocks(yaraRules, &iterator, 0, scanFunction, &resultContext, 0) != ERROR_SUCCESS) {
            std::string errorMessage(0xFFFF, '\x00');
            yr_compiler_get_error_message(compiler, errorMessage.data(), errorMessage.size());

            return wolv::util::Unexpected(Error { Error::Type::RuntimeError, errorMessage.c_str() });
        }

        if (m_interrupted)
            return wolv::util::Unexpected(Error { Error::Type::Interrupted, "" });

        return Result { resultContext.matchedRules, resultContext.consoleMessages };
    }

    void YaraRule::interrupt() {
        m_interrupted = true;
    }

    bool YaraRule::isInterrupted() const {
        return m_interrupted;
    }

}