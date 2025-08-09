#pragma once

#include <hex.hpp>
#include <pl/pattern_visitor.hpp>
#include <functional>

namespace hex::ui {

    class PatternValueEditor : public pl::PatternVisitor {
    public:
        PatternValueEditor() = default;
        explicit PatternValueEditor(const std::function<void()>& onEditCallback) : m_onEditCallback(onEditCallback) {}

        void visit(pl::ptrn::PatternArrayDynamic& pattern) override;
        void visit(pl::ptrn::PatternArrayStatic& pattern) override;
        void visit(pl::ptrn::PatternBitfield& pattern) override;
        void visit(pl::ptrn::PatternBitfieldField& pattern) override;
        void visit(pl::ptrn::PatternBitfieldArray& pattern) override;
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
        void visit(pl::ptrn::PatternError& pattern) override;
        void visit(pl::ptrn::Pattern& pattern) override;

    private:
        std::function<void()> m_onEditCallback = [](){};
    };

}
