#pragma once

#include <pl/patterns/pattern.hpp>
#include <pl/pattern_visitor.hpp>
#include <hex/providers/provider.hpp>

namespace hex {

    template<typename T>
    concept ArrayPattern = requires(u64 displayEnd, T pattern, std::function<void(int, pl::Pattern&)> fn) {
        { pattern.forEachArrayEntry(displayEnd, fn) } -> std::same_as<void>;
    };

    class PatternDrawer : public pl::PatternVisitor {
    public:
        PatternDrawer() = default;

        void visit(pl::PatternArrayDynamic& pattern) override;
        void visit(pl::PatternArrayStatic& pattern) override;
        void visit(pl::PatternBitfieldField& pattern) override;
        void visit(pl::PatternBitfield& pattern) override;
        void visit(pl::PatternBoolean& pattern) override;
        void visit(pl::PatternCharacter& pattern) override;
        void visit(pl::PatternEnum& pattern) override;
        void visit(pl::PatternFloat& pattern) override;
        void visit(pl::PatternPadding& pattern) override;
        void visit(pl::PatternPointer& pattern) override;
        void visit(pl::PatternSigned& pattern) override;
        void visit(pl::PatternString& pattern) override;
        void visit(pl::PatternStruct& pattern) override;
        void visit(pl::PatternUnion& pattern) override;
        void visit(pl::PatternUnsigned& pattern) override;
        void visit(pl::PatternWideCharacter& pattern) override;
        void visit(pl::PatternWideString& pattern) override;

    private:
        void createDefaultEntry(const pl::Pattern &pattern, const std::string &value, pl::Token::Literal &&literal) const;
        void createLeafNode(const pl::Pattern& pattern) const;
        bool createTreeNode(const pl::Pattern& pattern) const;

        void makeSelectable(const pl::Pattern &pattern) const;

        void draw(pl::Pattern& pattern);

        template<ArrayPattern T>
        void drawArray(T& pattern) {
            bool opened = this->drawArrayRoot(pattern, pattern.getEntryCount(), pattern.isInlined());

            if (opened) {
                auto& displayEnd = this->getDisplayEnd(pattern);
                pattern.forEachArrayEntry(displayEnd, [&] (u64 idx, auto &entry){
                    this->drawArrayNode(idx, displayEnd, entry);
                });
            }

            this->drawArrayEnd(pattern, opened);
        }

        bool drawArrayRoot(pl::Pattern& pattern, size_t entryCount, bool isInlined);
        void drawArrayNode(u64 idx, u64& displayEnd, pl::Pattern& pattern);
        void drawArrayEnd(pl::Pattern& pattern, bool opened);

        void drawCommentTooltip(const pl::Pattern &pattern) const;
        void drawTypenameColumn(const pl::Pattern& pattern, const std::string& pattern_name) const;
        void drawNameColumn(const pl::Pattern& pattern) const;
        void drawColorColumn(const pl::Pattern& pattern) const;
        void drawOffsetColumn(const pl::Pattern& pattern) const;
        void drawSizeColumn(const pl::Pattern& pattern) const;

        u64& getDisplayEnd(const pl::Pattern& pattern);

    private:
        std::map<const pl::Pattern*, u64> m_displayEnd;
    };
};