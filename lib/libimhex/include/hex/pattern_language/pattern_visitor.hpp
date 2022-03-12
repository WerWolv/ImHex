#pragma once

namespace hex::pl {

    class PatternArrayDynamic;
    class PatternArrayStatic;
    class PatternBitfield;
    class PatternBitfieldField;
    class PatternBoolean;
    class PatternCharacter;
    class PatternEnum;
    class PatternFloat;
    class PatternPadding;
    class PatternPointer;
    class PatternSigned;
    class PatternString;
    class PatternStruct;
    class PatternUnion;
    class PatternUnsigned;
    class PatternWideCharacter;
    class PatternWideString;

    class PatternVisitor
    {
    public:
        virtual void visit(PatternArrayDynamic& pattern) = 0;
        virtual void visit(PatternArrayStatic& pattern) = 0;
        virtual void visit(PatternBitfield& pattern) = 0;
        virtual void visit(PatternBitfieldField& pattern) = 0;
        virtual void visit(PatternBoolean& pattern) = 0;
        virtual void visit(PatternCharacter& pattern) = 0;
        virtual void visit(PatternEnum& pattern) = 0;
        virtual void visit(PatternFloat& pattern) = 0;
        virtual void visit(PatternPadding& pattern) = 0;
        virtual void visit(PatternPointer& pattern) = 0;
        virtual void visit(PatternSigned& pattern) = 0;
        virtual void visit(PatternString& pattern) = 0;
        virtual void visit(PatternStruct& pattern) = 0;
        virtual void visit(PatternUnion& pattern) = 0;
        virtual void visit(PatternUnsigned& pattern) = 0;
        virtual void visit(PatternWideCharacter& pattern) = 0;
        virtual void visit(PatternWideString& pattern) = 0;
    };
};