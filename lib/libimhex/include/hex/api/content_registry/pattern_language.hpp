#pragma once

#include <hex.hpp>

#include <pl/pattern_language.hpp>

#include <functional>
#include <span>
#include <string>
#include <map>
#include <vector>
#include <mutex>

EXPORT_MODULE namespace hex {

    #if !defined(HEX_MODULE_EXPORT)
        namespace prv { class Provider; }
    #endif

    /* Pattern Language Function Registry. Allows adding of new functions that may be used inside the pattern language */
    namespace ContentRegistry::PatternLanguage {

        namespace impl {

            using VisualizerFunctionCallback = std::function<void(pl::ptrn::Pattern&, bool, std::span<const pl::core::Token::Literal>)>;

            struct FunctionDefinition {
                pl::api::Namespace ns;
                std::string name;

                pl::api::FunctionParameterCount parameterCount;
                pl::api::FunctionCallback callback;

                bool dangerous;
            };

            struct TypeDefinition {
                pl::api::Namespace ns;
                std::string name;

                pl::api::FunctionParameterCount parameterCount;
                pl::api::TypeCallback callback;
            };

            struct Visualizer {
                pl::api::FunctionParameterCount parameterCount;
                VisualizerFunctionCallback callback;
            };

            const std::map<std::string, Visualizer>& getVisualizers();
            const std::map<std::string, Visualizer>& getInlineVisualizers();
            const std::map<std::string, pl::api::PragmaHandler>& getPragmas();
            const std::vector<FunctionDefinition>& getFunctions();
            const std::vector<TypeDefinition>& getTypes();

        }

        /**
         * @brief Provides access to the current provider's pattern language runtime
         * @return Runtime
         */
        pl::PatternLanguage& getRuntime();

        /**
         * @brief Provides access to the current provider's pattern language runtime's lock
         * @return Lock
         */
        std::mutex& getRuntimeLock();

        /**
         * @brief Configures the pattern language runtime using ImHex's default settings
         * @param runtime The pattern language runtime to configure
         * @param provider The provider to use for data access
         */
        void configureRuntime(pl::PatternLanguage &runtime, prv::Provider *provider);

        /**
         * @brief Adds a new pragma to the pattern language
         * @param name The name of the pragma
         * @param handler The handler that will be called when the pragma is encountered
         */
        void addPragma(const std::string &name, const pl::api::PragmaHandler &handler);

        /**
         * @brief Adds a new function to the pattern language
         * @param ns The namespace of the function
         * @param name The name of the function
         * @param parameterCount The amount of parameters the function takes
         * @param func The function callback
         */
        void addFunction(
            const pl::api::Namespace &ns,
            const std::string &name,
            pl::api::FunctionParameterCount parameterCount,
            const pl::api::FunctionCallback &func
        );

        /**
         * @brief Adds a new dangerous function to the pattern language
         * @note Dangerous functions are functions that require the user to explicitly allow them to be used
         * @param ns The namespace of the function
         * @param name The name of the function
         * @param parameterCount The amount of parameters the function takes
         * @param func The function callback
         */
        void addDangerousFunction(
            const pl::api::Namespace &ns,
            const std::string &name,
            pl::api::FunctionParameterCount parameterCount,
            const pl::api::FunctionCallback &func
        );

        /**
         * @brief Adds a new type to the pattern language
         * @param ns The namespace of the type
         * @param name The name of the type
         * @param parameterCount The amount of non-type template parameters the type takes
         * @param func The type callback
         */
        void addType(
            const pl::api::Namespace &ns,
            const std::string &name,
            pl::api::FunctionParameterCount parameterCount,
            const pl::api::TypeCallback &func
        );

        /**
         * @brief Adds a new visualizer to the pattern language
         * @note Visualizers are extensions to the [[hex::visualize]] attribute, used to visualize data
         * @param name The name of the visualizer
         * @param function The function callback
         * @param parameterCount The amount of parameters the function takes
         */
        void addVisualizer(
            const std::string &name,
            const impl::VisualizerFunctionCallback &function,
            pl::api::FunctionParameterCount parameterCount
        );

        /**
         * @brief Adds a new inline visualizer to the pattern language
         * @note Inline visualizers are extensions to the [[hex::inline_visualize]] attribute, used to visualize data
         * @param name The name of the visualizer
         * @param function The function callback
         * @param parameterCount The amount of parameters the function takes
         */
        void addInlineVisualizer(
            const std::string &name,
            const impl::VisualizerFunctionCallback &function,
            pl::api::FunctionParameterCount parameterCount
        );

    }

}