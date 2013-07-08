#include "MPITypeCheckingConsumer.h"

using namespace clang;
using namespace llvm;
using namespace std;





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



bool MPITypeCheckingConsumer::VisitAsmStmt(AsmStmt *S){
	if(!this->visitStart)
		return true;

	cout <<"The Asm stmt is \n"<<stmt2str(&ci->getSourceManager(),ci->getLangOpts(),S) <<endl;

	return true;
}

bool MPITypeCheckingConsumer::VisitIfStmt(IfStmt *ifStmt){
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
	this->VisitStmt(ifStmt->getThen());
	

	//visit else part
	this->VisitStmt(ifStmt->getElse());
	
	return true;
}




bool MPITypeCheckingConsumer::VisitDeclStmt(DeclStmt *S){
	if(!this->visitStart)
		return true;

	cout <<"The decl stmt is: "<<stmt2str(&ci->getSourceManager(),ci->getLangOpts(),S) <<endl;
	return true;
}


bool MPITypeCheckingConsumer::VisitSwitchStmt(SwitchStmt *S){
	if(!this->visitStart)
		return true;

	cout <<"Call Switch stmt" <<endl;
	return true;
}

bool MPITypeCheckingConsumer::VisitForStmt(ForStmt *S){
	if(!this->visitStart)
		return true;

	cout <<"The for stmt is \n"<<stmt2str(&ci->getSourceManager(),ci->getLangOpts(),S) <<endl;
	return true;
}

bool MPITypeCheckingConsumer::VisitDoStmt(DoStmt *S){
	if(!this->visitStart)
		return true;

	cout <<"Call Do stmt" <<endl;
	return true;
}

bool MPITypeCheckingConsumer::VisitWhileStmt(WhileStmt *S){
	if(!this->visitStart)
		return true;

	cout <<"The While stmt is \n"<<stmt2str(&ci->getSourceManager(),ci->getLangOpts(),S) <<endl;
	return true;
}

bool MPITypeCheckingConsumer::VisitLabelStmt(LabelStmt *S){
	if(!this->visitStart)
		return true;

	cout <<"Call Label stmt" <<endl;
	return true;
}

bool MPITypeCheckingConsumer::VisitReturnStmt(ReturnStmt *S){
	if(!this->visitStart)
		return true;

	cout <<"The return stmt is \n"<<stmt2str(&ci->getSourceManager(),ci->getLangOpts(),S) <<endl;

	if(!this->funcsList.empty()){
	string popped=this->funcsList.back();
	cout<<"The decl with name "<<popped<<" is going to be removed."<<endl;
	this->funcsList.pop_back();
	}

	return true;
}

bool MPITypeCheckingConsumer::VisitNullStmt(NullStmt *S){
	if(!this->visitStart)
		return true;

	cout <<"Call Null stmt" <<endl;
	return true;
}




bool MPITypeCheckingConsumer::VisitBreakStmt(BreakStmt *S){
	if(!this->visitStart)
		return true;

	cout <<"Call Break stmt" <<endl;
	return true;
}

bool MPITypeCheckingConsumer::VisitContinueStmt(ContinueStmt *S){
	if(!this->visitStart)
		return true;

	cout <<"Call Continue stmt" <<endl;
	return true;
}


bool MPITypeCheckingConsumer::VisitCallExpr(CallExpr *op){
	//if we haven't started to visit the main function, then do nothing.
	if(!this->visitStart)
		return true;

//	cout <<"The function going to be called is "<<stmt2str(&ci->getSourceManager(),ci->getLangOpts(),op) <<endl;

	Decl *decl=op->getCalleeDecl();
//	cout <<"It is a "<<decl->getDeclKindName()<<endl;

	FunctionDecl *funcCall=op->getDirectCallee();
	//perform a check. If the decl has been visited before, then throw err info
	//else traverse the decl.
	if (funcCall)
	{
		this->analyzeDecl(funcCall,op);
	}
	
	else{
		this->TraverseDecl(decl);
	}

	return true;
}

bool MPITypeCheckingConsumer::VisitDecl(Decl *decl){
	if(!this->visitStart)
		return true;


	if(isa<FunctionDecl>(decl)){
		FunctionDecl *func=cast<FunctionDecl>(decl);
		this->VisitFunctionDecl(func);
	}

	return true;
}

bool MPITypeCheckingConsumer::VisitFunctionDecl(FunctionDecl *funcDecl){
		if(!this->visitStart)
		return true;	
	
		/***************************************************************
		*The parameter list of the function call.
		*****************************************************************/
			for (FunctionDecl::param_iterator it = funcDecl->param_begin(); it !=funcDecl->param_end(); it++)
			{
				unsigned int index=(*it)->getFunctionScopeIndex();
				unsigned int depth=(*it)->getFunctionScopeDepth();

				cout << "The index and depth of the parameter "<<decl2str(&ci->getSourceManager(),ci->getLangOpts(),*it);
				cout <<" is " << index << " and " << depth << endl;

				Expr *argVal=(*it)->getDefaultArg();
				if(argVal && argVal->isRValue()){
					APSInt result;
					if(argVal->EvaluateAsInt(result,ci->getASTContext())){
						cout<<"The parameter in the "<<index<<" position has default value "<<result.toString(10)<<endl;						
					}
				
				}
			}
			

			if(funcDecl->hasBody()){
		
	//		cout<<funcDecl->getNameAsString()<<" has a body!"<<endl;
			this->TraverseStmt(funcDecl->getBody());

		}

		else
		{
			cout<<"The function "<<funcDecl->getNameAsString()<<" do not have a body here!"<<endl;
		}	
		
}




