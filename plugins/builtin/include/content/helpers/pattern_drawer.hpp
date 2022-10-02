#pragma once

#include <pl/patterns/pattern.hpp>
#include <pl/pattern_visitor.hpp>
#include <hex/providers/provider.hpp>

namespace hex {

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
        void draw(pl::ptrn::Pattern& pattern);

        constexpr static auto ChunkSize = 512;
        constexpr static auto DisplayEndStep = 64;

        void drawArray(pl::ptrn::Pattern& pattern, pl::ptrn::Iteratable &iteratable, bool isInlined);
        u64& getDisplayEnd(const pl::ptrn::Pattern& pattern);

    private:
        std::map<const pl::ptrn::Pattern*, u64> m_displayEnd;
    };
}