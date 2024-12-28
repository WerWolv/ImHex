#pragma once

#include <hex/helpers/fs.hpp>

#include <vector>

namespace hex::paths {

    namespace impl {

        class DefaultPath {
        protected:
            constexpr DefaultPath() = default;
            virtual ~DefaultPath() = default;

        public:
            DefaultPath(const DefaultPath&) = delete;
            DefaultPath(DefaultPath&&) = delete;
            DefaultPath& operator=(const DefaultPath&) = delete;
            DefaultPath& operator=(DefaultPath&&) = delete;

            virtual std::vector<std::fs::path> all() const = 0;
            virtual std::vector<std::fs::path> read() const;
            virtual std::vector<std::fs::path> write() const;
        };

        class ConfigPath : public DefaultPath {
        public:
            explicit ConfigPath(std::fs::path postfix) : m_postfix(std::move(postfix)) {}

            std::vector<std::fs::path> all() const override;

        private:
            std::fs::path m_postfix;
        };

        class DataPath : public DefaultPath {
        public:
            explicit DataPath(std::fs::path postfix) : m_postfix(std::move(postfix)) {}

            std::vector<std::fs::path> all() const override;
            std::vector<std::fs::path> write() const override;

        private:
            std::fs::path m_postfix;
        };

        class PluginPath : public DefaultPath {
        public:
            explicit PluginPath(std::fs::path postfix) : m_postfix(std::move(postfix)) {}

            std::vector<std::fs::path> all() const override;

        private:
            std::fs::path m_postfix;
        };

    }

    std::vector<std::fs::path> getDataPaths(bool includeSystemFolders);
    std::vector<std::fs::path> getConfigPaths(bool includeSystemFolders);

    const static inline impl::ConfigPath Config("config");
    const static inline impl::ConfigPath Recent("recent");

    const static inline impl::PluginPath Libraries("lib");
    const static inline impl::PluginPath Plugins("plugins");

    const static inline impl::DataPath Patterns("patterns");
    const static inline impl::DataPath PatternsInclude("includes");
    const static inline impl::DataPath Magic("magic");
    const static inline impl::DataPath Yara("yara");
    const static inline impl::DataPath YaraAdvancedAnalysis("yara/advanced_analysis");
    const static inline impl::DataPath Backups("backups");
    const static inline impl::DataPath Resources("resources");
    const static inline impl::DataPath Constants("constants");
    const static inline impl::DataPath Encodings("encodings");
    const static inline impl::DataPath Logs("logs");
    const static inline impl::DataPath Scripts("scripts");
    const static inline impl::DataPath Inspectors("scripts/inspectors");
    const static inline impl::DataPath Themes("themes");
    const static inline impl::DataPath Nodes("scripts/nodes");
    const static inline impl::DataPath Layouts("layouts");
    const static inline impl::DataPath Workspaces("workspaces");
    const static inline impl::DataPath Disassemblers("disassemblers");

    constexpr static inline std::array<const impl::DefaultPath*, 21> All = {
        &Config,
        &Recent,

        &Libraries,
        &Plugins,

        &Patterns,
        &PatternsInclude,
        &Magic,
        &Yara,
        &YaraAdvancedAnalysis,
        &Backups,
        &Resources,
        &Constants,
        &Encodings,
        &Logs,
        &Scripts,
        &Inspectors,
        &Themes,
        &Nodes,
        &Layouts,
        &Workspaces,
        &Disassemblers
    };

}