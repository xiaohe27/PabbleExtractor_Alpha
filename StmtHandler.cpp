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
			std::cout << "Look! LHS is the var:" << VD->getQualifiedNameAsString() <<"!!!"<< std::endl;
		}
	}

	APSInt Result;
	Expr *rhs=op->getRHS();

//	cout << "RHS has textual content: \n"<<stmt2str(&ci->getSourceManager(),ci->getLangOpts(),rhs)<<"\n"<<endl;


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

//almost finished
bool MPITypeCheckingConsumer::TraverseIfStmt(IfStmt *ifStmt){
	if(!this->visitStart)
		return true;

	//cout <<"The if stmt is \n"<<stmt2str(&ci->getSourceManager(),ci->getLangOpts(), ifStmt) <<endl;
	CommNode *choiceNode=new CommNode(ST_NODE_CHOICE);
	
	//The choice node is an intermediate node, so we need to reset the cur node in CommManager
	this->commManager->curNode->insert(choiceNode);

	this->commManager->curNode=choiceNode;

	Expr *condExpr=ifStmt->getCond();
	string typeOfCond=condExpr->getType().getAsString();

	cout<<"Type of condition is "<<typeOfCond<<"\nCond Expr is: "<<stmt2str(&ci->getSourceManager(),ci->getLangOpts(),condExpr)<<endl;

	//after exec this stmt, the condition for the then part will be put on the top of the stack
	this->commManager->insertCondition(condExpr);
	
	
	//visit then part
	cout<<"Going to visit then part of condition: "<<stmt2str(&ci->getSourceManager(),ci->getLangOpts(),condExpr)<<endl;


	//before traverse the then part, create a root node for it and change the cur node ref
	//only the processes that satisfy the then part conditon can enter the block!
	CommNode *thenNode=new CommNode(this->commManager->getTopCondition());
	this->commManager->curNode->insert(thenNode);
	this->commManager->curNode= thenNode;

	this->TraverseStmt(ifStmt->getThen());

	//after the stmt in the then block have been traversed, return the control to the choice node
	this->commManager->gotoParent();

	//should remove the condition for the then part now.
	cout<<"//should remove the condition for the then part now."<<endl;
	Condition condInIfPart=this->commManager->popCondition();

	Condition curCond=Condition::negateCondition(condInIfPart);
	Condition topCond=this->commManager->getTopCondition();
	Condition condInElsePart=curCond.AND(topCond);
	this->commManager->insertExistingCondition(condInElsePart);

	cout << "\n\n\n\n\nThe condition in else part is \n"<<condInElsePart.printConditionInfo()
		<<"\n\n\n\n\n"<<endl;

	//before visit else part, create a node for it
	CommNode *elseNode=new CommNode(this->commManager->getTopCondition());
	this->commManager->curNode->insert(elseNode);
	this->commManager->curNode= elseNode;

	//visit else part
	cout<<"Going to visit else part of condition: "<<stmt2str(&ci->getSourceManager(),ci->getLangOpts(),condExpr)<<endl;
	this->TraverseStmt(ifStmt->getElse());

	this->commManager->gotoParent();

	cout<<"//should remove the condition for the else part now."<<endl;
	this->commManager->popCondition();


	this->commManager->gotoParent();
	return true;
}




bool MPITypeCheckingConsumer::VisitDeclStmt(DeclStmt *S){

	if(!this->visitStart){
		DeclGroupRef d=S->getDeclGroup();
		
		DeclGroupRef::iterator it;

	for( it = d.begin(); it != d.end(); it++)
	{

		if(isa<VarDecl>(*it)){
			VarDecl *var=cast<VarDecl>(*it);
			string varName=var->getDeclName().getAsString();
			cout<<"Find the var "<<varName<<endl;

			this->commManager->insertVarName(varName);
		}
	}
		return true;
	}

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

	cout <<"The return stmt is "<<stmt2str(&ci->getSourceManager(),ci->getLangOpts(),S) <<endl;

	removeFuncFromList();

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


bool MPITypeCheckingConsumer::VisitCallExpr(CallExpr *E){
	//if we haven't started to visit the main function, then do nothing.
	if(!this->visitStart)
		return true;

	//	cout <<"The function going to be called is "<<stmt2str(&ci->getSourceManager(),ci->getLangOpts(),op) <<endl;

	PrintingPolicy Policy(ci->getLangOpts());
	int numOfArgs=E->getNumArgs();

	vector<string> args(numOfArgs);

	//get all the args of the function call
	for(int i=0, j=numOfArgs; i<j; i++)
	{
		string TypeS;
		raw_string_ostream s(TypeS);
		E->getArg(i)->printPretty(s, 0, Policy);
//		errs() << "arg: " << s.str() << "\n";

		args[i]=s.str();
	}

	Decl *decl=E->getCalleeDecl();

	FunctionDecl *funcCall=E->getDirectCallee();
	//perform a check. If the decl has been visited before, then throw err info
	//else traverse the decl.
	if (funcCall)
	{
		string funcName=funcCall->getQualifiedNameAsString();

		/////////////////////////////////////////////////////////////////////////////////////
		/*******************Enum the possible MPI OPs**************************************/

		//get the var storing number of processes
		if(funcName=="MPI_Comm_size"){
			this->numOfProcessesVar=args[1].substr(1,args[1].length()-1);

			cout<<"Var storing num of Processes is "<<this->numOfProcessesVar<<endl;
		}

		if(funcName=="MPI_Comm_rank"){
			
			this->rankVar=args[1].substr(1,args[1].length()-1);

			this->commManager->insertRankVarAndOffset(rankVar,0);
		
			cout<<"Rank var is "<<this->rankVar<<endl;
		}

		if(funcName=="MPI_Send"){
			
			string dataType=args[2];
			string dest=args[3];
			string tag=args[4];
			string group=args[5];

			MPIOperation *mpiOP=new MPIOperation(ST_NODE_SEND, dataType,
								this->commManager->curNode->getCond(), 
								this->commManager->extractCondFromExpr(E->getArg(3)), tag, group);

			CommNode *sendNode=new CommNode(mpiOP);

			this->commManager->insertNode(sendNode);

		}


		/*******************Enum the possible MPI OPs end***********************************/
		/////////////////////////////////////////////////////////////////////
		if(funcCall->hasBody())
		this->analyzeDecl(funcCall);
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

//		cout << "The index and depth of the parameter "<<decl2str(&ci->getSourceManager(),ci->getLangOpts(),*it);
//		cout <<" is " << index << " and " << depth << endl;

		Expr *argVal=(*it)->getDefaultArg();
		if(argVal && argVal->isRValue()){
			APSInt result;
			if(argVal->EvaluateAsInt(result,ci->getASTContext())){
				cout<<"The parameter in the "<<index<<" position has default value "<<result.toString(10)<<endl;						
			}

		}
	}

	string funcName=funcDecl->getQualifiedNameAsString();

	if(funcDecl->hasBody()){

		//		cout<<funcDecl->getNameAsString()<<" has a body!"<<endl;
		this->TraverseStmt(funcDecl->getBody());

		string retType=funcDecl->getResultType().getAsString();
		
		if(retType.compare("void")==0){
			if( !this->funcsList.empty() && this->funcsList.back().compare(funcName)==0){
				//if the void function does not have a return stmt, then remove the function name from the list here.
				this->removeFuncFromList();				
			}

			else{}
		}

	}

	else
	{
		cout<<"The function "<<funcName<<" do not have a body here!"<<endl;


	}	

	return true;
}




