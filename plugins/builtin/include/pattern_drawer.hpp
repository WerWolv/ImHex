#pragma once

#include <pl/patterns/pattern.hpp>
#include <pl/pattern_visitor.hpp>
#include <hex/providers/provider.hpp>

namespace hex {

    template<typename T>
    concept ArrayPattern = requires(u64 displayEnd, T pattern, std::function<void(int, pl::ptrn::Pattern&)> fn) {
        { pattern.forEachArrayEntry(displayEnd, fn) } -> std::same_as<void>;
    };

    class PatternDrawer : public pl::PatternVisitor {
    public:
        PatternDrawer() = default;

        void visit(pl::ptrn::PatternArrayDynamic& pattern) override;
        void visit(pl::ptrn::PatternArrayStatic& pattern) override;
        void visit(pl::ptrn::PatternBitfieldField& pattern) override;
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
        void createDefaultEntry(const pl::ptrn::Pattern &pattern, const std::string &value, pl::core::Token::Literal &&literal) const;
        void createLeafNode(const pl::ptrn::Pattern& pattern) const;
        bool createTreeNode(const pl::ptrn::Pattern& pattern) const;

        void makeSelectable(const pl::ptrn::Pattern &pattern) const;

        void draw(pl::ptrn::Pattern& pattern);

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

        bool drawArrayRoot(pl::ptrn::Pattern& pattern, size_t entryCount, bool isInlined);
        void drawArrayNode(u64 idx, u64& displayEnd, pl::ptrn::Pattern& pattern);
        void drawArrayEnd(pl::ptrn::Pattern& pattern, bool opened);

        void drawCommentTooltip(const pl::ptrn::Pattern &pattern) const;
        void drawTypenameColumn(const pl::ptrn::Pattern& pattern, const std::string& pattern_name) const;
        void drawNameColumn(const pl::ptrn::Pattern& pattern) const;
        void drawColorColumn(const pl::ptrn::Pattern& pattern) const;
        void drawOffsetColumn(const pl::ptrn::Pattern& pattern) const;
        void drawSizeColumn(const pl::ptrn::Pattern& pattern) const;

        u64& getDisplayEnd(const pl::ptrn::Pattern& pattern);

    private:
        std::map<const pl::ptrn::Pattern*, u64> m_displayEnd;
    };
};