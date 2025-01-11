#pragma once

#include <hex/api/task_manager.hpp>
#include <hex/api/content_registry.hpp>
#include <ui/visualizer_drawer.hpp>

#include <pl/patterns/pattern.hpp>
#include <pl/pattern_visitor.hpp>

#include <pl/formatters.hpp>

#include <set>

struct ImGuiTableSortSpecs;

namespace hex::ui {

    class PatternDrawer : public pl::PatternVisitor {
    public:
        PatternDrawer() {
            m_formatters = pl::gen::fmt::createFormatters();
        }

        virtual ~PatternDrawer() = default;

        void draw(const std::vector<std::shared_ptr<pl::ptrn::Pattern>> &patterns, const pl::PatternLanguage *runtime = nullptr, float height = 0.0F);

        enum class TreeStyle : u8 {
            Default         = 0,
            AutoExpanded    = 1,
            Flattened       = 2
        };

        void setTreeStyle(TreeStyle style) { m_treeStyle = style; }
        void setSelectionCallback(std::function<void(const pl::ptrn::Pattern *)> callback) { m_selectionCallback = std::move(callback); }
        void setHoverCallback(std::function<void(const pl::ptrn::Pattern *)> callback) { m_hoverCallback = std::move(callback); }
        void enableRowColoring(bool enabled) { m_rowColoring = enabled; }
        void enablePatternEditing(bool enabled) { m_editingEnabled = enabled; }
        void setMaxFilterDisplayItems(u32 count) { m_maxFilterDisplayItems = count; }

        void reset();

        void jumpToPattern(const pl::ptrn::Pattern *pattern) { m_jumpToPattern = pattern; }

    private:
        void draw(pl::ptrn::Pattern& pattern);

    public:
        void visit(pl::ptrn::PatternArrayDynamic& pattern) override;
        void visit(pl::ptrn::PatternArrayStatic& pattern) override;
        void visit(pl::ptrn::PatternBitfieldField& pattern) override;
        void visit(pl::ptrn::PatternBitfieldArray& pattern) override;
        void visit(pl::ptrn::PatternBitfield& pattern) override;
        void visit(pl::ptrn::PatternBoolean& pattern) override;
        void visit(pl::ptrn::PatternCharacter& pattern) override;
        void visit(pl::ptrn::PatternEnum& pattern) override;
        void visit(pl::ptrn::PatternFloat& pattern) override;
        void visit(pl::ptrn::PatternPadding& pattern) override;
        void visit(pl::ptrn::PatternPointer& pattern) override;
        void visit(pl::ptrn::PatternSigned& pattern) override;
        void visit(pl::ptrn::PatternString& pattern) override;
        void visit(pl::ptrn::PatternStruct& pattern) override;
        void visit(pl::ptrn::PatternUnion& pattern) override;
        void visit(pl::ptrn::PatternUnsigned& pattern) override;
        void visit(pl::ptrn::PatternWideCharacter& pattern) override;
        void visit(pl::ptrn::PatternWideString& pattern) override;
        void visit(pl::ptrn::Pattern& pattern) override;

    private:
        constexpr static auto ChunkSize = 512;
        constexpr static auto DisplayEndStep = 64;

        void drawArray(pl::ptrn::Pattern& pattern, pl::ptrn::IIterable &iterable, bool isInlined);
        u64& getDisplayEnd(const pl::ptrn::Pattern& pattern);
        void makeSelectable(const pl::ptrn::Pattern &pattern);

        void drawValueColumn(pl::ptrn::Pattern& pattern);
        void drawFavoriteColumn(const pl::ptrn::Pattern& pattern);
        bool drawNameColumn(const pl::ptrn::Pattern &pattern, bool leaf = false);
        void drawColorColumn(const pl::ptrn::Pattern& pattern);
        void drawCommentColumn(const pl::ptrn::Pattern& pattern);

        bool beginPatternTable(const std::vector<std::shared_ptr<pl::ptrn::Pattern>> &patterns, std::vector<pl::ptrn::Pattern*> &sortedPatterns, float height) const;
        bool createTreeNode(const pl::ptrn::Pattern& pattern, bool leaf = false);
        void createDefaultEntry(const pl::ptrn::Pattern &pattern);
        void closeTreeNode(bool inlined) const;

        bool sortPatterns(const ImGuiTableSortSpecs* sortSpecs, const pl::ptrn::Pattern * left, const pl::ptrn::Pattern * right) const;
        [[nodiscard]] bool isEditingPattern(const pl::ptrn::Pattern& pattern) const;
        void resetEditing();
        void traversePatternTree(pl::ptrn::Pattern &pattern, std::vector<std::string> &patternPath, const std::function<void(pl::ptrn::Pattern&)> &callback);
        [[nodiscard]] std::string getDisplayName(const pl::ptrn::Pattern& pattern) const;

        [[nodiscard]] std::vector<std::string> getPatternPath(const pl::ptrn::Pattern *pattern) const;

        struct Filter {
            std::vector<std::string> path;
            std::optional<pl::core::Token::Literal> value;
        };

        [[nodiscard]] static bool matchesFilter(const std::vector<std::string> &filterPath, const std::vector<std::string> &patternPath, bool fullMatch);
        [[nodiscard]] static std::optional<Filter> parseRValueFilter(const std::string &filter);
        void updateFilter();

    private:
        std::map<const pl::ptrn::Pattern*, u64> m_displayEnd;
        std::vector<pl::ptrn::Pattern*> m_sortedPatterns;

        const pl::ptrn::Pattern *m_editingPattern = nullptr;
        u64 m_editingPatternOffset = 0;
        hex::ui::VisualizerDrawer m_visualizerDrawer;

        TreeStyle m_treeStyle = TreeStyle::Default;
        bool m_rowColoring = false;
        bool m_editingEnabled = false;
        pl::ptrn::Pattern *m_currVisualizedPattern = nullptr;
        const pl::ptrn::Pattern *m_jumpToPattern = nullptr;

        std::set<pl::ptrn::Pattern*> m_visualizedPatterns;

        std::string m_filterText;
        Filter m_filter;
        std::vector<pl::ptrn::Pattern*> m_filteredPatterns;

        std::vector<std::string> m_currPatternPath;
        std::map<std::vector<std::string>, std::unique_ptr<pl::ptrn::Pattern>> m_favorites;
        std::map<std::string, std::vector<std::unique_ptr<pl::ptrn::Pattern>>> m_groups;
        bool m_showFavoriteStars = false;
        bool m_filtersUpdated = false;
        bool m_showSpecName = false;

        TaskHolder m_favoritesUpdateTask;

        std::function<void(const pl::ptrn::Pattern *)> m_selectionCallback = [](const pl::ptrn::Pattern *) { };
        std::function<void(const pl::ptrn::Pattern *)> m_hoverCallback = [](const pl::ptrn::Pattern *) { };

        pl::gen::fmt::FormatterArray m_formatters;
        u64 m_lastRunId = 0;

        u32 m_maxFilterDisplayItems = 128;
    };
}