//===------------------------- MicrosoftDemangle.h --------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_DEMANGLE_MICROSOFTDEMANGLE_H
#define LLVM_DEMANGLE_MICROSOFTDEMANGLE_H

#include "llvm/Demangle/MicrosoftDemangleNodes.h"
#include "llvm/Demangle/StringView.h"

#include <utility>

namespace llvm {
namespace ms_demangle {
// This memory allocator is extremely fast, but it doesn't call dtors
// for allocated objects. That means you can't use STL containers
// (such as std::vector) with this allocator. But it pays off --
// the demangler is 3x faster with this allocator compared to one with
// STL containers.
constexpr size_t AllocUnit = 4096;

class ArenaAllocator {
  struct AllocatorNode {
    uint8_t *Buf = nullptr;
    size_t Used = 0;
    size_t Capacity = 0;
    AllocatorNode *Next = nullptr;
  };

  void addNode(size_t Capacity) {
    AllocatorNode *NewHead = new AllocatorNode;
    NewHead->Buf = new uint8_t[Capacity];
    NewHead->Next = Head;
    NewHead->Capacity = Capacity;
    Head = NewHead;
    NewHead->Used = 0;
  }

public:
  ArenaAllocator() { addNode(AllocUnit); }

  ~ArenaAllocator() {
    while (Head) {
      assert(Head->Buf);
      delete[] Head->Buf;
      AllocatorNode *Next = Head->Next;
      delete Head;
      Head = Next;
    }
  }

  char *allocUnalignedBuffer(size_t Size) {
    assert(Head && Head->Buf);

    uint8_t *P = Head->Buf + Head->Used;

    Head->Used += Size;
    if (Head->Used <= Head->Capacity)
      return reinterpret_cast<char *>(P);

    addNode(std::max(AllocUnit, Size));
    Head->Used = Size;
    return reinterpret_cast<char *>(Head->Buf);
  }

  template <typename T, typename... Args> T *allocArray(size_t Count) {
    size_t Size = Count * sizeof(T);
    assert(Head && Head->Buf);

    size_t P = (size_t)Head->Buf + Head->Used;
    uintptr_t AlignedP =
        (((size_t)P + alignof(T) - 1) & ~(size_t)(alignof(T) - 1));
    uint8_t *PP = (uint8_t *)AlignedP;
    size_t Adjustment = AlignedP - P;

    Head->Used += Size + Adjustment;
    if (Head->Used <= Head->Capacity)
      return new (PP) T[Count]();

    addNode(std::max(AllocUnit, Size));
    Head->Used = Size;
    return new (Head->Buf) T[Count]();
  }

  template <typename T, typename... Args> T *alloc(Args &&... ConstructorArgs) {
    constexpr size_t Size = sizeof(T);
    assert(Head && Head->Buf);

    size_t P = (size_t)Head->Buf + Head->Used;
    uintptr_t AlignedP =
        (((size_t)P + alignof(T) - 1) & ~(size_t)(alignof(T) - 1));
    uint8_t *PP = (uint8_t *)AlignedP;
    size_t Adjustment = AlignedP - P;

    Head->Used += Size + Adjustment;
    if (Head->Used <= Head->Capacity)
      return new (PP) T(std::forward<Args>(ConstructorArgs)...);

    static_assert(Size < AllocUnit, "");
    addNode(AllocUnit);
    Head->Used = Size;
    return new (Head->Buf) T(std::forward<Args>(ConstructorArgs)...);
  }

private:
  AllocatorNode *Head = nullptr;
};

struct BackrefContext {
  static constexpr size_t Max = 10;

  TypeNode *FunctionParams[Max];
  size_t FunctionParamCount = 0;

  // The first 10 BackReferences in a mangled name can be back-referenced by
  // special name @[0-9]. This is a storage for the first 10 BackReferences.
  NamedIdentifierNode *Names[Max];
  size_t NamesCount = 0;
};

enum class QualifierMangleMode { Drop, Mangle, Result };

enum NameBackrefBehavior : uint8_t {
  NBB_None = 0,          // don't save any names as backrefs.
  NBB_Template = 1 << 0, // save template instanations.
  NBB_Simple = 1 << 1,   // save simple names.
};

enum class FunctionIdentifierCodeGroup { Basic, Under, DoubleUnder };

// Demangler class takes the main role in demangling symbols.
// It has a set of functions to parse mangled symbols into Type instances.
// It also has a set of functions to convert Type instances to strings.
class Demangler {
public:
  Demangler() = default;
  virtual ~Demangler() = default;

  // You are supposed to call parse() first and then check if error is true.  If
  // it is false, call output() to write the formatted name to the given stream.
  SymbolNode *parse(StringView &MangledName);

  TagTypeNode *parseTagUniqueName(StringView &MangledName);

  // True if an error occurred.
  bool Error = false;

  void dumpBackReferences();

private:
  SymbolNode *demangleEncodedSymbol(StringView &MangledName,
                                    QualifiedNameNode *QN);
  SymbolNode *demangleDeclarator(StringView &MangledName);
  SymbolNode *demangleMD5Name(StringView &MangledName);
  SymbolNode *demangleTypeinfoName(StringView &MangledName);

  VariableSymbolNode *demangleVariableEncoding(StringView &MangledName,
                                               StorageClass SC);
  FunctionSymbolNode *demangleFunctionEncoding(StringView &MangledName);

  Qualifiers demanglePointerExtQualifiers(StringView &MangledName);

  // Parser functions. This is a recursive-descent parser.
  TypeNode *demangleType(StringView &MangledName, QualifierMangleMode QMM);
  PrimitiveTypeNode *demanglePrimitiveType(StringView &MangledName);
  CustomTypeNode *demangleCustomType(StringView &MangledName);
  TagTypeNode *demangleClassType(StringView &MangledName);
  PointerTypeNode *demanglePointerType(StringView &MangledName);
  PointerTypeNode *demangleMemberPointerType(StringView &MangledName);
  FunctionSignatureNode *demangleFunctionType(StringView &MangledName,
                                              bool HasThisQuals);

  ArrayTypeNode *demangleArrayType(StringView &MangledName);

  NodeArrayNode *demangleFunctionParameterList(StringView &MangledName,
                                               bool &IsVariadic);
  NodeArrayNode *demangleTemplateParameterList(StringView &MangledName);

  std::pair<uint64_t, bool> demangleNumber(StringView &MangledName);
  uint64_t demangleUnsigned(StringView &MangledName);
  int64_t demangleSigned(StringView &MangledName);

  void memorizeString(StringView s);
  void memorizeIdentifier(IdentifierNode *Identifier);

  /// Allocate a copy of \p Borrowed into memory that we own.
  StringView copyString(StringView Borrowed);

  QualifiedNameNode *demangleFullyQualifiedTypeName(StringView &MangledName);
  QualifiedNameNode *demangleFullyQualifiedSymbolName(StringView &MangledName);

  IdentifierNode *demangleUnqualifiedTypeName(StringView &MangledName,
                                              bool Memorize);
  IdentifierNode *demangleUnqualifiedSymbolName(StringView &MangledName,
                                                NameBackrefBehavior NBB);

  QualifiedNameNode *demangleNameScopeChain(StringView &MangledName,
                                            IdentifierNode *UnqualifiedName);
  IdentifierNode *demangleNameScopePiece(StringView &MangledName);

  NamedIdentifierNode *demangleBackRefName(StringView &MangledName);
  IdentifierNode *demangleTemplateInstantiationName(StringView &MangledName,
                                                    NameBackrefBehavior NBB);
  IntrinsicFunctionKind
  translateIntrinsicFunctionCode(char CH, FunctionIdentifierCodeGroup Group);
  IdentifierNode *demangleFunctionIdentifierCode(StringView &MangledName);
  IdentifierNode *
  demangleFunctionIdentifierCode(StringView &MangledName,
                                 FunctionIdentifierCodeGroup Group);
  StructorIdentifierNode *demangleStructorIdentifier(StringView &MangledName,
                                                     bool IsDestructor);
  ConversionOperatorIdentifierNode *
  demangleConversionOperatorIdentifier(StringView &MangledName);
  LiteralOperatorIdentifierNode *
  demangleLiteralOperatorIdentifier(StringView &MangledName);

  SymbolNode *demangleSpecialIntrinsic(StringView &MangledName);
  SpecialTableSymbolNode *
  demangleSpecialTableSymbolNode(StringView &MangledName,
                                 SpecialIntrinsicKind SIK);
  LocalStaticGuardVariableNode *
  demangleLocalStaticGuard(StringView &MangledName, bool IsThread);
  VariableSymbolNode *demangleUntypedVariable(ArenaAllocator &Arena,
                                              StringView &MangledName,
                                              StringView VariableName);
  VariableSymbolNode *
  demangleRttiBaseClassDescriptorNode(ArenaAllocator &Arena,
                                      StringView &MangledName);
  FunctionSymbolNode *demangleInitFiniStub(StringView &MangledName,
                                           bool IsDestructor);

  NamedIdentifierNode *demangleSimpleName(StringView &MangledName,
                                          bool Memorize);
  NamedIdentifierNode *demangleAnonymousNamespaceName(StringView &MangledName);
  NamedIdentifierNode *demangleLocallyScopedNamePiece(StringView &MangledName);
  EncodedStringLiteralNode *demangleStringLiteral(StringView &MangledName);
  FunctionSymbolNode *demangleVcallThunkNode(StringView &MangledName);

  StringView demangleSimpleString(StringView &MangledName, bool Memorize);

  FuncClass demangleFunctionClass(StringView &MangledName);
  CallingConv demangleCallingConvention(StringView &MangledName);
  StorageClass demangleVariableStorageClass(StringView &MangledName);
  bool demangleThrowSpecification(StringView &MangledName);
  wchar_t demangleWcharLiteral(StringView &MangledName);
  uint8_t demangleCharLiteral(StringView &MangledName);

  std::pair<Qualifiers, bool> demangleQualifiers(StringView &MangledName);

  // Memory allocator.
  ArenaAllocator Arena;

  // A single type uses one global back-ref table for all function params.
  // This means back-refs can even go "into" other types.  Examples:
  //
  //  // Second int* is a back-ref to first.
  //  void foo(int *, int*);
  //
  //  // Second int* is not a back-ref to first (first is not a function param).
  //  int* foo(int*);
  //
  //  // Second int* is a back-ref to first (ALL function types share the same
  //  // back-ref map.
  //  using F = void(*)(int*);
  //  F G(int *);
  BackrefContext Backrefs;
};

} // namespace ms_demangle
} // namespace llvm

#endif // LLVM_DEMANGLE_MICROSOFTDEMANGLE_H
