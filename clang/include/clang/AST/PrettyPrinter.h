//===--- PrettyPrinter.h - Classes for aiding with AST printing -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file defines helper types for AST pretty-printing.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_AST_PRETTYPRINTER_H
#define LLVM_CLANG_AST_PRETTYPRINTER_H

#include "clang/Basic/LLVM.h"
#include "clang/Basic/LangOptions.h"

namespace clang {

class BuiltinType;
class CallExpr;
class CompoundStmt;
class CXXOperatorCallExpr;
class CXXTemporaryObjectExpr;
class DeclRefExpr;
class Expr;
class LangOptions;
class MemberExpr;
class SourceManager;
class Stmt;
class TagDecl;

class PrinterHelper {
public:
  virtual ~PrinterHelper();
  virtual bool handledStmt(Stmt *E, raw_ostream &OS) = 0;
};

/// Callbacks to use to customize the behavior of the pretty-printer.
class PrintingCallbacks {
protected:
  ~PrintingCallbacks() = default;

public:
  /// Remap a path to a form suitable for printing.
  virtual std::string remapPath(StringRef Path) const {
    return std::string(Path);
  }

  /// Override name used for a BuiltinType.
  virtual StringRef overrideBuiltinTypeName(const BuiltinType *T) const {
    return {};
  }

  /// Override name used in a TagDecl.
  virtual StringRef overrideTagDeclIdentifier(TagDecl *D) const { return {}; }

  /// Override name used in a call (including CXXMemberCallExprs).
  virtual StringRef overrideBuiltinFunctionIdentifier(CallExpr *C) const {
    return {};
  }

  /// Incrementally build call arguments using either supplied lambda.
  /// Returning false will apply default arguments instead.
  virtual bool
  overrideCallArguments(CallExpr *C,
                        const std::function<void(StringRef)> &StringArg,
                        const std::function<void(Expr *)> &ExprArg) const {
    return false;
  }

  /// Incrementally print operator call to replace default grammar.
  virtual bool
  overrideCXXOperatorCall(CXXOperatorCallExpr *C, raw_ostream &OS,
                          const std::function<void(Expr *)> &ExprArg) const {
    return false;
  }

  /// Incrementally print temporary object expr replace default grammar.
  virtual bool overrideCXXTemporaryObjectExpr(
      CXXTemporaryObjectExpr *C, raw_ostream &OS,
      const std::function<void(Expr *)> &ExprArg) const {
    return false;
  }

  /// Override name used in a DeclRefExpr.
  virtual StringRef overrideDeclRefIdentifier(DeclRefExpr *DR) const {
    return {};
  }

  /// Prepend string to member expr base, optionally replacing it entirely.
  virtual StringRef prependMemberExprBase(MemberExpr *ME,
                                          bool &ReplaceBase) const {
    return {};
  }

  /// Print member access operator as underscore in case it is flattened into
  /// an identifier.
  virtual bool shouldPrintMemberExprUnderscore(MemberExpr *ME) const {
    return false;
  }

  /// Opportunity to print something before first statement.
  virtual void
  printCompoundStatementBefore(const std::function<raw_ostream &()> &Indent,
                               CompoundStmt *CS) const {}

  /// Opportunity to print something after last statement.
  virtual void
  printCompoundStatementAfter(const std::function<raw_ostream &()> &Indent,
                              CompoundStmt *CS) const {}
};

/// Describes how types, statements, expressions, and declarations should be
/// printed.
///
/// This type is intended to be small and suitable for passing by value.
/// It is very frequently copied.
struct PrintingPolicy {
  /// Create a default printing policy for the specified language.
  PrintingPolicy(const LangOptions &LO)
      : Indentation(2), SuppressSpecifiers(false),
        SuppressTagKeyword(LO.CPlusPlus), IncludeTagDefinition(false),
        SuppressScope(false), SuppressUnwrittenScope(false),
        SuppressInitializers(false), ConstantArraySizeAsWritten(false),
        AnonymousTagLocations(true), SuppressStrongLifetime(false),
        SuppressLifetimeQualifiers(false),
        SuppressTemplateArgsInCXXConstructors(false), Bool(LO.Bool),
        Restrict(LO.C99), Alignof(LO.CPlusPlus11), UnderscoreAlignof(LO.C11),
        UseVoidForZeroParams(!LO.CPlusPlus), TerseOutput(false),
        PolishForDeclaration(false), Half(LO.Half),
        MSWChar(LO.MicrosoftExt && !LO.WChar), IncludeNewlines(true),
        MSVCFormatting(false), ConstantsAsWritten(false),
        SuppressImplicitBase(false), FullyQualifiedName(false),
        PrintCanonicalTypes(false), SuppressNestedQualifiers(false),
        SuppressListInitialization(false), SeparateConditionVarDecls(false),
        ConstantExprsAsInt(false), SilentNullStatement(false),
        NeverSuppressScope(false) {}

  /// Adjust this printing policy for cases where it's known that we're
  /// printing C++ code (for instance, if AST dumping reaches a C++-only
  /// construct). This should not be used if a real LangOptions object is
  /// available.
  void adjustForCPlusPlus() {
    SuppressTagKeyword = true;
    Bool = true;
    UseVoidForZeroParams = false;
  }

  /// The number of spaces to use to indent each line.
  unsigned Indentation : 8;

  /// Whether we should suppress printing of the actual specifiers for
  /// the given type or declaration.
  ///
  /// This flag is only used when we are printing declarators beyond
  /// the first declarator within a declaration group. For example, given:
  ///
  /// \code
  /// const int *x, *y;
  /// \endcode
  ///
  /// SuppressSpecifiers will be false when printing the
  /// declaration for "x", so that we will print "int *x"; it will be
  /// \c true when we print "y", so that we suppress printing the
  /// "const int" type specifier and instead only print the "*y".
  unsigned SuppressSpecifiers : 1;

  /// Whether type printing should skip printing the tag keyword.
  ///
  /// This is used when printing the inner type of elaborated types,
  /// (as the tag keyword is part of the elaborated type):
  ///
  /// \code
  /// struct Geometry::Point;
  /// \endcode
  unsigned SuppressTagKeyword : 1;

  /// When true, include the body of a tag definition.
  ///
  /// This is used to place the definition of a struct
  /// in the middle of another declaration as with:
  ///
  /// \code
  /// typedef struct { int x, y; } Point;
  /// \endcode
  unsigned IncludeTagDefinition : 1;

  /// Suppresses printing of scope specifiers.
  unsigned SuppressScope : 1;

  /// Suppress printing parts of scope specifiers that don't need
  /// to be written, e.g., for inline or anonymous namespaces.
  unsigned SuppressUnwrittenScope : 1;

  /// Suppress printing of variable initializers.
  ///
  /// This flag is used when printing the loop variable in a for-range
  /// statement. For example, given:
  ///
  /// \code
  /// for (auto x : coll)
  /// \endcode
  ///
  /// SuppressInitializers will be true when printing "auto x", so that the
  /// internal initializer constructed for x will not be printed.
  unsigned SuppressInitializers : 1;

  /// Whether we should print the sizes of constant array expressions as written
  /// in the sources.
  ///
  /// This flag determines whether array types declared as
  ///
  /// \code
  /// int a[4+10*10];
  /// char a[] = "A string";
  /// \endcode
  ///
  /// will be printed as written or as follows:
  ///
  /// \code
  /// int a[104];
  /// char a[9] = "A string";
  /// \endcode
  unsigned ConstantArraySizeAsWritten : 1;

  /// When printing an anonymous tag name, also print the location of that
  /// entity (e.g., "enum <anonymous at t.h:10:5>"). Otherwise, just prints
  /// "(anonymous)" for the name.
  unsigned AnonymousTagLocations : 1;

  /// When true, suppress printing of the __strong lifetime qualifier in ARC.
  unsigned SuppressStrongLifetime : 1;

  /// When true, suppress printing of lifetime qualifier in ARC.
  unsigned SuppressLifetimeQualifiers : 1;

  /// When true, suppresses printing template arguments in names of C++
  /// constructors.
  unsigned SuppressTemplateArgsInCXXConstructors : 1;

  /// Whether we can use 'bool' rather than '_Bool' (even if the language
  /// doesn't actually have 'bool', because, e.g., it is defined as a macro).
  unsigned Bool : 1;

  /// Whether we can use 'restrict' rather than '__restrict'.
  unsigned Restrict : 1;

  /// Whether we can use 'alignof' rather than '__alignof'.
  unsigned Alignof : 1;

  /// Whether we can use '_Alignof' rather than '__alignof'.
  unsigned UnderscoreAlignof : 1;

  /// Whether we should use '(void)' rather than '()' for a function prototype
  /// with zero parameters.
  unsigned UseVoidForZeroParams : 1;

  /// Provide a 'terse' output.
  ///
  /// For example, in this mode we don't print function bodies, class members,
  /// declarations inside namespaces etc.  Effectively, this should print
  /// only the requested declaration.
  unsigned TerseOutput : 1;

  /// When true, do certain refinement needed for producing proper declaration
  /// tag; such as, do not print attributes attached to the declaration.
  ///
  unsigned PolishForDeclaration : 1;

  /// When true, print the half-precision floating-point type as 'half'
  /// instead of '__fp16'
  unsigned Half : 1;

  /// When true, print the built-in wchar_t type as __wchar_t. For use in
  /// Microsoft mode when wchar_t is not available.
  unsigned MSWChar : 1;

  /// When true, include newlines after statements like "break", etc.
  unsigned IncludeNewlines : 1;

  /// Use whitespace and punctuation like MSVC does. In particular, this prints
  /// anonymous namespaces as `anonymous namespace' and does not insert spaces
  /// after template arguments.
  unsigned MSVCFormatting : 1;

  /// Whether we should print the constant expressions as written in the
  /// sources.
  ///
  /// This flag determines whether constants expressions like
  ///
  /// \code
  /// 0x10
  /// 2.5e3
  /// \endcode
  ///
  /// will be printed as written or as follows:
  ///
  /// \code
  /// 0x10
  /// 2.5e3
  /// \endcode
  unsigned ConstantsAsWritten : 1;

  /// When true, don't print the implicit 'self' or 'this' expressions.
  unsigned SuppressImplicitBase : 1;

  /// When true, print the fully qualified name of function declarations.
  /// This is the opposite of SuppressScope and thus overrules it.
  unsigned FullyQualifiedName : 1;

  /// Whether to print types as written or canonically.
  unsigned PrintCanonicalTypes : 1;

  unsigned SuppressNestedQualifiers : 1;

  unsigned SuppressListInitialization : 1;

  unsigned SeparateConditionVarDecls : 1;

  unsigned ConstantExprsAsInt : 1;

  unsigned SilentNullStatement : 1;

  unsigned NeverSuppressScope : 1;

  /// Callbacks to use to allow the behavior of printing to be customized.
  const PrintingCallbacks *Callbacks = nullptr;
};

} // end namespace clang

#endif
