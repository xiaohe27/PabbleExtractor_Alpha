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





void checkIdTable(clang::CompilerInstance *ci);
string decl2str(SourceManager *sm, LangOptions lopt,Decl *d);
string decl2str(SourceManager *sm, LangOptions lopt,const Decl *d);
string stmt2str(SourceManager *sm, LangOptions lopt,Stmt *stmt);
string stmt2str(SourceManager *sm, LangOptions lopt,const Stmt *stmt);
string expr2str(SourceManager *sm, LangOptions lopt,Expr *stmt);
string delSpaces(string &str);

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

	int numOfProcs;

	CommManager *commManager;


public:
	MPITypeCheckingConsumer(CompilerInstance *ci);

	MPITypeCheckingConsumer(CompilerInstance *ci, int numOfProc);

	virtual bool HandleTopLevelDecl( DeclGroupRef d);

	void HandleTranslationUnit(ASTContext &Ctx);

	bool checkWhetherTheDeclHasBeenVisitedBefore(FunctionDecl *decl);

	void analyzeDecl(FunctionDecl *decl);

	void removeFuncFromList();

	//return the list of non-rank var names that have been inserted to the formal stack
	vector<string> analyzeNonRankVarCond(map<string,stack<Condition>> tmpNonRankVarCondMap);

	//remove the non-rank var from the stack after visiting the relevant block
	void removeNonRankVarCondInStack(vector<string> stackOfNonRankVarNames);

	//remove the old non-rank var from the stack and inserting the negation
	void removeAndAddNewNonRankVarCondInStack(vector<string> stackOfNonRankVarNames);

	//return the expected iteration numbers. If it is not clear, then -1 will be returned
	int analyzeForStmt(Stmt* initStmt, Expr* condExpr, Expr* incExpr, Stmt* body, vector<string> nonRankVarList);

	void printTheTree(){cout<<this->commManager->printTheTree()<<endl;}

	bool isChangingByOneUnit(Expr *inc);
	string getVarInIncExpr(Expr *inc);






	bool VisitAsmStmt(AsmStmt *S);

	bool TraverseIfStmt(IfStmt *S);

	bool VisitDeclStmt(DeclStmt *S);

	bool VisitSwitchStmt(SwitchStmt *S);

	bool TraverseForStmt(ForStmt *S);

	bool VisitDoStmt(DoStmt *S);

	bool TraverseWhileStmt(WhileStmt *S);

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
