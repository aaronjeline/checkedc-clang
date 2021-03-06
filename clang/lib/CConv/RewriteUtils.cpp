//=--RewriteUtils.cpp---------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// This class contains implementation of the functions and
// classes of RewriteUtils.h
//===----------------------------------------------------------------------===//

#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/CConv/CastPlacement.h"
#include "clang/CConv/CheckedRegions.h"
#include "clang/CConv/DeclRewriter.h"
#include "clang/Tooling/Refactoring/SourceCode.h"

using namespace llvm;
using namespace clang;

SourceLocation DComp::getDeclBegin(DeclReplacement *D) const {
  SourceLocation Begin =
      (*D->getStatement()->decls().begin())->getSourceRange().getBegin();
  for (const auto &DT : D->getStatement()->decls()) {
    if (DT == D->getDecl())
      return Begin;
    Begin = DT->getSourceRange().getEnd();
  }
  llvm_unreachable("Declaration not found in DeclStmt.");
}

SourceRange DComp::getReplacementSourceRange(DeclReplacement *D) const {
  SourceRange Range = D->getSourceRange(SM);

  // Also take into account whether or not there is a multi-statement
  // decl, because the generated ranges will overlap.
  DeclStmt *LhStmt = D->getStatement();
  if (LhStmt && !LhStmt->isSingleDecl()) {
    SourceLocation NewBegin = getDeclBegin(D);
    Range.setBegin(NewBegin);
    // This is needed to make the subsequent test inclusive.
    Range.setEnd(Range.getEnd().getLocWithOffset(-1));
  }

  return Range;
}

bool DComp::operator()(DeclReplacement *Lhs,
                       DeclReplacement *Rhs) const {
  // Does the source location of the Decl in lhs overlap at all with
  // the source location of rhs?
  SourceRange SrLhs = getReplacementSourceRange(Lhs);
  SourceRange SrRhs = getReplacementSourceRange(Rhs);

  SourceLocation X1 = SrLhs.getBegin();
  SourceLocation X2 = SrLhs.getEnd();
  SourceLocation Y1 = SrRhs.getBegin();
  SourceLocation Y2 = SrRhs.getEnd();

  if (Lhs->getStatement() == nullptr && Rhs->getStatement() == nullptr) {
    // These are global declarations. Get the source location
    // and compare them lexicographically.
    PresumedLoc LHsPLocB = SM.getPresumedLoc(X2);
    PresumedLoc RHsPLocE = SM.getPresumedLoc(Y2);

    // Are both the source location valid?
    if (LHsPLocB.isValid() && RHsPLocE.isValid()) {
      // They are in same fine?
      if (!strcmp(LHsPLocB.getFilename(), RHsPLocE.getFilename())) {
        // Are they in same line?
        if (LHsPLocB.getLine() == RHsPLocE.getLine())
          return LHsPLocB.getColumn() < RHsPLocE.getColumn();

        return LHsPLocB.getLine() < RHsPLocE.getLine();
      }
      return strcmp(LHsPLocB.getFilename(), RHsPLocE.getFilename()) > 0;
    }
    return LHsPLocB.isValid();
  }

  bool Contained =  SM.isBeforeInTranslationUnit(X1, Y2) &&
                    SM.isBeforeInTranslationUnit(Y1, X2);

  if (Contained)
    return false;
  else
    return SM.isBeforeInTranslationUnit(X2, Y1);
}

void GlobalVariableGroups::addGlobalDecl(Decl *VD,
                                         std::set<Decl *> *VDSet) {
  if (VD && GlobVarGroups.find(VD) == GlobVarGroups.end()) {
    if (VDSet == nullptr)
      VDSet = new std::set<Decl *>();
    VDSet->insert(VD);
    GlobVarGroups[VD] = VDSet;
    // Process the next decl.
    Decl *NDecl = VD->getNextDeclInContext();
    if (isa_and_nonnull<VarDecl>(NDecl) || isa_and_nonnull<FieldDecl>(NDecl)) {
      PresumedLoc OrigDeclLoc =
          SM.getPresumedLoc(VD->getSourceRange().getBegin());
      PresumedLoc NewDeclLoc =
          SM.getPresumedLoc(NDecl->getSourceRange().getBegin());
      // Check if both declarations are on the same line.
      if (OrigDeclLoc.isValid() && NewDeclLoc.isValid() &&
          !strcmp(OrigDeclLoc.getFilename(), NewDeclLoc.getFilename()) &&
          OrigDeclLoc.getLine() == NewDeclLoc.getLine())
        addGlobalDecl(dyn_cast<Decl>(NDecl), VDSet);
    }
  }
}


std::set<Decl *> &GlobalVariableGroups::getVarsOnSameLine(Decl *D) {
  assert (GlobVarGroups.find(D) != GlobVarGroups.end() &&
         "Expected to find the group.");
  return *(GlobVarGroups[D]);
}


GlobalVariableGroups::~GlobalVariableGroups() {
  std::set<std::set<Decl *> *> VVisited;
  // Free each of the group.
  for (auto &currV : GlobVarGroups) {
    // Avoid double free by caching deleted sets.
    if (VVisited.find(currV.second) != VVisited.end())
      continue;
    VVisited.insert(currV.second);
    delete (currV.second);
  }
  GlobVarGroups.clear();

}

// Test to see if we can rewrite a given SourceRange.
// Note that R.getRangeSize will return -1 if SR is within
// a macro as well. This means that we can't re-write any
// text that occurs within a macro.
bool canRewrite(Rewriter &R, SourceRange &SR) {
  return SR.isValid() && (R.getRangeSize(SR) != -1);
}

static void emit(Rewriter &R, ASTContext &C, std::set<FileID> &Files,
                 std::string &OutputPostfix) {

  // Check if we are outputing to stdout or not, if we are, just output the
  // main file ID to stdout.
  if (Verbose)
    errs() << "Writing files out\n";

  SourceManager &SM = C.getSourceManager();
  if (OutputPostfix == "-") {
    if (const RewriteBuffer *B = R.getRewriteBufferFor(SM.getMainFileID()))
      B->write(outs());
  } else
    for (const auto &F : Files)
      if (const RewriteBuffer *B = R.getRewriteBufferFor(F))
        if (const FileEntry *FE = SM.getFileEntryForID(F)) {
          assert(FE->isValid());

          // Produce a path/file name for the rewritten source file.
          // That path should be the same as the old one, with a
          // suffix added between the file name and the extension.
          // For example \foo\bar\a.c should become \foo\bar\a.checked.c
          // if the OutputPostfix parameter is "checked" .

          std::string PfName = sys::path::filename(FE->getName()).str();
          std::string DirName = sys::path::parent_path(FE->getName()).str();
          std::string
              FileName = sys::path::remove_leading_dotslash(PfName).str();
          std::string Ext = sys::path::extension(FileName).str();
          std::string Stem = sys::path::stem(FileName).str();
          std::string NFile = Stem + "." + OutputPostfix + Ext;
          if (!DirName.empty())
            NFile = DirName + sys::path::get_separator().str() + NFile;

          // Write this file out if it was specified as a file on the command
          // line.
          std::string FeAbsS = "";
          if (getAbsoluteFilePath(FE->getName(), FeAbsS)) {
            FeAbsS = sys::path::remove_leading_dotslash(FeAbsS);
          }

          if (canWrite(FeAbsS)) {
            std::error_code EC;
            raw_fd_ostream out(NFile, EC, sys::fs::F_None);

            if (!EC) {
              if (Verbose)
                outs() << "writing out " << NFile << "\n";
              B->write(out);
            }
            else
              errs() << "could not open file " << NFile << "\n";
            // This is awkward. What to do? Since we're iterating,
            // we could have created other files successfully. Do we go back
            // and erase them? Is that surprising? For now, let's just keep
            // going.
          }
        }
}

// Rewrites types that inside other expressions. This includes cast expression
// and compound literal expressions.
class TypeExprRewriter
    : public clang::RecursiveASTVisitor<TypeExprRewriter> {
public:
  explicit TypeExprRewriter(ASTContext *C, ProgramInfo &I, Rewriter &R)
      : Context(C), Info(I) , Writer(R) {}

  bool VisitCompoundLiteralExpr(CompoundLiteralExpr *CLE) {
    SourceRange TypeSrcRange(CLE->getBeginLoc().getLocWithOffset(1),
         CLE->getTypeSourceInfo()->getTypeLoc().getEndLoc());
    rewriteType(CLE, TypeSrcRange);
    return true;
  }

  bool VisitCStyleCastExpr(CStyleCastExpr *ECE) {
    SourceRange TypeSrcRange
        (ECE->getBeginLoc().getLocWithOffset(1),
          ECE->getTypeInfoAsWritten()->getTypeLoc().getEndLoc());
    rewriteType(ECE, TypeSrcRange);
    return true;
  }

private:
  ASTContext *Context;
  ProgramInfo &Info;
  Rewriter &Writer;

  void rewriteType(Expr *E, SourceRange &Range) {
    CVarSet CVSingleton = Info.getPersistentConstraintVars(E, Context);
    if (CVSingleton.empty())
      return;
    ConstraintVariable *CV = getOnly(CVSingleton);

    // Only rewrite if the type has changed.
    if (CV->anyChanges(Info.getConstraints().getVariables())){
      // The constraint variable is able to tell us what the new type string
      // should be.
      std::string
          NewType = CV->mkString(Info.getConstraints().getVariables(), false);

      // Replace the original type with this new one
      if (canRewrite(Writer, Range))
        Writer.ReplaceText(Range, NewType);
    }
  }
};

// Adds type parameters to calls to alloc functions.
// The basic assumption this makes is that an alloc function will be surrounded
// by a cast expression giving its type when used as a type other than void*.
class TypeArgumentAdder
  : public clang::RecursiveASTVisitor<TypeArgumentAdder> {
public:
  explicit TypeArgumentAdder(ASTContext *C, ProgramInfo &I, Rewriter &R)
      : Context(C), Info(I), Writer(R) {}

  bool VisitCallExpr(CallExpr *CE) {
    if (isa_and_nonnull<FunctionDecl>(CE->getCalleeDecl())) {
      // If the function call already has type arguments, we'll trust that
      // they're correct and not add anything else.
      if (typeArgsProvided(CE))
        return true;

      if (Info.hasTypeParamBindings(CE, Context)) {
        // Construct a string containing concatenation of all type arguments for
        // the function call.
        std::string TypeParamString;
        for (auto Entry : Info.getTypeParamBindings(CE, Context))
          if (Entry.second != nullptr) {
            std::string TyStr =
                Entry.second->mkString(Info.getConstraints().getVariables(),
                                     false, false, true);
            if (TyStr.back() == ' ')
              TyStr.pop_back();
            TypeParamString += TyStr + ",";
          } else {
            // If it's null, then the type variable was not used consistently,
            // so we can only put void here instead of useful type.
            TypeParamString += "void,";
          }
        TypeParamString.pop_back();

        SourceLocation TypeParamLoc = getTypeArgLocation(CE);
        Writer.InsertTextAfter(TypeParamLoc, "<" + TypeParamString + ">");
      }
    }
    return true;
  }

private:
  ASTContext *Context;
  ProgramInfo &Info;
  Rewriter &Writer;

  // Attempt to find the right spot to insert the type arguments. This should be
  // directly after the name of the function being called.
  SourceLocation getTypeArgLocation(CallExpr *Call) {
    Expr *Callee = Call->getCallee()->IgnoreImpCasts();
    if (DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(Callee)) {
      size_t NameLength = DRE->getNameInfo().getAsString().length();
      return Call->getBeginLoc().getLocWithOffset(NameLength);
    }
    llvm_unreachable("Could find SourceLocation for type arguments!");
  }

  // Check if type arguments have already been provided for this function
  // call so that we don't mess with anything already there.
  bool typeArgsProvided(CallExpr *Call) {
    Expr *Callee = Call->getCallee()->IgnoreImpCasts();
    if (DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(Callee)) {
      // ArgInfo is null if there are no type arguments in the program
      if (auto *ArgInfo = DRE->GetTypeArgumentInfo())
        for (auto TypeArg : ArgInfo->typeArgumentss())
          if (!TypeArg.typeName->isVoidType())
            return true;
      return false;
    }
    // We only handle direct calls, so there must be a DeclRefExpr.
    llvm_unreachable("Callee of function call is not DeclRefExpr.");
  }
};


std::string ArrayBoundsRewriter::getBoundsString(PVConstraint *PV,
                                                 Decl *D, bool Isitype) {
  auto &ABInfo = Info.getABoundsInfo();

  // Try to find a bounds key for the constraint variable. If we can't,
  // ValidBKey is set to false, indicating that DK has not been initialized.
  BoundsKey DK;
  bool ValidBKey = true;
  if (PV->hasBoundsKey())
    DK = PV->getBoundsKey();
  else if(!ABInfo.tryGetVariable(D, DK))
    ValidBKey = false;

  std::string BString = "";
  // For itype we do not want to add a second ":".
  std::string Pfix = Isitype ? " " : " : ";

  if (ValidBKey) {
    ABounds *ArrB = ABInfo.getBounds(DK);
    if (ArrB != nullptr) {
      BString = ArrB->mkString(&ABInfo);
      if (!BString.empty())
        BString = Pfix + BString;
    }
  }
  if (BString.empty() && PV->hasBoundsStr()) {
    BString = Pfix + PV->getBoundsStr();
  }
  return BString;
}

void RewriteConsumer::HandleTranslationUnit(ASTContext &Context) {
  Info.enterCompilationUnit(Context);

  // Rewrite Variable declarations
  Rewriter R(Context.getSourceManager(), Context.getLangOpts());
  std::set<FileID> TouchedFiles;
  DeclRewriter::rewriteDecls(Context, Info, R, TouchedFiles);

  // Take care of some other rewriting tasks
  std::set<llvm::FoldingSetNodeID> Seen;
  std::map<llvm::FoldingSetNodeID, AnnotationNeeded> NodeMap;
  CheckedRegionFinder CRF(&Context, R, Info, Seen, NodeMap);
  CheckedRegionAdder CRA(&Context, R, NodeMap);
  CastPlacementVisitor ECPV(&Context, Info, R);
  TypeExprRewriter TER(&Context, Info, R);
  TypeArgumentAdder TPA(&Context, Info, R);
  TranslationUnitDecl *TUD = Context.getTranslationUnitDecl();
  for (const auto &D : TUD->decls()) {
    ECPV.TraverseDecl(D);
    if (AddCheckedRegions) {
      // Adding checked regions enabled!?
      // TODO: Should checked region finding happen somewhere else? This is
      //       supposed to be rewriting.
      CRF.TraverseDecl(D);
      CRA.TraverseDecl(D);
    }
    TER.TraverseDecl(D);
    TPA.TraverseDecl(D);
  }

  // Output files.
  emit(R, Context, TouchedFiles, OutputPostfix);

  Info.exitCompilationUnit();
  return;
}
