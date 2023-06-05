#pragma once

#include <pl/patterns/pattern.hpp>
#include <pl/pattern_visitor.hpp>

#include <pl/formatters.hpp>

#include <hex/providers/provider.hpp>

namespace hex::plugin::builtin::ui {

    class PatternDrawer : public pl::PatternVisitor {
    public:
        PatternDrawer() {
            this->m_formatters = pl::gen::fmt::createFormatters();
        }

        void draw(const std::vector<std::shared_ptr<pl::ptrn::Pattern>> &patterns, pl::PatternLanguage *runtime = nullptr, float height = 0.0F);

        enum class TreeStyle {
            Default         = 0,
            AutoExpanded    = 1,
            Flattened       = 2
        };

        void setTreeStyle(TreeStyle style) { this->m_treeStyle = style; }
        void setSelectionCallback(std::function<void(Region)> callback) { this->m_selectionCallback = std::move(callback); }
        void reset();

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

    private:
        constexpr static auto ChunkSize = 512;
        constexpr static auto DisplayEndStep = 64;

        void drawArray(pl::ptrn::Pattern& pattern, pl::ptrn::IIterable &iterable, bool isInlined);
        u64& getDisplayEnd(const pl::ptrn::Pattern& pattern);
        void makeSelectable(const pl::ptrn::Pattern &pattern);

        void drawValueColumn(pl::ptrn::Pattern& pattern);
        void drawVisualizer(const std::vector<pl::core::Token::Literal> &arguments, pl::ptrn::Pattern &pattern, pl::ptrn::IIterable &iterable, bool reset);
        void drawFavoriteColumn(const pl::ptrn::Pattern& pattern);

        bool createTreeNode(const pl::ptrn::Pattern& pattern, bool leaf = false);
        void createDefaultEntry(pl::ptrn::Pattern &pattern);
        void closeTreeNode(bool inlined);

        bool isEditingPattern(const pl::ptrn::Pattern& pattern) const;
        void resetEditing();
        bool matchesFilter(const std::vector<std::string> &filterPath, bool fullMatch);
        void traversePatternTree(pl::ptrn::Pattern &pattern, const std::function<void(pl::ptrn::Pattern&)> &callback);

    private:
        std::map<const pl::ptrn::Pattern*, u64> m_displayEnd;
        std::vector<pl::ptrn::Pattern*> m_sortedPatterns;

        const pl::ptrn::Pattern *m_editingPattern = nullptr;
        u64 m_editingPatternOffset = 0;

        TreeStyle m_treeStyle = TreeStyle::Default;
        pl::ptrn::Pattern *m_currVisualizedPattern = nullptr;

        std::set<pl::ptrn::Pattern*> m_visualizedPatterns;
        std::string m_lastVisualizerError;

        std::string m_filterText;
        std::vector<std::string> m_filter;
        std::vector<std::string> m_currPatternPath;
        std::map<std::vector<std::string>, std::unique_ptr<pl::ptrn::Pattern>> m_favorites;
        bool m_showFavoriteStars = false;
        bool m_favoritesUpdated = false;

        std::function<void(Region)> m_selectionCallback = [](Region) { };

        pl::gen::fmt::FormatterArray m_formatters;
    };
}