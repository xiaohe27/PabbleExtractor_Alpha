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

bool MPITypeCheckingConsumer::VisitBinaryOperator(BinaryOperator *S){
	cout <<"Visit BinaryOperator!"<<endl;
	return true;
}

bool MPITypeCheckingConsumer::VisitBinAndAssign(CompoundAssignOperator *op){
	cout <<"Call Compound Ass Op" <<endl;
	return true;
}

bool MPITypeCheckingConsumer::VisitBinComma(BinaryOperator *binOP){
	cout <<"Call Compound Ass Op" <<endl;
	return true;
}



bool MPITypeCheckingConsumer::TraverseAsmStmt(AsmStmt *S){
	cout <<"Call Asm stmt" <<endl;

	return true;
}

bool MPITypeCheckingConsumer::TraverseIfStmt(IfStmt *ifStmt){
	cout <<"Call If stmt" <<endl;

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


bool MPITypeCheckingConsumer::TraverseDeclStmt(DeclStmt *S){
	cout <<"Call Decl stmt" <<endl;
	return true;
}




bool MPITypeCheckingConsumer::TraverseSwitchStmt(SwitchStmt *S){
	cout <<"Call Switch stmt" <<endl;
	return true;
}

bool MPITypeCheckingConsumer::TraverseForStmt(ForStmt *S){
	cout <<"Call For stmt" <<endl;
	return true;
}

bool MPITypeCheckingConsumer::TraverseDoStmt(DoStmt *S){
	cout <<"Call Do stmt" <<endl;
	return true;
}

bool MPITypeCheckingConsumer::TraverseWhileStmt(WhileStmt *S){
	cout <<"Call While stmt" <<endl;
	return true;
}

bool MPITypeCheckingConsumer::TraverseLabelStmt(LabelStmt *S){
	cout <<"Call Label stmt" <<endl;
	return true;
}

bool MPITypeCheckingConsumer::TraverseReturnStmt(ReturnStmt *S){
	cout <<"Call Return stmt" <<endl;
	return true;
}

bool MPITypeCheckingConsumer::TraverseNullStmt(NullStmt *S){
	cout <<"Call Null stmt" <<endl;
	return true;
}


bool MPITypeCheckingConsumer::TraverseBinaryOperator(BinaryOperator *op){
	cout <<"Call Bin Op stmt" <<endl;
	cout << op->getOpcodeStr().data()<<endl;

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
	if (rhs->EvaluateAsInt(Result, this->ci->getASTContext())) {
		std::cout << "RHS has value " << Result.toString(10) << std::endl;
	}
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


bool MPITypeCheckingConsumer::TraverseCallExpr(CallExpr *op){
	cout <<"Call Call Expr stmt" <<endl;

	return true;
}

