//===--- DLangDemangle.cpp ------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file defines a demangler for the D programming language as specified
/// in the ABI specification, available at:
/// https://dlang.org/spec/abi.html#name_mangling
///
//===----------------------------------------------------------------------===//

#include "llvm/Demangle/Demangle.h"
#include "llvm/Demangle/StringView.h"
#include "llvm/Demangle/Utility.h"

#include <cctype>
#include <cstring>
#include <limits>

using namespace llvm;
using llvm::itanium_demangle::OutputBuffer;
using llvm::itanium_demangle::StringView;

namespace {

/// Demangle information structure.
struct Demangler {
  /// Initialize the information structure we use to pass around information.
  ///
  /// \param Mangled String to demangle.
  Demangler(const char *Mangled);

  /// Extract and demangle the mangled symbol and append it to the output
  /// string.
  ///
  /// \param Demangled Output buffer to write the demangled name.
  ///
  /// \return The remaining string on success or nullptr on failure.
  ///
  /// \see https://dlang.org/spec/abi.html#name_mangling .
  /// \see https://dlang.org/spec/abi.html#MangledName .
  const char *parseMangle(OutputBuffer *Demangled);

private:
  /// Extract and demangle a given mangled symbol and append it to the output
  /// string.
  ///
  /// \param Demangled output buffer to write the demangled name.
  /// \param Mangled mangled symbol to be demangled.
  ///
  /// \return The remaining string on success or nullptr on failure.
  ///
  /// \see https://dlang.org/spec/abi.html#name_mangling .
  /// \see https://dlang.org/spec/abi.html#MangledName .
  const char *parseMangle(OutputBuffer *Demangled, const char *Mangled);

  /// Extract the number from a given string.
  ///
  /// \param Mangled string to extract the number.
  /// \param Ret assigned result value.
  ///
  /// \return The remaining string on success or nullptr on failure.
  ///
  /// \note A result larger than UINT_MAX is considered a failure.
  ///
  /// \see https://dlang.org/spec/abi.html#Number .
  const char *decodeNumber(const char *Mangled, unsigned long &Ret);

  /// Extract the back reference position from a given string.
  ///
  /// \param Mangled string to extract the back reference position.
  /// \param Ret assigned result value.
  ///
  /// \return the remaining string on success or nullptr on failure.
  ///
  /// \note Ret is always >= 0 on success, and unspecified on failure
  ///
  /// \see https://dlang.org/spec/abi.html#back_ref .
  /// \see https://dlang.org/spec/abi.html#NumberBackRef .
  const char *decodeBackrefPos(const char *Mangled, long &Ret);

  /// Extract the symbol pointed by the back reference form a given string.
  ///
  /// \param Mangled string to extract the back reference position.
  /// \param Ret assigned result value.
  ///
  /// \return the remaining string on success or nullptr on failure.
  ///
  /// \see https://dlang.org/spec/abi.html#back_ref .
  const char *decodeBackref(const char *Mangled, const char *&Ret);

  /// Extract and demangle backreferenced symbol from a given mangled symbol
  /// and append it to the output string.
  ///
  /// \param Demangled output buffer to write the demangled name.
  /// \param Mangled mangled symbol to be demangled.
  ///
  /// \return the remaining string on success or nullptr on failure.
  ///
  /// \see https://dlang.org/spec/abi.html#back_ref .
  /// \see https://dlang.org/spec/abi.html#IdentifierBackRef .
  const char *parseSymbolBackref(OutputBuffer *Demangled, const char *Mangled);

  /// Extract and demangle backreferenced type from a given mangled symbol
  /// and append it to the output string.
  ///
  /// \param Mangled mangled symbol to be demangled.
  ///
  /// \return the remaining string on success or nullptr on failure.
  ///
  /// \see https://dlang.org/spec/abi.html#back_ref .
  /// \see https://dlang.org/spec/abi.html#TypeBackRef .
  const char *parseTypeBackref(const char *Mangled);

  /// Check whether it is the beginning of a symbol name.
  ///
  /// \param Mangled string to extract the symbol name.
  ///
  /// \return true on success, false otherwise.
  ///
  /// \see https://dlang.org/spec/abi.html#SymbolName .
  bool isSymbolName(const char *Mangled);

  /// Extract and demangle an identifier from a given mangled symbol append it
  /// to the output string.
  ///
  /// \param Demangled Output buffer to write the demangled name.
  /// \param Mangled Mangled symbol to be demangled.
  ///
  /// \return The remaining string on success or nullptr on failure.
  ///
  /// \see https://dlang.org/spec/abi.html#SymbolName .
  const char *parseIdentifier(OutputBuffer *Demangled, const char *Mangled);

  /// Extract and demangle the plain identifier from a given mangled symbol and
  /// prepend/append it to the output string, with a special treatment for some
  /// magic compiler generated symbols.
  ///
  /// \param Demangled Output buffer to write the demangled name.
  /// \param Mangled Mangled symbol to be demangled.
  /// \param Len Length of the mangled symbol name.
  ///
  /// \return The remaining string on success or nullptr on failure.
  ///
  /// \see https://dlang.org/spec/abi.html#LName .
  const char *parseLName(OutputBuffer *Demangled, const char *Mangled,
                         unsigned long Len);

  /// Extract and demangle the qualified symbol from a given mangled symbol
  /// append it to the output string.
  ///
  /// \param Demangled Output buffer to write the demangled name.
  /// \param Mangled Mangled symbol to be demangled.
  ///
  /// \return The remaining string on success or nullptr on failure.
  ///
  /// \see https://dlang.org/spec/abi.html#QualifiedName .
  const char *parseQualified(OutputBuffer *Demangled, const char *Mangled);

  /// Extract and demangle a type from a given mangled symbol append it to
  /// the output string.
  ///
  /// \param Mangled mangled symbol to be demangled.
  ///
  /// \return the remaining string on success or nullptr on failure.
  ///
  /// \see https://dlang.org/spec/abi.html#Type .
  const char *parseType(const char *Mangled);

  /// The string we are demangling.
  const char *Str;
  /// The index of the last back reference.
  int LastBackref;
};

} // namespace

const char *Demangler::decodeNumber(const char *Mangled, unsigned long &Ret) {
  // Return nullptr if trying to extract something that isn't a digit.
  if (Mangled == nullptr || !std::isdigit(*Mangled))
    return nullptr;

  unsigned long Val = 0;

  do {
    unsigned long Digit = Mangled[0] - '0';

    // Check for overflow.
    if (Val > (std::numeric_limits<unsigned int>::max() - Digit) / 10)
      return nullptr;

    Val = Val * 10 + Digit;
    ++Mangled;
  } while (std::isdigit(*Mangled));

  if (*Mangled == '\0')
    return nullptr;

  Ret = Val;
  return Mangled;
}

const char *Demangler::decodeBackrefPos(const char *Mangled, long &Ret) {
  // Return nullptr if trying to extract something that isn't a digit
  if (Mangled == nullptr || !std::isalpha(*Mangled))
    return nullptr;

  // Any identifier or non-basic type that has been emitted to the mangled
  // symbol before will not be emitted again, but is referenced by a special
  // sequence encoding the relative position of the original occurrence in the
  // mangled symbol name.
  // Numbers in back references are encoded with base 26 by upper case letters
  // A-Z for higher digits but lower case letters a-z for the last digit.
  //    NumberBackRef:
  //        [a-z]
  //        [A-Z] NumberBackRef
  //        ^
  unsigned long Val = 0;

  while (std::isalpha(*Mangled)) {
    // Check for overflow
    if (Val > (std::numeric_limits<unsigned long>::max() - 25) / 26)
      break;

    Val *= 26;

    if (Mangled[0] >= 'a' && Mangled[0] <= 'z') {
      Val += Mangled[0] - 'a';
      if ((long)Val <= 0)
        break;
      Ret = Val;
      return Mangled + 1;
    }

    Val += Mangled[0] - 'A';
    ++Mangled;
  }

  return nullptr;
}

const char *Demangler::decodeBackref(const char *Mangled, const char *&Ret) {
  assert(Mangled != nullptr && *Mangled == 'Q' && "Invalid back reference!");
  Ret = nullptr;

  // Position of 'Q'
  const char *Qpos = Mangled;
  long RefPos;
  ++Mangled;

  Mangled = decodeBackrefPos(Mangled, RefPos);
  if (Mangled == nullptr)
    return nullptr;

  if (RefPos > Qpos - Str)
    return nullptr;

  // Set the position of the back reference.
  Ret = Qpos - RefPos;

  return Mangled;
}

const char *Demangler::parseSymbolBackref(OutputBuffer *Demangled,
                                          const char *Mangled) {
  // An identifier back reference always points to a digit 0 to 9.
  //    IdentifierBackRef:
  //        Q NumberBackRef
  //        ^
  const char *Backref;
  unsigned long Len;

  // Get position of the back reference
  Mangled = decodeBackref(Mangled, Backref);

  // Must point to a simple identifier
  Backref = decodeNumber(Backref, Len);
  if (Backref == nullptr || strlen(Backref) < Len)
    return nullptr;

  Backref = parseLName(Demangled, Backref, Len);
  if (Backref == nullptr)
    return nullptr;

  return Mangled;
}

const char *Demangler::parseTypeBackref(const char *Mangled) {
  // A type back reference always points to a letter.
  //    TypeBackRef:
  //        Q NumberBackRef
  //        ^
  const char *Backref;

  // If we appear to be moving backwards through the mangle string, then
  // bail as this may be a recursive back reference.
  if (Mangled - Str >= LastBackref)
    return nullptr;

  int SaveRefPos = LastBackref;
  LastBackref = Mangled - Str;

  // Get position of the back reference.
  Mangled = decodeBackref(Mangled, Backref);

  // Can't decode back reference.
  if (Backref == nullptr)
    return nullptr;

  // TODO: Add support for function type back references.
  Backref = parseType(Backref);

  LastBackref = SaveRefPos;

  if (Backref == nullptr)
    return nullptr;

  return Mangled;
}

bool Demangler::isSymbolName(const char *Mangled) {
  long Ret;
  const char *Qref = Mangled;

  if (std::isdigit(*Mangled))
    return true;

  // TODO: Handle template instances.

  if (*Mangled != 'Q')
    return false;

  Mangled = decodeBackrefPos(Mangled + 1, Ret);
  if (Mangled == nullptr || Ret > Qref - Str)
    return false;

  return std::isdigit(Qref[-Ret]);
}

const char *Demangler::parseMangle(OutputBuffer *Demangled,
                                   const char *Mangled) {
  // A D mangled symbol is comprised of both scope and type information.
  //    MangleName:
  //        _D QualifiedName Type
  //        _D QualifiedName Z
  //        ^
  // The caller should have guaranteed that the start pointer is at the
  // above location.
  // Note that type is never a function type, but only the return type of
  // a function or the type of a variable.
  Mangled += 2;

  Mangled = parseQualified(Demangled, Mangled);

  if (Mangled != nullptr) {
    // Artificial symbols end with 'Z' and have no type.
    if (*Mangled == 'Z')
      ++Mangled;
    else {
      Mangled = parseType(Mangled);
    }
  }

  return Mangled;
}

const char *Demangler::parseQualified(OutputBuffer *Demangled,
                                      const char *Mangled) {
  // Qualified names are identifiers separated by their encoded length.
  // Nested functions also encode their argument types without specifying
  // what they return.
  //    QualifiedName:
  //        SymbolFunctionName
  //        SymbolFunctionName QualifiedName
  //        ^
  //    SymbolFunctionName:
  //        SymbolName
  //        SymbolName TypeFunctionNoReturn
  //        SymbolName M TypeFunctionNoReturn
  //        SymbolName M TypeModifiers TypeFunctionNoReturn
  // The start pointer should be at the above location.

  // Whether it has more than one symbol
  size_t NotFirst = false;
  do {
    // Skip over anonymous symbols.
    if (*Mangled == '0') {
      do
        ++Mangled;
      while (*Mangled == '0');

      continue;
    }

    if (NotFirst)
      *Demangled << '.';
    NotFirst = true;

    Mangled = parseIdentifier(Demangled, Mangled);

  } while (Mangled && isSymbolName(Mangled));

  return Mangled;
}

const char *Demangler::parseIdentifier(OutputBuffer *Demangled,
                                       const char *Mangled) {
  unsigned long Len;

  if (Mangled == nullptr || *Mangled == '\0')
    return nullptr;

  if (*Mangled == 'Q')
    return parseSymbolBackref(Demangled, Mangled);

  // TODO: Parse lengthless template instances.

  const char *Endptr = decodeNumber(Mangled, Len);

  if (Endptr == nullptr || Len == 0)
    return nullptr;

  if (strlen(Endptr) < Len)
    return nullptr;

  Mangled = Endptr;

  // TODO: Parse template instances with a length prefix.

  // There can be multiple different declarations in the same function that
  // have the same mangled name.  To make the mangled names unique, a fake
  // parent in the form `__Sddd' is added to the symbol.
  if (Len >= 4 && Mangled[0] == '_' && Mangled[1] == '_' && Mangled[2] == 'S') {
    const char *NumPtr = Mangled + 3;
    while (NumPtr < (Mangled + Len) && std::isdigit(*NumPtr))
      ++NumPtr;

    if (Mangled + Len == NumPtr) {
      // Skip over the fake parent.
      Mangled += Len;
      return parseIdentifier(Demangled, Mangled);
    }

    // Else demangle it as a plain identifier.
  }

  return parseLName(Demangled, Mangled, Len);
}

const char *Demangler::parseType(const char *Mangled) {
  if (*Mangled == '\0')
    return nullptr;

  switch (*Mangled) {
  // TODO: Parse type qualifiers.
  // TODO: Parse function types.
  // TODO: Parse compound types.
  // TODO: Parse delegate types.
  // TODO: Parse tuple types.

  // Basic types.
  case 'i':
    ++Mangled;
    // TODO: Add type name dumping
    return Mangled;

    // TODO: Add support for the rest of the basic types.

  // Back referenced type.
  case 'Q':
    return parseTypeBackref(Mangled);

  default: // unhandled.
    return nullptr;
  }
}

const char *Demangler::parseLName(OutputBuffer *Demangled, const char *Mangled,
                                  unsigned long Len) {
  switch (Len) {
  case 6:
    if (strncmp(Mangled, "__initZ", Len + 1) == 0) {
      // The static initializer for a given symbol.
      Demangled->prepend("initializer for ");
      Demangled->setCurrentPosition(Demangled->getCurrentPosition() - 1);
      Mangled += Len;
      return Mangled;
    }
    if (strncmp(Mangled, "__vtblZ", Len + 1) == 0) {
      // The vtable symbol for a given class.
      Demangled->prepend("vtable for ");
      Demangled->setCurrentPosition(Demangled->getCurrentPosition() - 1);
      Mangled += Len;
      return Mangled;
    }
    break;

  case 7:
    if (strncmp(Mangled, "__ClassZ", Len + 1) == 0) {
      // The classinfo symbol for a given class.
      Demangled->prepend("ClassInfo for ");
      Demangled->setCurrentPosition(Demangled->getCurrentPosition() - 1);
      Mangled += Len;
      return Mangled;
    }
    break;

  case 11:
    if (strncmp(Mangled, "__InterfaceZ", Len + 1) == 0) {
      // The interface symbol for a given class.
      Demangled->prepend("Interface for ");
      Demangled->setCurrentPosition(Demangled->getCurrentPosition() - 1);
      Mangled += Len;
      return Mangled;
    }
    break;

  case 12:
    if (strncmp(Mangled, "__ModuleInfoZ", Len + 1) == 0) {
      // The ModuleInfo symbol for a given module.
      Demangled->prepend("ModuleInfo for ");
      Demangled->setCurrentPosition(Demangled->getCurrentPosition() - 1);
      Mangled += Len;
      return Mangled;
    }
    break;
  }

  *Demangled << StringView(Mangled, Len);
  Mangled += Len;

  return Mangled;
}

Demangler::Demangler(const char *Mangled)
    : Str(Mangled), LastBackref(strlen(Mangled)) {}

const char *Demangler::parseMangle(OutputBuffer *Demangled) {
  return parseMangle(Demangled, this->Str);
}

char *llvm::dlangDemangle(const char *MangledName) {
  if (MangledName == nullptr || strncmp(MangledName, "_D", 2) != 0)
    return nullptr;

  OutputBuffer Demangled;
  if (!initializeOutputBuffer(nullptr, nullptr, Demangled, 1024))
    return nullptr;

  if (strcmp(MangledName, "_Dmain") == 0) {
    Demangled << "D main";
  } else {

    Demangler D = Demangler(MangledName);
    MangledName = D.parseMangle(&Demangled);

    // Check that the entire symbol was successfully demangled.
    if (MangledName == nullptr || *MangledName != '\0') {
      std::free(Demangled.getBuffer());
      return nullptr;
    }
  }

  // OutputBuffer's internal buffer is not null terminated and therefore we need
  // to add it to comply with C null terminated strings.
  if (Demangled.getCurrentPosition() > 0) {
    Demangled << '\0';
    Demangled.setCurrentPosition(Demangled.getCurrentPosition() - 1);
    return Demangled.getBuffer();
  }

  std::free(Demangled.getBuffer());
  return nullptr;
}
