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

//analyze and then insert the tuples to the formal nonRankVarCondStack
vector<string> MPITypeCheckingConsumer::analyzeNonRankVarCond(map<string,stack<Condition>> tmpNonRankVarCondMap){
	vector<string> stackOfNonRankVarNames;

	for (map<string,stack<Condition>>::iterator it=tmpNonRankVarCondMap.begin(); it!=tmpNonRankVarCondMap.end(); ++it){
		stack<Condition> stackOfCond=it->second;
		Condition combinedCond=Condition(true);
		int size=stackOfCond.size();
		for (int i = 0; i < size; i++)
		{
			combinedCond=combinedCond.AND(stackOfCond.top());
			stackOfCond.pop();
		}

		//insert to the formal stack
		this->commManager->insertNonRankVarAndCondtion(it->first,combinedCond);
		stackOfNonRankVarNames.push_back(it->first);
	}

	tmpNonRankVarCondMap.clear();
	return stackOfNonRankVarNames;
}

void MPITypeCheckingConsumer::removeNonRankVarCondInStack(vector<string> stackOfNonRankVarNames){
	for (int i = 0; i < stackOfNonRankVarNames.size(); i++)
	{
		this->commManager->removeTopCond4NonRankVar(stackOfNonRankVarNames[i]);
	}

}

void MPITypeCheckingConsumer::removeAndAddNewNonRankVarCondInStack(vector<string> stackOfNonRankVarNames){
	for (int i = 0; i < stackOfNonRankVarNames.size(); i++)
	{
		Condition theCond=this->commManager->getTopCond4NonRankVar(stackOfNonRankVarNames[i]);
		this->commManager->removeTopCond4NonRankVar(stackOfNonRankVarNames[i]);
		theCond=Condition::negateCondition(theCond);
		this->commManager->insertNonRankVarAndCondtion(stackOfNonRankVarNames[i],theCond);
	}

}

bool MPITypeCheckingConsumer::TraverseIfStmt(IfStmt *ifStmt){
	if(!this->visitStart)
		return true;

	//cout <<"The if stmt is \n"<<stmt2str(&ci->getSourceManager(),ci->getLangOpts(), ifStmt) <<endl;
	CommNode *choiceNode=new CommNode(ST_NODE_CHOICE);

	//the choice node will become the cur node automatically
	this->mpiSimulator->insertNode(choiceNode);



	Expr *condExpr=ifStmt->getCond();
	string typeOfCond=condExpr->getType().getAsString();

	cout<<"Type of condition is "<<typeOfCond<<"\nCond Expr is: "<<stmt2str(&ci->getSourceManager(),ci->getLangOpts(),condExpr)<<endl;

	//after exec this stmt, the condition for the then part will be put on the top of the stack
	this->commManager->insertCondition(condExpr);

	//insert the non-rank var conditions to formal stack, if any
	vector<string> stackOfNonRankVarNames=this->analyzeNonRankVarCond(this->commManager->getTmpNonRankVarCondStackMap());

	//visit then part
	cout<<"Going to visit then part of condition: "<<stmt2str(&ci->getSourceManager(),ci->getLangOpts(),condExpr)<<endl;


	//before traverse the then part, create a root node for it 
	//only the processes that satisfy the then part conditon can enter the block!
	CommNode *thenNode=new CommNode(ST_NODE_ROOT);
	this->mpiSimulator->insertNode(thenNode);

	this->TraverseStmt(ifStmt->getThen());

	//after the stmt in the then block have been traversed, return the control to the choice node
	this->mpiSimulator->gotoParent();

	//should remove the condition for the then part now.
	cout<<"//should remove the condition for the then part now."<<endl;
	Condition condInIfPart=this->commManager->popCondition();
	this->removeAndAddNewNonRankVarCondInStack(stackOfNonRankVarNames);



	//	Condition curCond=Condition::negateCondition(condInIfPart);
	Condition topCond=this->commManager->getTopCondition();
	Condition condInElsePart=topCond.Diff(condInIfPart);
	this->commManager->insertExistingCondition(condInElsePart);

	cout << "\n\n\n\n\nThe condition in else part is \n"<<condInElsePart.printConditionInfo()
		<<"\n\n\n\n\n"<<endl;

	//before visit else part, create a node for it
	CommNode *elseNode=new CommNode(ST_NODE_ROOT);
	this->mpiSimulator->insertNode(elseNode);


	//visit else part
	cout<<"Going to visit else part of condition: "<<stmt2str(&ci->getSourceManager(),ci->getLangOpts(),condExpr)<<endl;
	this->TraverseStmt(ifStmt->getElse());

	this->mpiSimulator->gotoParent();

	cout<<"//should remove the condition for the else part now."<<endl;
	this->commManager->popCondition();

	this->removeNonRankVarCondInStack(stackOfNonRankVarNames);

	//the choice node does not add any extra condition, so just go to its parent node
	this->mpiSimulator->gotoParent();
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

bool MPITypeCheckingConsumer::TraverseForStmt(ForStmt *S){
	if(!this->visitStart)
		return true;

	cout <<"The for stmt is \n"<<stmt2str(&ci->getSourceManager(),ci->getLangOpts(),S) <<endl;

	Expr *condOfFor=S->getCond();
	string condOfForStr=stmt2str(&ci->getSourceManager(),ci->getLangOpts(), condOfFor);

	Stmt *initFor = S->getInit();
	cout<<"type of init stmt in for stmt is "<<initFor->getStmtClassName()<<endl;
	string initForStmtStr=stmt2str(&ci->getSourceManager(),ci->getLangOpts(), initFor);

	Expr *inc=S->getInc();
	string incOfForStr=stmt2str(&ci->getSourceManager(),ci->getLangOpts(), inc);

	Stmt *bodyOfFor= S->getBody();
	string bodyOfForStmtStr=stmt2str(&ci->getSourceManager(),ci->getLangOpts(), bodyOfFor);

	
	//	cout<<"The var decl is "<<varDeclStr<<endl;
	cout<<"The init is "<<initForStmtStr<<endl;
	cout<<"The condition is "<<condOfForStr<<endl;
	cout<<"The inc is "<<incOfForStr<<"; It is of type "<<inc->getStmtClassName()<<endl;
	cout<<"The body of for stmt is "<<bodyOfForStmtStr<<endl;



	//analyse the non-rank var 
	Condition theCondOfFor=this->commManager->extractCondFromBoolExpr(condOfFor);
	if(!theCondOfFor.isComplete())
		throw new MPI_TypeChecking_Error("The condition in for loop should not contain rank-related var!");

	vector<string> nonRankVarList=this->analyzeNonRankVarCond(this->commManager->getTmpNonRankVarCondStackMap());

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////


	int size=this->analyzeForStmt(initFor,condOfFor,inc,bodyOfFor,nonRankVarList);

	cout<<"The for loop will iterate "<<size<<" times!"<<endl;

	//create node for 'for' stmt
	RecurNode *forNode=new RecurNode(size);
	this->mpiSimulator->insertNode(forNode);

	//visit each stmt inside the for loop
	this->TraverseStmt(bodyOfFor);

	this->removeNonRankVarCondInStack(nonRankVarList);

	return true;
}

bool MPITypeCheckingConsumer::VisitDoStmt(DoStmt *S){
	if(!this->visitStart)
		return true;

	cout <<"Call Do stmt" <<endl;
	return true;
}

bool MPITypeCheckingConsumer::TraverseWhileStmt(WhileStmt *S){
	if(!this->visitStart)
		return true;

	cout <<"The While stmt is \n"<<stmt2str(&ci->getSourceManager(),ci->getLangOpts(),S) <<endl;

	Expr *condOfWhile=S->getCond();
	string condOfWhileStr=stmt2str(&ci->getSourceManager(),ci->getLangOpts(), condOfWhile);

	Stmt *bodyOfWhile= S->getBody();
	string bodyOfWhileStmtStr=stmt2str(&ci->getSourceManager(),ci->getLangOpts(), bodyOfWhile);

	//analyse the non-rank var 
	Condition theCondOfWhile=this->commManager->extractCondFromBoolExpr(condOfWhile);
	if(!theCondOfWhile.isComplete())
		throw new MPI_TypeChecking_Error("The condition in while loop should not contain rank-related var!");

	vector<string> nonRankVarList=this->analyzeNonRankVarCond(this->commManager->getTmpNonRankVarCondStackMap());

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////

	//create node for 'while' stmt
	RecurNode *whileNode=new RecurNode(-1);
	this->mpiSimulator->insertNode(whileNode);

	//visit each stmt inside the for loop
	this->TraverseStmt(bodyOfWhile);

	this->removeNonRankVarCondInStack(nonRankVarList);

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
	
	CommNode *cont=new CommNode(ST_NODE_CONTINUE);
	this->mpiSimulator->insertNode(cont);
	return true;
}


bool MPITypeCheckingConsumer::VisitCallExpr(CallExpr *E){
	//if we haven't started to visit the main function, then do nothing.
	if(!this->visitStart)
		return true;

	cout <<"The function going to be called is "<<stmt2str(&ci->getSourceManager(),ci->getLangOpts(),E) <<endl;


	int numOfArgs=E->getNumArgs();


	vector<string> args(numOfArgs+1);


	//get all the args of the function call
	for(int i=0; i<numOfArgs; i++)
	{		
		args[i]=expr2str(&ci->getSourceManager(),ci->getLangOpts(),E->getArg(i));
	}


	//	Decl *decl=E->getCalleeDecl();


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
			string commGroup=args[0];

			this->rankVar=args[1].substr(1,args[1].length()-1);

			this->commManager->insertRankVarAndOffset(rankVar,0);

			this->commManager->insertRankVarAndCommGroupMapping(this->rankVar,commGroup);

			cout<<"Rank var is "<<this->rankVar <<".\t AND it is associated with group "<<commGroup<<endl;
		}

		if(funcName=="MPI_Send"){

			string dataType=args[2];
			string dest=args[3];
			string tag=args[4];
			string group=args[5];

			MPIOperation *mpiOP=new MPIOperation(ST_NODE_SEND, 
												dataType,
												this->commManager->getTopCondition(), //the performer
												this->commManager->extractCondFromTargetExpr(E->getArg(3)), //the dest
												tag, 
												group);

			CommNode *sendNode=new CommNode(mpiOP);
			sendNode->setNodeType(ST_NODE_SEND);
			this->mpiSimulator->insertNode(sendNode);


			cout <<"\n\n\n\n\nThe dest of mpi send is"
				<< mpiOP->getDestCond().printConditionInfo()<<"\n\n\n\n\n" <<endl;

		}

		if(funcName=="MPI_Recv"){

			string dataType=args[2];
			string src=args[3];
			string tag=args[4];
			string group=args[5];

			MPIOperation *mpiOP=new MPIOperation(ST_NODE_RECV, dataType,
								this->commManager->extractCondFromTargetExpr(E->getArg(3)),
								this->commManager->getTopCondition(), 
								tag, group);

			CommNode *recvNode=new CommNode(mpiOP);
			recvNode->setNodeType(ST_NODE_RECV);
			this->mpiSimulator->insertNode(recvNode);


			cout <<"\n\n\n\n\nThe src of mpi recv is"<<
				mpiOP->getSrcCond().printConditionInfo()<<"\n\n\n\n\n" <<endl;

		}


		/*******************Enum the possible MPI OPs end***********************************/
		/////////////////////////////////////////////////////////////////////
		if(funcCall->hasBody())
			this->analyzeDecl(funcCall);
	}

	else{
		this->TraverseDecl(E->getCalleeDecl());
	}


	args.clear();
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





















/*************************************************************************
Analyze For Stmt!
*************************************************************************/
int MPITypeCheckingConsumer::analyzeForStmt(Stmt* initStmt, Expr* condExpr, Expr* incExpr, Stmt* body,vector<string> nonRankVarList)
{

	map<string,int> varValMap;


	if (isa<DeclStmt>(initStmt))
	{
		DeclStmt initDeclStmt=cast<DeclStmt>(*initStmt);
		DeclGroupRef dgr=initDeclStmt.getDeclGroup();


		for (DeclGroupRef::iterator it=dgr.begin();it!=dgr.end();++it)
		{
			Decl *decl=*it;
			if (isa<VarDecl>(*decl))
			{
				VarDecl varDecl=cast<VarDecl>(*decl);
				string varName=varDecl.getNameAsString();
				Expr *initExprInVarDecl=varDecl.getInit();

				APSInt result;
				if (initExprInVarDecl->EvaluateAsInt(result,ci->getASTContext()))
				{
					int num=atoi(result.toString(10).c_str());
					varValMap[varName]=num;
				}

				string initExpr=stmt2str(&ci->getSourceManager(),ci->getLangOpts(),initExprInVarDecl);
				cout<<"the var "<<varName<<" is declared!"
					<<"The init in the decl is "<<initExpr<<endl;
			}
		}

	}

	else if (isa<BinaryOperator>(initStmt))
	{
		BinaryOperator binOP=cast<BinaryOperator>(*initStmt);
		Expr *lhs=binOP.getLHS();
		Expr *rhs=binOP.getRHS();
		string op=binOP.getOpcodeStr();

		if(op=="="){
		string lhsStr=expr2str(&ci->getSourceManager(),ci->getLangOpts(),lhs);
			if (this->commManager->isAVar(lhsStr))
			{
				APSInt Result;
				if (rhs->EvaluateAsInt(Result, this->ci->getASTContext())) {
					int num=atoi(Result.toString(10).c_str());

					varValMap[lhsStr]=num;
					}
			}
		}
		
	}

	
	map<string,bool> isRelevant;
	map<string,bool> isIn;

	for (auto &x:varValMap)
	{
		string theVarName=x.first;
		Condition cond=this->commManager->getTopCond4NonRankVar(theVarName);

		if(cond.isIgnored()){
			isRelevant[theVarName]=false;
			isIn[theVarName]=false;
		}

		else{
			isRelevant[theVarName]=true;

			if(cond.isRankInside(x.second))
				isIn[theVarName]=true;
			else
				isIn[theVarName]=false;
		}

	}

	bool allCondVarHaveBeenInitialized=true;
	bool noneCondCanBeSatisfied=true;

	for (int i = 0; i < nonRankVarList.size(); i++)
	{
		string nonRankVar=nonRankVarList[i];
		if(varValMap.count(nonRankVar)==0)
		{
			allCondVarHaveBeenInitialized=false;
			break;
		}

		if(isIn[nonRankVar]){
			noneCondCanBeSatisfied=false;
			break;
		}
	}

	if(allCondVarHaveBeenInitialized && noneCondCanBeSatisfied){
		//no condition can be satisfied, loop will iterate zero time
		return 0;
	}

	if(nonRankVarList.size()>1)
		return -1; //cannot estimate the number of iterations if there are more than 1 var involved.


	if (nonRankVarList.size()==1)
	{
		if(!this->isChangingByOneUnit(incExpr)){
			return -1;
		}

		string theVar=nonRankVarList.back();
		int initValOfTheVar=varValMap[theVar];
		Condition cond4TheVar=this->commManager->getTopCond4NonRankVar(theVar);
		Range theTargetRange=cond4TheVar.getTheRangeContainingTheNum(initValOfTheVar);

		string incStr=expr2str(&ci->getSourceManager(),ci->getLangOpts(),incExpr);
		
		//inc
		size_t found1 = incStr.find("++");
		size_t found2 = incStr.find("+=");
		if (found1!=string::npos || found2!=string::npos)
		{
			//if the end pos is related to N, then no exact iteration numbers can be estimated
			if(theTargetRange.getEnd()==InitEndIndex){return -1;}

			else{return theTargetRange.getEnd()-initValOfTheVar+1;}
		}

		//dec
		found1 = incStr.find("--");
		found2 = incStr.find("-=");
		if (found1!=string::npos || found2!=string::npos)
		{
			return initValOfTheVar-theTargetRange.getStart()+1;
		}
		
	}

	return -1; 
}