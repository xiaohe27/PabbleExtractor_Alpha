#ifndef MPITypeCheckingConsumer_H
#define MPITypeCheckingConsumer_H

#include <iostream>
#include <unordered_map>
#include "llvm/Support/Host.h"
#include "llvm/ADT/IntrusiveRefCntPtr.h"

#include "clang/Basic/DiagnosticOptions.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Basic/TargetOptions.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/Basic/LangOptions.h"
#include "clang/Parse/Parser.h"
#include "clang/Parse/ParseAST.h"
#include "clang/Analysis/CFG.h"
#include "clang/AST/Stmt.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Analysis/CallGraph.h"
#include "clang/Analysis/AnalysisContext.h"
#include "clang/AST/DeclVisitor.h"
#include "clang/AST/StmtVisitor.h"

using namespace clang;
using namespace std;
using namespace llvm;

class MPITypeCheckingConsumer;





void checkIdTable(clang::CompilerInstance *ci);
string decl2str(SourceManager *sm, LangOptions lopt,clang::Decl *d);
string stmt2str(SourceManager *sm, LangOptions lopt,clang::Stmt *stmt);


class MPITypeCheckingConsumer:
	public ASTConsumer,
	public RecursiveASTVisitor<MPITypeCheckingConsumer>
//	,public DeclVisitor<MPITypeCheckingConsumer>
{
private:
	CompilerInstance *ci;

	FunctionDecl *mainFunc;

	//visitStart is true if we begin to visit the stmts in main function.
	bool visitStart;


public:
	MPITypeCheckingConsumer(CompilerInstance *ci);

	virtual bool HandleTopLevelDecl( DeclGroupRef d);

	void HandleTranslationUnit(ASTContext &Ctx);

	bool TraverseAsmStmt(AsmStmt *S);

	bool TraverseIfStmt(IfStmt *S);

	//bool TraverseDeclStmt(DeclStmt *S);
	bool VisitDeclStmt(DeclStmt *S);


	bool TraverseSwitchStmt(SwitchStmt *S);

	bool TraverseForStmt(ForStmt *S);

	bool TraverseDoStmt(DoStmt *S);

	bool TraverseWhileStmt(WhileStmt *S);

	bool TraverseLabelStmt(LabelStmt *S);

	bool TraverseReturnStmt(ReturnStmt *S);

	bool TraverseNullStmt(NullStmt *S);

//	bool TraverseBinaryOperator(BinaryOperator *op);

	bool TraverseBreakStmt(BreakStmt *S);

	bool TraverseContinueStmt(ContinueStmt *S);

//	bool TraverseCallExpr(CallExpr *op);

	bool VisitCallExpr(CallExpr *op);


//	bool TraverseCompoundStmt(CompoundStmt *S);
	bool VisitBinAndAssign(CompoundAssignOperator *OP);

	bool VisitBinComma(BinaryOperator *S);

	bool VisitBinaryOperator(BinaryOperator *S);

	bool VisitDecl(Decl *);

	bool VisitCompoundStmt(CompoundStmt *cs);
};


#endif
