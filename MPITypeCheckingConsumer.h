#ifndef MPITypeCheckingConsumer_H
#define MPITypeCheckingConsumer_H

#include <iostream>
#include <map>
#include <algorithm>
#include <vector>
#include "Comm.h"




using namespace clang;
using namespace std;
using namespace llvm;

class MPITypeCheckingConsumer;

class FunctionRecursionError{
private: string errInfo;

public:
		 FunctionRecursionError(string err){
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

	string rankVar;

	string numOfProcessesVar;

	CommManager *commManager;




public:
	MPITypeCheckingConsumer(CompilerInstance *ci);

	virtual bool HandleTopLevelDecl( DeclGroupRef d);

	void HandleTranslationUnit(ASTContext &Ctx);

	bool checkWhetherTheDeclHasBeenVisitedBefore(FunctionDecl *decl);

	void analyzeDecl(FunctionDecl *decl);

	void removeFuncFromList();

	//translate to a special format so that the class Property can understand the string.
	string extractConditionStrFromExpr(Expr* expr){return nullptr;}

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
