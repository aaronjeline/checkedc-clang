//=--ProgramInfo.h------------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// This class represents all the information about a source file
// collected by the converter.
//===----------------------------------------------------------------------===//

#ifndef _PROGRAM_INFO_H
#define _PROGRAM_INFO_H
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/Tooling.h"

#include "ConstraintVariables.h"
#include "Utils.h"
#include "PersistentSourceLoc.h"
#include "ArrayBoundsInformation.h"
#include "CConvInteractiveData.h"
#include "GatherTypes.h"


class ProgramInfo;

class ProgramInfo {
public:
  typedef std::map<std::string, std::map<std::string, std::set<FVConstraint *>>>
      StaticFunctionMapType;

  typedef std::map<std::string, std::set<FVConstraint *>>
      ExternalFunctionMapType;

  ProgramInfo();
  void print(llvm::raw_ostream &O) const;
  void dump() const { print(llvm::errs()); }
  void dump_json(llvm::raw_ostream &O) const;
  void dump_stats(std::set<std::string> &F) { print_stats(F, llvm::errs()); }
  void print_stats(std::set<std::string> &F, llvm::raw_ostream &O,
                   bool OnlySummary =false);

  // Populate Variables, VarDeclToStatement, RVariables, and DepthMap with
  // AST data structures that correspond do the data stored in PDMap and 
  // ReversePDMap. 
  void enterCompilationUnit(clang::ASTContext &Context);

  // Remove any references we maintain to AST data structure pointers. 
  // After this, the Variables, VarDeclToStatement, RVariables, and DepthMap
  // should all be empty. 
  void exitCompilationUnit();

  // For each pointer type in the declaration of D, add a variable to the 
  // constraint system for that pointer type.
  void addVariable(clang::DeclaratorDecl *D, clang::ASTContext *astContext);
  void addVariable(clang::TypedefNameDecl*D, clang::ASTContext *astContext);
  void addCompoundLiteral(CompoundLiteralExpr *CLE, ASTContext *AstContext);

  // Get constraint variable for the provided Decl
  std::set<ConstraintVariable *> getVariable(clang::Decl *D,
                                             clang::ASTContext *C);
  std::set<ConstraintVariable *> getCompoundLiteral(CompoundLiteralExpr *CLE, ASTContext *C);

  ConstraintVariable * getTypedefVar(clang::TypedefNameDecl *D, clang::ASTContext *C);

  // Retrieve a function's constraints by decl, or by name; nullptr if not found
  std::set<FVConstraint *> *getFuncConstraints(FunctionDecl *D, ASTContext *C);
  std::set<FVConstraint *> *getExtFuncDefnConstraintSet(std::string FuncName);
  std::set<FVConstraint *> *getStaticFuncConstraintSet(std::string FuncName, std::string FileName);

  // Check if the given function is an extern function.
  bool isAnExternFunction(const std::string &FName);

  // Called when we are done adding constraints and visiting ASTs.
  // Links information about global symbols together and adds
  // constraints where appropriate.
  bool link();

  VariableMap &getVarMap() { return Variables; }
  Constraints &getConstraints() { return CS;  }

  // Parameter map is used for cast insertion, post-rewriting
  void merge_MF(ParameterMap &MF);
  ParameterMap &get_MF();

  ArrayBoundsInformation &getArrayBoundsInformation() {
    return *ArrBoundsInfo;
  }

  DisjointSet &getPointerConstraintDisjointSet() {
    return ConstraintDisjointSet;
  }
  bool computePointerDisjointSet();

  ExternalFunctionMapType &getExternFuncDefFVMap() {
    return ExternalFunctionFVCons;
  }

  StaticFunctionMapType &getStaticFuncDefFVMap() {
    return StaticFunctionFVCons;
  }

private:
  // List of all constraint variables, indexed by their location in the source.
  // This information persists across invocations of the constraint analysis
  // from compilation unit to compilation unit.
  VariableMap Variables;
  // Constraint system.
  Constraints CS;
  // Is the ProgramInfo persisted? Only tested in asserts. Starts at true.
  bool persisted;

  // Map of global decls for which we don't have a body, the keys are
  // names of external functions/vars, the value is whether the body/def
  // has been seen before.
  std::map<std::string, bool> ExternFunctions;
  std::map<std::string, bool> ExternGVars;
  // Map for typedef variables
  std::map<PersistentSourceLoc, ConstraintVariable*> typedefs;

  // Maps for global/static functions, global variables
  ExternalFunctionMapType ExternalFunctionFVCons;
  StaticFunctionMapType StaticFunctionFVCons;
  std::map<std::string, std::set<PVConstraint *>> GlobalVariableSymbols;

  ParameterMap MF;
  // Object that contains all the bounds information of various
  // array variables.
  ArrayBoundsInformation *ArrBoundsInfo;
  // Disjoint sets for constraints.
  DisjointSet ConstraintDisjointSet;

  // Function to check if an external symbol is okay to leave
  // constrained.
  bool isExternOkay(std::string Ext);

  // Insert the given FVConstraint* set into the provided Map.
  // Returns true if successful else false.
  bool insertIntoExternalFunctionMap(ExternalFunctionMapType &Map,
                                     const std::string &FuncName,
                                     std::set<FVConstraint *> &ToIns);

  // Inserts the given FVConstraint* set into the provided static map.
  // Returns true if successful else false.
  bool insertIntoStaticFunctionMap(StaticFunctionMapType &Map,
                                   const std::string &FuncName,
                                   const std::string &FileName,
                                   std::set<FVConstraint *> &ToIns);


  // Special-case handling for decl introductions. For the moment this covers:
  //  * void-typed variables
  //  * va_list-typed variables
  void specialCaseVarIntros(ValueDecl *D, ASTContext *Context);

  // Inserts the given FVConstraint* set into the global map, depending
  // on whether static or not; returns true on success
  bool
  insertNewFVConstraints(FunctionDecl *FD, std::set<FVConstraint *> &FVcons,
                         ASTContext *C);

  // Retrieves a FVConstraint* based on the decl (which could be static,
  //   or global)
  std::set<FVConstraint *> *getFuncFVConstraints(FunctionDecl *FD,
                                                 ASTContext *C);
  void constrainWildIfMacro(std::set<ConstraintVariable *> S,
                            SourceLocation Location);
};

#endif
