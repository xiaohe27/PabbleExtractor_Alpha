#include "MPITypeCheckingConsumer.h"

using namespace clang;
using namespace llvm;
using namespace std;

/*
bool MPITypeCheckingConsumer::TraverseCompoundStmt(CompoundStmt *cs){
	cout <<"Call Compound stmt" <<endl;
	Stmt **stmtIt;

	int count=0;
	for(stmtIt=cs->body_begin(); stmtIt!=cs->body_end();stmtIt++){

		Stmt *curS= *stmtIt;

		cout << "stmt " << (++count) <<" : " <<curS->getStmtClassName()<< endl;


		if(isa<BinaryOperator>(curS)){
			BinaryOperator *binop = llvm::cast<clang::BinaryOperator>(curS);
			this->TraverseBinaryOperator(binop);
		}

		else
			this->TraverseStmt(curS);

	}


	return true;
}
*/


bool MPITypeCheckingConsumer::VisitCompoundStmt(CompoundStmt *cs){
	cout <<"Visit Compound stmt" <<endl;
	Stmt **stmtIt;

	int count=0;
	for(stmtIt=cs->body_begin(); stmtIt!=cs->body_end();stmtIt++){

		Stmt *curS= *stmtIt;

		cout << "stmt " << (++count) <<" : " <<curS->getStmtClassName()<< endl;


		if(isa<BinaryOperator>(curS)){
			BinaryOperator *binop = llvm::cast<clang::BinaryOperator>(curS);
			this->VisitBinaryOperator(binop);
		}

		else
			this->VisitStmt(curS);

	}


	return true;
}



bool MPITypeCheckingConsumer::VisitBinaryOperator(BinaryOperator *op){
	if(!this->visitStart)
		return true;

	cout <<"Visit '"<< op->getStmtClassName()<<"':\n" ;
	cout << stmt2str(&ci->getSourceManager(),ci->getLangOpts(),op) <<endl;

	Expr *lhs=op->getLHS();
	if (DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(lhs)) {
		// It's a reference to a declaration...
		if (VarDecl *VD = dyn_cast<VarDecl>(DRE->getDecl())) {
			// It's a reference to a variable (a local, function parameter, global, or static data member).
			std::cout << "LHS is " << VD->getQualifiedNameAsString() << std::endl;
		}
	}

	APSInt Result;
	Expr *rhs=op->getRHS();

	cout << "RHS has textual content: \n"<<stmt2str(&ci->getSourceManager(),ci->getLangOpts(),rhs)<<"\n"<<endl;

			
	if (rhs->EvaluateAsInt(Result, this->ci->getASTContext())) {
		std::cout << "RHS has value " << Result.toString(10) << std::endl;
	}
	return true;
}

bool MPITypeCheckingConsumer::VisitBinAndAssign(CompoundAssignOperator *op){
	if(!this->visitStart)
		return true;

	cout <<"Call Compound Ass Op" <<endl;
	return true;
}

bool MPITypeCheckingConsumer::VisitBinComma(BinaryOperator *binOP){
	cout <<"Call Compound Ass Op" <<endl;
	return true;
}



bool MPITypeCheckingConsumer::TraverseAsmStmt(AsmStmt *S){
	cout <<"The Asm stmt is \n"<<stmt2str(&ci->getSourceManager(),ci->getLangOpts(),S) <<endl;

	return true;
}

bool MPITypeCheckingConsumer::TraverseIfStmt(IfStmt *ifStmt){
	if(!this->visitStart)
		return true;

cout <<"The if stmt is \n"<<stmt2str(&ci->getSourceManager(),ci->getLangOpts(), ifStmt) <<endl;

	VarDecl *ifCondVar=ifStmt->getConditionVariable();


	if (ifCondVar)
	{
		cout <<"The var in if part is " << ifCondVar->getIdentifier()->getName().data() <<endl;
	}
	
	//cout << "The body of the var decl "<<ifCondVar->getName().data()<<" is \n" <<
	//	decl2str(&ci->getSourceManager(),ci->getLangOpts(),ifCondVar) <<endl;


	//visit then part
	this->TraverseStmt(ifStmt->getThen());
	

	//visit else part
	this->TraverseStmt(ifStmt->getElse());
	
	return true;
}



/*
bool MPITypeCheckingConsumer::TraverseDeclStmt(DeclStmt *S){
	cout <<"The decl stmt is \n"<<stmt2str(&ci->getSourceManager(),ci->getLangOpts(),S) <<endl;
	return true;
}

*/

bool MPITypeCheckingConsumer::VisitDeclStmt(DeclStmt *S){
	if(!this->visitStart)
		return true;

	cout <<"The decl stmt is \n"<<stmt2str(&ci->getSourceManager(),ci->getLangOpts(),S) <<endl;
	return true;
}


bool MPITypeCheckingConsumer::TraverseSwitchStmt(SwitchStmt *S){
	cout <<"Call Switch stmt" <<endl;
	return true;
}

bool MPITypeCheckingConsumer::TraverseForStmt(ForStmt *S){
	cout <<"The for stmt is \n"<<stmt2str(&ci->getSourceManager(),ci->getLangOpts(),S) <<endl;
	return true;
}

bool MPITypeCheckingConsumer::TraverseDoStmt(DoStmt *S){
	cout <<"Call Do stmt" <<endl;
	return true;
}

bool MPITypeCheckingConsumer::TraverseWhileStmt(WhileStmt *S){
	cout <<"The While stmt is \n"<<stmt2str(&ci->getSourceManager(),ci->getLangOpts(),S) <<endl;
	return true;
}

bool MPITypeCheckingConsumer::TraverseLabelStmt(LabelStmt *S){
	cout <<"Call Label stmt" <<endl;
	return true;
}

bool MPITypeCheckingConsumer::TraverseReturnStmt(ReturnStmt *S){
	cout <<"The return stmt is \n"<<stmt2str(&ci->getSourceManager(),ci->getLangOpts(),S) <<endl;
	return true;
}

bool MPITypeCheckingConsumer::TraverseNullStmt(NullStmt *S){
	cout <<"Call Null stmt" <<endl;
	return true;
}




bool MPITypeCheckingConsumer::TraverseBreakStmt(BreakStmt *S){
	cout <<"Call Break stmt" <<endl;
	return true;
}

bool MPITypeCheckingConsumer::TraverseContinueStmt(ContinueStmt *S){
	cout <<"Call Continue stmt" <<endl;
	return true;
}


bool MPITypeCheckingConsumer::VisitCallExpr(CallExpr *op){
	//if we haven't started to visit the main function, then do nothing.
	if(!this->visitStart)
		return true;

	cout <<"The function being visited is "<<stmt2str(&ci->getSourceManager(),ci->getLangOpts(),op) <<endl;

	Decl *decl=op->getCalleeDecl();
	cout <<"It is a "<<decl->getDeclKindName()<<endl;

	this->VisitStmt(decl->getBody());
	
	return true;
}

bool MPITypeCheckingConsumer::VisitDecl(Decl *decl){
	if(!this->visitStart)
		return true;


	if(isa<FunctionDecl>(decl)){
		FunctionDecl *func=cast<FunctionDecl>(decl);
		this->VisitStmt(func->getBody());
	}
	return true;
}


/*
bool MPITypeCheckingConsumer::TraverseCallExpr(CallExpr *funcCall){
	cout <<"The function being called is "<<stmt2str(&ci->getSourceManager(),ci->getLangOpts(),funcCall) <<endl;

	Decl *decl=funcCall->getCalleeDecl();
	cout <<"It is a "<<decl->getDeclKindName()<<endl;

	this->TraverseDecl(decl);

	/*
	if(isa<FunctionDecl>(decl)){
		FunctionDecl *fd=llvm::cast<FunctionDecl>(decl);
		cout <<"The name of the function is "<<fd->getNameAsString()<<endl;
		Stmt *body=fd->getBody();
		this->VisitStmt(body);
	}
	

	return true;
}
*/
