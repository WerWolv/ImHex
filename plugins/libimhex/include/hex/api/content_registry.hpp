#pragma once

#include <hex.hpp>

#include <hex/helpers/utils.hpp>

#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

namespace hex {

    class View;
    namespace lang { class ASTNode; }
    namespace lang { class Evaluator; }
    namespace dp { class Node; }

    /*
        The Content Registry is the heart of all features in ImHex that are in some way extendable by Plugins.
        It allows you to add/register new content that will be picked up and used by the ImHex core or by other
        plugins when needed.
    */
    struct ContentRegistry {
        ContentRegistry() = delete;

        /* Settings Registry. Allows adding of new entries into the ImHex preferences window. */
        struct Settings {
            Settings() = delete;

            struct Entry {
                std::string name;
                std::function<bool(std::string_view, nlohmann::json&)> callback;
            };

            static void load();
            static void store();

            static void add(std::string_view unlocalizedCategory, std::string_view unlocalizedName, s64 defaultValue, const std::function<bool(std::string_view, nlohmann::json&)> &callback);
            static void add(std::string_view unlocalizedCategory, std::string_view unlocalizedName, std::string_view defaultValue, const std::function<bool(std::string_view, nlohmann::json&)> &callback);

            static void write(std::string_view unlocalizedCategory, std::string_view unlocalizedName, s64 value);
            static void write(std::string_view unlocalizedCategory, std::string_view unlocalizedName, std::string_view value);
            static void write(std::string_view unlocalizedCategory, std::string_view unlocalizedName, const std::vector<std::string>& value);

            static s64 read(std::string_view unlocalizedCategory, std::string_view unlocalizedName, s64 defaultValue);
            static std::string read(std::string_view unlocalizedCategory, std::string_view unlocalizedName, std::string_view defaultValue);
            static std::vector<std::string> read(std::string_view unlocalizedCategory, std::string_view unlocalizedName, const std::vector<std::string>& defaultValue = { });

            static std::map<std::string, std::vector<Entry>>& getEntries();
            static nlohmann::json getSetting(std::string_view unlocalizedCategory, std::string_view unlocalizedName);
            static nlohmann::json& getSettingsData();
        };

        /* Command Palette Command Registry. Allows adding of new commands to the command palette */
        struct CommandPaletteCommands {
            CommandPaletteCommands() = delete;

            enum class Type : u32 {
                SymbolCommand,
                KeywordCommand
            };

            struct Entry {
                Type type;
                std::string command;
                std::string unlocalizedDescription;
                std::function<std::string(std::string)> displayCallback;
                std::function<void(std::string)> executeCallback;
            };

            static void add(Type type, std::string_view command, std::string_view unlocalizedDescription, const std::function<std::string(std::string)> &displayCallback, const std::function<void(std::string)> &executeCallback = [](auto){});
            static std::vector<Entry>& getEntries();
        };

        /* Pattern Language Function Registry. Allows adding of new functions that may be used inside the pattern language */
        struct PatternLanguageFunctions {
            PatternLanguageFunctions() = delete;

            constexpr static u32 UnlimitedParameters   = 0xFFFF'FFFF;
            constexpr static u32 MoreParametersThan    = 0x8000'0000;
            constexpr static u32 LessParametersThan    = 0x4000'0000;
            constexpr static u32 NoParameters          = 0x0000'0000;

            struct Function {
                u32 parameterCount;
                std::function<hex::lang::ASTNode*(hex::lang::Evaluator&, std::vector<hex::lang::ASTNode*>&)> func;
            };

            static void add(std::string_view name, u32 parameterCount, const std::function<hex::lang::ASTNode*(hex::lang::Evaluator&, std::vector<hex::lang::ASTNode*>&)> &func);
            static std::map<std::string, ContentRegistry::PatternLanguageFunctions::Function>& getEntries();
        };

        /* View Registry. Allows adding of new windows */
        struct Views {
            Views() = delete;

            template<hex::derived_from<View> T, typename ... Args>
            static void add(Args&& ... args) {
                return add(new T(std::forward<Args>(args)...));
            }

            static std::vector<View*>& getEntries();

        private:
            static void add(View *view);


        };

        /* Tools Registry. Allows adding new entries to the tools window */
        struct Tools {
            Tools() = delete;

            struct Entry {
                std::string name;
                std::function<void()> function;
            };

            static void add(std::string_view unlocalizedName, const std::function<void()> &function);

            static std::vector<Entry>& getEntries();
        };

        /* Data Inspector Registry. Allows adding of new types to the data inspector */
        struct DataInspector {
            DataInspector() = delete;

            enum class NumberDisplayStyle {
                Decimal,
                Hexadecimal,
                Octal
            };

            using DisplayFunction = std::function<std::string()>;
            using GeneratorFunction = std::function<DisplayFunction(const std::vector<u8>&, std::endian, NumberDisplayStyle)>;

            struct Entry {
                std::string unlocalizedName;
                size_t requiredSize;
                GeneratorFunction generatorFunction;
            };

            static void add(std::string_view unlocalizedName, size_t requiredSize, GeneratorFunction function);

            static std::vector<Entry>& getEntries();
        };

        /* Data Processor Node Registry. Allows adding new processor nodes to be used in the data processor */
        struct DataProcessorNode {
            using CreatorFunction = std::function<dp::Node*()>;
            struct Entry {
                std::string category;
                std::string name;
                CreatorFunction creatorFunction;
            };

            template<hex::derived_from<dp::Node> T, typename ... Args>
            static void add(std::string_view unlocalizedCategory, std::string_view unlocalizedName, Args&& ... args) {
                add(Entry{ unlocalizedCategory.data(), unlocalizedName.data(),
                   [args..., name = std::string(unlocalizedName)]{
                        auto node = new T(std::forward<Args>(args)...);
                        node->setUnlocalizedName(name);
                        return node;
                   }
                });
            }

            static void addSeparator();

            static std::vector<Entry>& getEntries();
        private:
            static void add(const Entry &entry);
        };

        /* Language Registry. Allows together with the LangEntry class and the _lang user defined literal to add new languages */
        struct Language {
            static void registerLanguage(std::string_view name, std::string_view languageCode);
            static void addLocalizations(std::string_view languageCode, const LanguageDefinition &definition);

            static std::map<std::string, std::string>& getLanguages();
            static std::map<std::string, std::vector<LanguageDefinition>>& getLanguageDefinitions();
        };

        /* Interface Registry. Allows adding new items to various interfaces */
        struct Interface {
            using DrawCallback = std::function<void()>;

            static void addWelcomeScreenEntry(const DrawCallback &function);
            static void addFooterItem(const DrawCallback &function);

            static std::vector<DrawCallback>& getWelcomeScreenEntries();
            static std::vector<DrawCallback>& getFooterItems();
        };
    };

}