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
    namespace lang { class LogConsole; }

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
                std::function<bool(nlohmann::json&)> callback;
            };

            static void load();
            static void store();

            static void add(std::string_view category, std::string_view name, s64 defaultValue, const std::function<bool(nlohmann::json&)> &callback);
            static void add(std::string_view category, std::string_view name, std::string_view defaultValue, const std::function<bool(nlohmann::json&)> &callback);

            static std::map<std::string, std::vector<Entry>>& getEntries();
            static nlohmann::json& getSettingsData();
        };

        /* Events Registry. Allows to define new events that can be used by other plugins later on subscribe to */
        struct Events {
            Events() = delete;

            static auto get(std::string_view name);
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
                std::string description;
                std::function<std::string(std::string)> callback;
            };

            static void add(Type type, std::string_view command, std::string_view description, const std::function<std::string(std::string)> &callback);
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
                std::function<hex::lang::ASTNode*(hex::lang::LogConsole&, std::vector<hex::lang::ASTNode*>)> func;
            };

            static void add(std::string_view name, u32 parameterCount, const std::function<hex::lang::ASTNode*(hex::lang::LogConsole&, std::vector<hex::lang::ASTNode*>)> &func);
            static std::map<std::string, ContentRegistry::PatternLanguageFunctions::Function>& getEntries();
        };

        /* View Registry. Allows adding of new windows */
        struct Views {
            Views() = delete;

            template<hex::derived_from<View> T, typename ... Args>
            static T* add(Args&& ... args) {
                return static_cast<T*>(add(new T(std::forward<Args>(args)...)));
            }

            static std::vector<View*>& getEntries();

        private:
            static View* add(View *view);


        };

        /* Tools Registry. Allows adding new entries to the tools window */
        struct Tools {
            Tools() = delete;

            struct Entry {
                std::string name;
                std::function<void()> function;
            };

            static void add(std::string_view name, const std::function<void()> &function);

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

            using DisplayFunction = std::function<void()>;
            using GeneratorFunction = std::function<DisplayFunction(const std::vector<u8>&, std::endian, NumberDisplayStyle)>;

            struct Entry {
                std::string name;
                size_t requiredSize;
                GeneratorFunction generatorFunction;
            };

            static void add(std::string_view name, size_t requiredSize, GeneratorFunction function);

            static std::vector<Entry>& getEntries();
        };
    };

}