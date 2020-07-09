//=--Utils.h------------------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// Type declarations for map data structures and other general helper methods.
//===----------------------------------------------------------------------===//

#ifndef _UTILS_H
#define _UTILS_H
#include <set>
#include <ctime>
#include <unordered_map>
#include <map>
#include <unordered_set>
#include "llvm/Support/CommandLine.h"
#include "clang/AST/Type.h"
#include "llvm/Support/Casting.h"

#include "PersistentSourceLoc.h"

class ConstraintVariable;
class ProgramInfo;

// Maps a Decl to the set of constraint variables for that Decl.
typedef std::map<PersistentSourceLoc, 
  std::set<ConstraintVariable *>> VariableMap;

// Maps a Decl to the DeclStmt that defines the Decl.
typedef std::map<clang::Decl *, clang::DeclStmt *> VariableDecltoStmtMap;



extern std::set<std::string> FilePaths;

template <typename T>
T getOnly(std::set<T> &singletonSet) {
  assert(singletonSet.size() == 1);
  return (*singletonSet.begin());
}


const clang::Type *getNextTy(const clang::Type *Ty);

clang::FunctionDecl *getDeclaration(clang::FunctionDecl *FD);

clang::FunctionDecl *getDefinition(clang::FunctionDecl *FD);

clang::CheckedPointerKind
getCheckedPointerKind(clang::InteropTypeExpr *ItypeExpr);

bool hasFunctionBody(clang::Decl *D);

std::string getStorageQualifierString(clang::Decl *D);

bool getAbsoluteFilePath(std::string FileName, std::string &AbsoluteFp);

bool isNULLExpression(clang::Expr *E, clang::ASTContext &C);

// Get the time spent in seconds since the provided time stamp.
float getTimeSpentInSeconds(clock_t StartTime);

// Check if the function has varargs i.e., foo(<named_arg>,...)
bool functionHasVarArgs(clang::FunctionDecl *FD);

// Check if the function is a allocator.
bool isFunctionAllocator(std::string FuncName);

// Is the given variable built  in type?
bool isPointerType(clang::VarDecl *VD);

// Check if provided type is a var arg type?
bool isVarArgType(const std::string &TypeName);

// Check if the variable is of a structure or union type.
bool isStructOrUnionType(clang::VarDecl *VD);

// Check if two source locations overlap
bool contains(clang::SourceRange haystack, clang::SourceRange needle);

bool isSelfContainedStruct(clang::TypedefDecl *);

// Helper method to print a Type in a way that can be represented in the source.
std::string tyToStr(const clang::Type *T);

// Get the end source location of the end of the provided function.
clang::SourceLocation getFunctionDeclarationEnd(clang::FunctionDecl *FD,
                                                clang::SourceManager &S);

// Remove auxillary casts from the provided expression.
clang::Expr *removeAuxillaryCasts(clang::Expr *SrcExpr);

// Get normalized expression by removing clang syntactic sugar
// clang::Expr *getNormalizedExpr(clang::Expr *CE);

// OK to cast from Src to Dst?
bool isCastSafe(clang::QualType DstType,
                clang::QualType SrcType);

// Check if the provided file path belongs to the input project
// and can be rewritten.
bool canWrite(const std::string &FilePath);

// Check if the provided variable has void as one of its type.
bool hasVoidType(clang::ValueDecl *D);
// Check if the provided type has void as one of its type
bool isTypeHasVoid(clang::QualType QT);

// Check if the provided declaration is in system header.
bool isInSysHeader(clang::Decl *D);

// Find the longest common subsequence.
unsigned longestCommonSubsequence(const char *Str1, const char *Str2,
                                  unsigned long Str1Len, unsigned long Str2Len);

#endif
