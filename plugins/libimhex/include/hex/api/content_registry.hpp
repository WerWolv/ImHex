#pragma once

#include <hex.hpp>
#include <hex/helpers/concepts.hpp>
#include <hex/pattern_language/token.hpp>

#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json_fwd.hpp>

namespace hex {

    class View;
    class LanguageDefinition;
    namespace pl { class Evaluator; }
    namespace dp { class Node; }

    /*
        The Content Registry is the heart of all features in ImHex that are in some way extendable by Plugins.
        It allows you to add/register new content that will be picked up and used by the ImHex core or by other
        plugins when needed.
    */
    namespace ContentRegistry {

        /* Settings Registry. Allows adding of new entries into the ImHex preferences window. */
        namespace Settings {
            using Callback = std::function<bool(const std::string&, nlohmann::json&)>;

            struct Entry {
                std::string name;
                Callback callback;
            };

            void load();
            void store();

            void add(const std::string &unlocalizedCategory, const std::string &unlocalizedName, s64 defaultValue, const Callback &callback);
            void add(const std::string &unlocalizedCategory, const std::string &unlocalizedName, const std::string &defaultValue, const Callback &callback);

            void write(const std::string &unlocalizedCategory, const std::string &unlocalizedName, s64 value);
            void write(const std::string &unlocalizedCategory, const std::string &unlocalizedName, const std::string &value);
            void write(const std::string &unlocalizedCategory, const std::string &unlocalizedName, const std::vector<std::string>& value);

            s64 read(const std::string &unlocalizedCategory, const std::string &unlocalizedName, s64 defaultValue);
            std::string read(const std::string &unlocalizedCategory, const std::string &unlocalizedName, const std::string &defaultValue);
            std::vector<std::string> read(const std::string &unlocalizedCategory, const std::string &unlocalizedName, const std::vector<std::string>& defaultValue = { });

            std::map<std::string, std::vector<Entry>>& getEntries();
            nlohmann::json getSetting(const std::string &unlocalizedCategory, const std::string &unlocalizedName);
            nlohmann::json& getSettingsData();
        };

        /* Command Palette Command Registry. Allows adding of new commands to the command palette */
        namespace CommandPaletteCommands {

            enum class Type : u32 {
                SymbolCommand,
                KeywordCommand
            };

            using DisplayCallback = std::function<std::string(std::string)>;
            using ExecuteCallback = std::function<void(std::string)>;

            struct Entry {
                Type type;
                std::string command;
                std::string unlocalizedDescription;
                DisplayCallback displayCallback;
                ExecuteCallback executeCallback;
            };

            void add(Type type, const std::string &command, const std::string &unlocalizedDescription, const DisplayCallback &displayCallback, const ExecuteCallback &executeCallback = [](auto){});
            std::vector<Entry>& getEntries();
        };

        /* Pattern Language Function Registry. Allows adding of new functions that may be used inside the pattern language */
        namespace PatternLanguageFunctions {

            constexpr static u32 UnlimitedParameters   = 0xFFFF'FFFF;
            constexpr static u32 MoreParametersThan    = 0x8000'0000;
            constexpr static u32 LessParametersThan    = 0x4000'0000;
            constexpr static u32 NoParameters          = 0x0000'0000;

            using Namespace = std::vector<std::string>;
            using Callback = std::function<std::optional<hex::pl::Token::Literal>(hex::pl::Evaluator*, const std::vector<hex::pl::Token::Literal>&)>;

            struct Function {
                u32 parameterCount;
                Callback func;
            };

            void add(const Namespace &ns, const std::string &name, u32 parameterCount, const Callback &func);
            std::map<std::string, ContentRegistry::PatternLanguageFunctions::Function>& getEntries();
        };

        /* View Registry. Allows adding of new windows */
        namespace Views {
            void add(View *view);

            template<hex::derived_from<View> T, typename ... Args>
            void add(Args&& ... args) {
                return add(new T(std::forward<Args>(args)...));
            }

            std::vector<View*>& getEntries();

        };

        /* Tools Registry. Allows adding new entries to the tools window */
        namespace Tools {
            using Callback = std::function<void()>;

            struct Entry {
                std::string name;
                Callback function;
            };

            void add(const std::string &unlocalizedName, const Callback &function);

            std::vector<Entry>& getEntries();
        };

        /* Data Inspector Registry. Allows adding of new types to the data inspector */
        namespace DataInspector {

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

            void add(const std::string &unlocalizedName, size_t requiredSize, GeneratorFunction function);

            std::vector<Entry>& getEntries();
        };

        /* Data Processor Node Registry. Allows adding new processor nodes to be used in the data processor */
        namespace DataProcessorNode {

            using CreatorFunction = std::function<dp::Node*()>;
            struct Entry {
                std::string category;
                std::string name;
                CreatorFunction creatorFunction;
            };

            void add(const Entry &entry);

            template<hex::derived_from<dp::Node> T, typename ... Args>
            void add(const std::string &unlocalizedCategory, const std::string &unlocalizedName, Args&& ... args) {
                add(Entry{ unlocalizedCategory.c_str(), unlocalizedName.c_str(),
                   [=]{
                        auto node = new T(std::forward<Args>(args)...);
                        node->setUnlocalizedName(unlocalizedName);
                        return node;
                   }
                });
            }

            void addSeparator();

            std::vector<Entry>& getEntries();

        };

        /* Language Registry. Allows together with the LangEntry class and the _lang user defined literal to add new languages */
        namespace Language {
            void registerLanguage(const std::string &name, const std::string &languageCode);
            void addLocalizations(const std::string &languageCode, const LanguageDefinition &definition);

            std::map<std::string, std::string>& getLanguages();
            std::map<std::string, std::vector<LanguageDefinition>>& getLanguageDefinitions();
        };

        /* Interface Registry. Allows adding new items to various interfaces */
        namespace Interface {
            using DrawCallback = std::function<void()>;

            void addWelcomeScreenEntry(const DrawCallback &function);
            void addFooterItem(const DrawCallback &function);
            void addToolbarItem(const DrawCallback &function);

            std::vector<DrawCallback>& getWelcomeScreenEntries();
            std::vector<DrawCallback>& getFooterItems();
            std::vector<DrawCallback>& getToolbarItems();
        };
    };

}