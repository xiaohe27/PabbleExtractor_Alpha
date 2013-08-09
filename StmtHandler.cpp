#include "MPITypeCheckingConsumer.h"

using namespace clang;
using namespace llvm;






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
	CommNode *choiceNode=new CommNode(ST_NODE_CHOICE,Condition(true));


	Expr *condExpr=ifStmt->getCond();


	this->commManager->clearTmpNonRankVarCondStackMap();	

	Condition thenCond=this->commManager->extractCondFromBoolExpr(condExpr);

	cout<<"\n\n\n\n\nThe if condition is \n"
		<<thenCond.printConditionInfo()
		<<"\n\n\n\n\n"<<endl;

	//insert the non-rank var conditions to formal stack, if any
	vector<string> stackOfNonRankVarNames=this->analyzeNonRankVarCond(this->commManager->getTmpNonRankVarCondStackMap());

	if (stackOfNonRankVarNames.size()==0)
	{
		choiceNode->setAsRelatedToRank();
	}
	//the choice node will become the cur node automatically
	this->mpiSimulator->insertNode(choiceNode);

	////////////////////////////////////////////////////////////////////////////////////////////////////////////


	//visit then part
	cout<<"Going to visit then part of condition: "<<stmt2str(&ci->getSourceManager(),ci->getLangOpts(),condExpr)<<endl;


	//before traverse the then part, create a root node for it 
	//only the processes that satisfy the then part conditon can enter the block!
	CommNode *thenNode=new CommNode(ST_NODE_ROOT,thenCond);
	this->mpiSimulator->insertNode(thenNode);

	////////////////////////////////////////////////////////////////////////////////////
	Condition theIFCond=this->mpiSimulator->getCurNode()->getCond();



	bool shouldInsertToMPITree=false;
	string ifCondStr=stmt2str(&ci->getSourceManager(),ci->getLangOpts(),condExpr);

	if (theIFCond.isComplete() && !this->commManager->containsRankStr(ifCondStr)
		&& !thenCond.isTrivial()){
			shouldInsertToMPITree=true;

			CommNode* theIfNode=this->mpiSimulator->getCurNode();
			CommNode* theChoiceCommNode=theIfNode->getParent();

			theIfNode->setMaster();

			MPINode* theChoiceMPINode=new MPINode(theChoiceCommNode);
			MPINode* theIfMPINode=new MPINode(theIfNode);
			this->mpiTree->insertNode(theChoiceMPINode);
			this->mpiTree->insertNode(theIfMPINode);

			this->mpiSimulator->insertPosAndMPINodeTuple(theChoiceCommNode->getPosIndex(),theChoiceMPINode);
			this->mpiSimulator->insertPosAndMPINodeTuple(theIfNode->getPosIndex(),theIfMPINode);
	}
	////////////////////////////////////////////////////////////////////////////////////

	this->TraverseStmt(ifStmt->getThen());

	//after the stmt in the then block have been traversed, return the control to the choice node
	this->mpiSimulator->gotoParent();

	//should remove the condition for the then part now.
	cout<<"//should remove the condition for the then part now."<<endl;

	this->removeAndAddNewNonRankVarCondInStack(stackOfNonRankVarNames);


	Condition condInElsePart;
	if(strict){ //if only the same type of expr appears in the conditional expr
		if (!thenCond.isRelatedToRank())
			condInElsePart=Condition(thenCond);
		else
			condInElsePart=Condition::negateCondition(thenCond);

	}

	else{
		if (thenCond.isMixtureCond())
			condInElsePart=Condition(false);

		else
		{
			if (!thenCond.isRelatedToRank())
				condInElsePart=Condition(thenCond);
			else
				condInElsePart=Condition::negateCondition(thenCond);
		}
	}


	cout << "\n\n\n\n\nThe condition in else part is \n"<<condInElsePart.printConditionInfo()
		<<"\n\n\n\n\n"<<endl;

	//before visit else part, create a node for it
	CommNode *elseNode=new CommNode(ST_NODE_ROOT,condInElsePart);
	this->mpiSimulator->insertNode(elseNode);

	/////////////////////////////////////////////////////////////////////
	if (shouldInsertToMPITree)
	{
		CommNode* theElseNode=this->mpiSimulator->getCurNode();
		theElseNode->setMaster();

		MPINode *theElseMPINode=new MPINode(theElseNode);
		this->mpiTree->insertNode(theElseMPINode);

		this->mpiSimulator->insertPosAndMPINodeTuple(theElseNode->getPosIndex(),theElseMPINode);
	}
	///////////////////////////////////////////////////////////////////////////////////////////////

	//visit else part
	cout<<"Going to visit else part of condition: "<<stmt2str(&ci->getSourceManager(),ci->getLangOpts(),condExpr)<<endl;
	this->TraverseStmt(ifStmt->getElse());

	this->mpiSimulator->gotoParent();

	this->removeNonRankVarCondInStack(stackOfNonRankVarNames);

	//the choice node does not add any extra condition, so just go to its parent node
	this->mpiSimulator->gotoParent();
	return true;
}




bool MPITypeCheckingConsumer::VisitDeclStmt(DeclStmt *S){


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
	if(!theCondOfFor.isComplete() || this->commManager->containsRankStr(condOfForStr))
		throw new MPI_TypeChecking_Error("The condition in for loop should not contain rank-related var!");

	vector<string> nonRankVarList=this->analyzeNonRankVarCond(this->commManager->getTmpNonRankVarCondStackMap());

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////


	int size=this->analyzeForStmt(initFor,condOfFor,inc,bodyOfFor,nonRankVarList);

	cout<<"The for loop will iterate "<<size<<" times!"<<endl;


	//////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//create node for 'for' stmt
	RecurNode *forNode=new RecurNode(size);
	this->mpiSimulator->insertNode(forNode);

	//if the iter num is -1, then the iter num is unknown

	//if the iter num is unknown, then either throw exception or create a recur node.
	this->handleLoop();

	//visit each stmt inside the for loop
	this->TraverseStmt(bodyOfFor);

	this->mpiSimulator->insertAllTheContNodesWithLabel(forNode->getSrcCodeInfo());

	this->mpiSimulator->gotoParent();

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
	if(!theCondOfWhile.isComplete() || this->commManager->containsRankStr(condOfWhileStr))
		throw new MPI_TypeChecking_Error("The condition in while loop should not contain rank-related var!");

	vector<string> nonRankVarList=this->analyzeNonRankVarCond(this->commManager->getTmpNonRankVarCondStackMap());


	//create node for 'while' stmt
	RecurNode *whileNode=new RecurNode(-1);
	this->mpiSimulator->insertNode(whileNode);

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////
	this->handleLoop();
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////

	//visit each stmt inside the for loop
	this->TraverseStmt(bodyOfWhile);

	this->mpiSimulator->insertAllTheContNodesWithLabel(whileNode->getSrcCodeInfo());

	this->mpiSimulator->gotoParent();

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

	CommNode *cont=new CommNode(ST_NODE_CONTINUE,Condition(true));

	this->mpiSimulator->insertNode(cont);

	CommNode* curNd=this->mpiSimulator->getCurNode();
	RecurNode* tmpRecurNode=curNd->getInnerMostRecurNode();


	if (!tmpRecurNode->hasKnownNumberOfIterations())
	{
		cont->setInfo(tmpRecurNode->getSrcCodeInfo());
		MPINode* contMPINode=new MPINode(cont);
		this->mpiSimulator->insertToTmpContNodeList(contMPINode);
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
			this->commManager->removeTopCond4NonRankVar(theVar);
			this->commManager->insertNonRankVarAndCondtion(theVar,Condition(Range(initValOfTheVar,theTargetRange.getEnd())));

			return theTargetRange.getEnd()-initValOfTheVar+1;
		}

		//dec
		found1 = incStr.find("--");
		found2 = incStr.find("-=");
		if (found1!=string::npos || found2!=string::npos)
		{
			this->commManager->removeTopCond4NonRankVar(theVar);
			this->commManager->insertNonRankVarAndCondtion(theVar,Condition(Range(theTargetRange.getStart(),initValOfTheVar)));

			return initValOfTheVar-theTargetRange.getStart()+1;
		}

	}

	return -1; 
}