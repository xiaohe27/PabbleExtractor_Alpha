#ifndef MPITypeCheckingConsumer_H
#define MPITypeCheckingConsumer_H

#include <iostream>
#include <unordered_map>
#include <algorithm>


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

class FunctionRecursionError{
private: string errInfo;

public:
		 FunctionRecursionError::FunctionRecursionError(string err){
			this->errInfo=err;
		 }

		 void printErrInfo(){
			cout<<errInfo<<endl;
		 }
};





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

	//the list holds all the functions which have not finished execution
	list<string> funcsList;


public:
	MPITypeCheckingConsumer(CompilerInstance *ci);

	virtual bool HandleTopLevelDecl( DeclGroupRef d);

	void HandleTranslationUnit(ASTContext &Ctx);

	bool checkWhetherTheDeclHasBeenVisitedBefore(FunctionDecl *decl);

	void analyzeDecl(FunctionDecl *decl);




	bool VisitAsmStmt(AsmStmt *S);

	bool VisitIfStmt(IfStmt *S);

	bool VisitDeclStmt(DeclStmt *S);

	bool VisitSwitchStmt(SwitchStmt *S);

	bool VisitForStmt(ForStmt *S);

	bool VisitDoStmt(DoStmt *S);

	bool VisitWhileStmt(WhileStmt *S);

	bool VisitLabelStmt(LabelStmt *S);

	bool VisitReturnStmt(ReturnStmt *S);

	bool VisitNullStmt(NullStmt *S);

	bool VisitBreakStmt(BreakStmt *S);

	bool VisitContinueStmt(ContinueStmt *S);

	bool VisitCallExpr(CallExpr *op);

	bool VisitBinAndAssign(CompoundAssignOperator *OP);

	bool VisitBinaryOperator(BinaryOperator *S);

	bool VisitDecl(Decl *decl);

	bool VisitFunctionDecl(FunctionDecl *funcDecl);
};


#endif
