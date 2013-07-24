#include "Comm.h"

using namespace llvm;
using namespace clang;
//some functions
int min(int a, int b){if(a<b) return a; else return b;}
int max(int a, int b){if(a<b) return b; else return a;}

int minEnd(int a, int b){
	if(a==InitEndIndex)
		return b;

	if(b==InitEndIndex)
		return a;

	return min(a,b);
}


int maxEnd(int a, int b){
	if(a==InitEndIndex)
		return a;

	if(b==InitEndIndex)
		return b;

	return max(a,b);

}

bool isCmpOp(string op){
	
	array<string,6> cmpOPArr={"==","<",">","<=",">=","!="};
	int size=cmpOPArr.size();
	
	for (int i = 0; i < size; i++)
	{
		if(op==cmpOPArr[i])
			return true;
	}

	return false;
}

int compute(string op, int operand1, int operand2){
	if(op=="+")
		return operand1+operand2;

	if(op=="-")
		return operand1-operand2;

	return -1;
}

bool areTheseTwoNumsAdjacent(int a, int b){
	if(a==b) return true;

	if(a==b-1 || a==b+1) return true;

	return false;
}

string convertIntToStr(int number)
{
	stringstream ss;//create a stringstream
	ss << number;//add number to the stream
	return ss.str();//return a string with the contents of the stream
}






/********************************************************************/
//Class CommManager impl start										****
/********************************************************************/

CommManager::CommManager(CompilerInstance *ci0, int numOfProc0):root(ST_NODE_ROOT){
		root.setCond(Condition(true));
	
		curNode=&root;
		
		this->ci=ci0;
		this->numOfProc=numOfProc0;

		this->paramRoleNameMapping[WORLD]=ParamRole();
}

void CommManager::insertVarName(string name){
	this->varNames.push_back(name);
}

bool CommManager::isAVar(string name){
	for (int i = 0; i < this->varNames.size(); i++)
	{
		if(varNames[i]==name)
			return true;
	}

	return false;
}

Condition CommManager::extractCondFromBoolExpr(Expr *expr){
	string exprStr=stmt2str(&ci->getSourceManager(),ci->getLangOpts(),expr);
	cout<<"The expr "<<exprStr<<" is obtained by Comm.cpp"<<endl;
	
	cout<<"The stmt class type is "<<expr->getStmtClassName()<<endl;


	//**************************If the condition is a single item***********************************************
	//if it is a single number
	bool boolResult;
	bool canBeEval=expr->EvaluateAsBooleanCondition(boolResult,this->ci->getASTContext());
	
	if(canBeEval){
		cout <<"The expr can be evaluated!"<<endl;

		cout<<"The boolResult is "<<boolResult<<endl;

			if(boolResult){
				
				return Condition(true);

			}
			
			else{return Condition(false);}			
		
	}

	else{cout <<"The expr can NOT be evaluated!"<<endl;}

	//if it is a single var
	if(this->isAVar(exprStr)){
		//a var can contain any value, so assume true.
		return Condition(true);
	}


	if(isa<ParenExpr>(expr)){
		ParenExpr *bracketExpr=cast<ParenExpr>(expr);
		Expr *withoutParen=bracketExpr->getSubExpr();
		cout<<"The expr without brackets is "<<stmt2str(&ci->getSourceManager(),ci->getLangOpts(),withoutParen)<<endl;

		return extractCondFromBoolExpr(withoutParen);
	}
	/////////////////////////////////////////////////////////////
	//the expr is a bin op.
	////////////////////////////////////////////////////////////
	if(isa<BinaryOperator>(expr)){
		BinaryOperator *binOP=cast<BinaryOperator>(expr);

		Expr *lhs=binOP->getLHS();
		Expr *rhs=binOP->getRHS();

		string lhsStr=stmt2str(&ci->getSourceManager(),ci->getLangOpts(),lhs);
		string rhsStr=stmt2str(&ci->getSourceManager(),ci->getLangOpts(),rhs);
		string op=binOP->getOpcodeStr();

		cout<<"The bin op is "<<stmt2str(&ci->getSourceManager(),ci->getLangOpts(),binOP)<<"\n";
		cout<<"The lhs is "<<lhsStr<<"\n";
		cout<<"The rhs is "<<rhs<<"\n";
		cout<<"The operator is : "<<op<<endl;

		
		
///////////////////////////////////////////////////////////////////////////////////////////////////
		

		if(op=="&&"){
			Condition lCond=extractCondFromBoolExpr(lhs);
			Condition rCond=extractCondFromBoolExpr(rhs);

			string nonRankVarName="";
			string lhsNonRankVarName=lCond.getNonRankVarName();
			string rhsNonRankVarName=rCond.getNonRankVarName();

			if(lhsNonRankVarName==rhsNonRankVarName && lhsNonRankVarName!=""){
				nonRankVarName=lhsNonRankVarName;

				Condition tmpCond1=this->tmpNonRankVarCondMap[lhsNonRankVarName].top();
				this->tmpNonRankVarCondMap[lhsNonRankVarName].pop();
				Condition tmpCond2=this->tmpNonRankVarCondMap[lhsNonRankVarName].top();
				this->tmpNonRankVarCondMap[lhsNonRankVarName].pop();

				Condition newTmpCond=tmpCond1.AND(tmpCond2);
				newTmpCond.setNonRankVarName(lhsNonRankVarName);
				this->tmpNonRankVarCondMap[lhsNonRankVarName].push(newTmpCond);
			}

			if(!lCond.hasSameGroupComparedTo(rCond))
			{
				string errInfo="ERROR: mixture of conditions in Group "+
					lCond.getGroupName()+" AND Group "+rCond.getGroupName();
				throw new MPI_TypeChecking_Error(errInfo);
			}

			Condition andCond=lCond.AND(rCond);

			andCond.setNonRankVarName(nonRankVarName);
			cout <<"\n\n\n\n\n"<< andCond.printConditionInfo()<<"\n\n\n\n\n" <<endl;
			return andCond;
		}

		else if(op=="||"){
			Condition lCond=extractCondFromBoolExpr(lhs);
			Condition rCond=extractCondFromBoolExpr(rhs);

			string nonRankVarName="";
			string lhsNonRankVarName=lCond.getNonRankVarName();
			string rhsNonRankVarName=rCond.getNonRankVarName();

			if(lhsNonRankVarName==rhsNonRankVarName && lhsNonRankVarName!=""){
				nonRankVarName=lhsNonRankVarName;

				Condition tmpCond1=this->tmpNonRankVarCondMap[lhsNonRankVarName].top();
				this->tmpNonRankVarCondMap[lhsNonRankVarName].pop();
				Condition tmpCond2=this->tmpNonRankVarCondMap[lhsNonRankVarName].top();
				this->tmpNonRankVarCondMap[lhsNonRankVarName].pop();

				Condition newTmpCond=tmpCond1.OR(tmpCond2);
				newTmpCond.setNonRankVarName(lhsNonRankVarName);
				this->tmpNonRankVarCondMap[lhsNonRankVarName].push(newTmpCond);
			}

			if(!lCond.hasSameGroupComparedTo(rCond))
			{
				string errInfo="ERROR: mixture of conditions in Group "+
					lCond.getGroupName()+" AND Group "+rCond.getGroupName();
				throw new MPI_TypeChecking_Error(errInfo);
			}

			Condition orCond=lCond.OR(rCond);

			orCond.setNonRankVarName(nonRankVarName);
			cout <<"\n\n\n\n\n"<< orCond.printConditionInfo()<<"\n\n\n\n\n" <<endl;
			return orCond;
		}
		////it is a basic op
		else {

		//if the cur op is not a comparison op, then return true;
		if(!isCmpOp(op))
			return Condition(true);

		//if either lhs or rhs are bin op, then return true;
		if(isa<BinaryOperator>(lhs) || isa<BinaryOperator>(rhs))
			return Condition(true);

			
			bool leftIsVar=false;
			bool rightIsVar=false;
			string lVarName=lhsStr;
			string rVarName=rhsStr;

			
//check if the lhs or rhs are var decl.
////////////////////////////////////////////////////////////////////////////////
			if (DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(lhs)) {
				// It's a reference to a declaration...
				cout<<"//It's a reference to a declaration..."<<endl;

				if (VarDecl *VD = dyn_cast<VarDecl>(DRE->getDecl())) {
				// It's a reference to a variable (a local, function parameter, global, or static data member).
				cout<<"// It's a reference to a variable (a local, function parameter, global, or static data member)."<<endl;

				lVarName=VD->getQualifiedNameAsString();
				std::cout << "LHS is var: " << lVarName << std::endl;
				leftIsVar=true;
				}
			}

			//check if the lhs is a var ref
			else{
				if(this->isAVar(lVarName)){leftIsVar=true;}
			}


			if (DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(rhs)) {
				
				// It's a reference to a declaration...
				cout<<"//It's a reference to a declaration..."<<endl;
				if (VarDecl *VD = dyn_cast<VarDecl>(DRE->getDecl())) {
				// It's a reference to a variable (a local, function parameter, global, or static data member).
				cout<<"// It's a reference to a variable (a local, function parameter, global, or static data member)."<<endl;
				
				rVarName=VD->getQualifiedNameAsString();
				std::cout << "RHS is var: " << rVarName << std::endl;
				rightIsVar=true;
				}
			}

			else{
				if(this->isAVar(rVarName)){rightIsVar=true;}
			}

////////////////////////////////////////////////////////////////////////////////



			if(leftIsVar && rightIsVar){
				cout<<"Both lhs and rhs are var, so, all Range Condition!"<<endl;

				return Condition(true);
			}

			else if(!this->isVarRelatedToRank(lVarName)
				&&  !this->isVarRelatedToRank(rVarName)){
					APSInt Result;
					string nonRankVarName="";

					if(leftIsVar){
						nonRankVarName=lVarName;

						if (rhs->EvaluateAsInt(Result, this->ci->getASTContext())) {
						int num=atoi(Result.toString(10).c_str());
						std::cout << "The non-rank cond is created by (" <<op<<","<< num <<")"<< std::endl;

						Condition cond=Condition::createCondByOp(op,num);

						this->insertTmpNonRankVarCond(lVarName,cond);
						}
					}

					if(rightIsVar){
						nonRankVarName=rVarName;

						if (lhs->EvaluateAsInt(Result, this->ci->getASTContext())) {
						int num=atoi(Result.toString(10).c_str());
						std::cout << "The non-rank cond is created by (" <<op<<","<< num <<")"<< std::endl;

						Condition cond=Condition::createCondByOp(op,num);
						this->insertTmpNonRankVarCond(rVarName,cond);
						}
					}

				//if one of the vars is non-rank var, then it might be true
				//so return the "all" range.
				cout<<"Either the lvar or rvar is a non-rank var. So All Range Condition!"<<endl;
				//also set the name of the non-rank var
				Condition nonRankCond=Condition(true);
				nonRankCond.setNonRankVarName(nonRankVarName);
				return nonRankCond;
			}

			else{
				APSInt Result;

				if(leftIsVar){
					string commGroupName=this->getCommGroup4RankVar(lVarName);

					if (rhs->EvaluateAsInt(Result, this->ci->getASTContext())) {
						int num=atoi(Result.toString(10).c_str());
						std::cout << "The cond is created by (" <<op<<","<< num <<")"<< std::endl;

						Condition cond=Condition::createCondByOp(op,num);

						if(commGroupName!=WORLD)
							cond.setCommGroup(commGroupName);

						return cond;
					}
				}

				if(rightIsVar){
					string commGroupName=this->getCommGroup4RankVar(rVarName);

					if (lhs->EvaluateAsInt(Result, this->ci->getASTContext())) {
						int num=atoi(Result.toString(10).c_str());
						std::cout << "The cond is created by (" <<op<<","<< num <<")"<< std::endl;

						Condition cond=Condition::createCondByOp(op,num);

						if(commGroupName!=WORLD)
							cond.setCommGroup(commGroupName);

						return cond;
					}
				}
			}			
			
		}
	}

	return Condition(false);
}


void CommManager::insertNode(CommNode *node){

//set the condition for the node.
node->setCond(this->getTopCondition());

int nodeT=node->getNodeType();


if(node->getNodeType()==ST_NODE_CONTINUE){
	CommNode *theParent=this->curNode;
	while (theParent->getNodeType()!=ST_NODE_RECUR)
	{
		theParent=theParent->getParent();
	}

	if(theParent!=nullptr){
		ContNode *contNode=(ContNode*)node;
		RecurNode *loopNode=(RecurNode*)(theParent);

		contNode->setRefNode(loopNode);
	}

}

//insert the node
this->curNode->insert(node);

if(nodeT==ST_NODE_CHOICE || nodeT==ST_NODE_RECUR
   ||nodeT==ST_NODE_ROOT || nodeT==ST_NODE_PARALLEL){

	   this->curNode=node;
}

}


void CommManager::insertCondition(Expr *expr){
			this->clearTmpNonRankVarCondStackMap();	

			Condition cond=this->extractCondFromBoolExpr(expr);

			if(!stackOfRankConditions.empty()){
				Condition top=stackOfRankConditions.back();
				cond=top.AND(cond);
			}

			stackOfRankConditions.push_back(cond);

			cout<<"\n\n\n\n\nThe inner condition is \n"
				<<stackOfRankConditions.back().printConditionInfo()
				<<"\n\n\n\n\n"<<endl;


			string commGroupName=cond.getGroupName();
			if(this->paramRoleNameMapping.count(commGroupName)>0)
				paramRoleNameMapping[commGroupName].addAllTheRangesInTheCondition(cond);

			else
			{
				paramRoleNameMapping[commGroupName]=ParamRole(cond);
			}

}
	



void CommManager::insertRankVarAndOffset(string varName, int offset){
	if(this->isVarRelatedToRank(varName)){
		this->rankVarOffsetMapping.find(varName)->second=offset;
	}

	else{
		this->rankVarOffsetMapping[varName]=offset;
	}
	
}

void CommManager::cancelRelation(string varName){
	this->rankVarOffsetMapping.erase(this->rankVarOffsetMapping.find(varName));
}


bool CommManager::isVarRelatedToRank(string varName){
	if(this->rankVarOffsetMapping.count(varName)>0)
		return true;

	else
		return false;

}

Condition CommManager::getTopCondition(){
		if(this->stackOfRankConditions.size()==0){
			return Condition(true);
		}

		else{
			return this->stackOfRankConditions.back();
		}
}

void CommManager::insertExistingCondition(Condition cond){
		this->stackOfRankConditions.push_back(cond);

		string commGroupName=cond.getGroupName();
			if(this->paramRoleNameMapping.count(commGroupName)>0)
				paramRoleNameMapping[commGroupName].addAllTheRangesInTheCondition(cond);

			else
			{
				paramRoleNameMapping[commGroupName]=ParamRole(cond);
			}

}

Condition CommManager::popCondition(){
		Condition tmp=stackOfRankConditions.back();	
		this->stackOfRankConditions.pop_back();
		return tmp;
}

void CommManager::gotoParent(){
this->curNode= this->curNode->getParent();
}


void CommManager::insertRankVarAndCommGroupMapping(string rankVar, string commGroup){
	this->rankVarCommGroupMapping[rankVar]=commGroup;
}

string CommManager::getCommGroup4RankVar(string rankVar){
	return this->rankVarCommGroupMapping[rankVar];
}

void CommManager::insertNonRankVarAndCondtion(string nonRankVar, Condition cond){
	if(this->nonRankVarAndStackOfCondMapping.count(nonRankVar)>0)
	{
		if (this->nonRankVarAndStackOfCondMapping[nonRankVar].size()>0)
		{
			Condition top=this->getTopCond4NonRankVar(nonRankVar);
			cond=cond.AND(top);
		}
		
		cond.setNonRankVarName(nonRankVar);
		this->nonRankVarAndStackOfCondMapping[nonRankVar].push(cond);
	}

	else{
		stack<Condition> newStack;
		cond.setNonRankVarName(nonRankVar);
		newStack.push(cond);
		this->nonRankVarAndStackOfCondMapping[nonRankVar]=newStack;
	}

}

void CommManager::insertTmpNonRankVarCond(string nonRankVar, Condition cond){
	if(this->tmpNonRankVarCondMap.count(nonRankVar)>0){
		this->tmpNonRankVarCondMap[nonRankVar].push(cond);
	}

	else{
		stack<Condition> newStack;
		newStack.push(cond);
		this->tmpNonRankVarCondMap[nonRankVar]=newStack;
	}

}


void CommManager::clearTmpNonRankVarCondStackMap(){
	this->tmpNonRankVarCondMap.clear();
}


map<string,stack<Condition>> CommManager::getTmpNonRankVarCondStackMap()
{
	return tmpNonRankVarCondMap;
}


Condition CommManager::getTopCond4NonRankVar(string nonRankVar){
	if(this->nonRankVarAndStackOfCondMapping.count(nonRankVar)>0){
		if(this->nonRankVarAndStackOfCondMapping[nonRankVar].size()>0)
		return this->nonRankVarAndStackOfCondMapping[nonRankVar].top();

		else
			return Condition(false);
	}

	else{return Condition(false);}
}

void CommManager::removeTopCond4NonRankVar(string nonRankVar){
	if(this->nonRankVarAndStackOfCondMapping.count(nonRankVar)>0){
		if(this->nonRankVarAndStackOfCondMapping[nonRankVar].size()>0){
		this->nonRankVarAndStackOfCondMapping[nonRankVar].pop();
		
		if(this->nonRankVarAndStackOfCondMapping[nonRankVar].size()==0)
			this->nonRankVarAndStackOfCondMapping.erase(nonRankVar);
		}

		else
			throw new MPI_TypeChecking_Error("try to pop from an empty stack of non-rank var condtion");
//			this->nonRankVarAndStackOfCondMapping.erase(nonRankVar);
	}

	else{
		throw new MPI_TypeChecking_Error("try to pop from a non-existing non-rank var condtion stack");
	}

}


Condition CommManager::extractCondFromTargetExpr(Expr *expr){
	string errInfo="The current system does not support the operators other than + or - when constructing the target of MPI operation";
	string errInfo2="the current system does not allow to use unknown non-rank vars to denote target processes";
	string errInfo3="To represent the rank of a process using plus op, current system only support number plus number or rank-related var plus number";
	string errInfo4="To represent the rank of a process using minus op, current system only support rank-related var minus number";
	
	
	string exprStr=stmt2str(&ci->getSourceManager(),ci->getLangOpts(),expr);
	cout<<"The target expr "<<exprStr<<" is obtained by the extract method"<<endl;
	
	cout<<"The stmt class type is "<<expr->getStmtClassName()<<endl;


	//**************************If the condition is a single item***********************************************
	//if it is a single number
	APSInt IntResult;
	bool canBeEval=expr->EvaluateAsInt(IntResult,ci->getASTContext());
	
	if(canBeEval){
		cout <<"The expr can be evaluated as int!"<<endl;

		int rankNum=atoi(IntResult.toString(10).c_str());

		cout<<"The rank here is "<<rankNum<<endl;
		
		return Condition(Range(rankNum,rankNum));
	}

	else{cout <<"The expr can NOT be evaluated as int!"<<endl;}

	//if it is a single var
	if(this->isAVar(exprStr)){
		if(this->isVarRelatedToRank(exprStr))
			return this->getTopCondition().addANumber(this->getOffSet4RankRelatedVar(exprStr));

		else if(this->nonRankVarAndStackOfCondMapping.count(exprStr)>0){
			if(this->nonRankVarAndStackOfCondMapping[exprStr].size()>0){
				//we have identified the range of the non-rank var 
				return this->getTopCond4NonRankVar(exprStr);
			}

		}

		//the current system does not allow to use unknown non-rank vars to denote target processes.
		throw new MPI_TypeChecking_Error(errInfo2);
	}


	if(isa<ParenExpr>(expr)){
		ParenExpr *bracketExpr=cast<ParenExpr>(expr);
		Expr *withoutParen=bracketExpr->getSubExpr();
		cout<<"The expr without brackets is "<<stmt2str(&ci->getSourceManager(),ci->getLangOpts(),withoutParen)<<endl;

		return extractCondFromTargetExpr(withoutParen);
	}


	if(isa<BinaryOperator>(expr)){
		BinaryOperator *binOP=cast<BinaryOperator>(expr);

		Expr *lhs=binOP->getLHS();
		Expr *rhs=binOP->getRHS();

		string lhsStr=stmt2str(&ci->getSourceManager(),ci->getLangOpts(),lhs);
		string rhsStr=stmt2str(&ci->getSourceManager(),ci->getLangOpts(),rhs);
		string op=binOP->getOpcodeStr();

		cout<<"The bin op is "<<stmt2str(&ci->getSourceManager(),ci->getLangOpts(),binOP)<<"\n";
		cout<<"The lhs is "<<lhsStr<<"\n";
		cout<<"The rhs is "<<rhs<<"\n";
		cout<<"The operator is : "<<op<<endl;

		/*********************************************************************************************/
		if(op=="+" || op=="-"){
			APSInt INT_Result;
			bool lhsIsNum=false;
			bool rhsIsNum=false;
			int lhsNum=-1;
			int rhsNum=-1;

			if (lhs->EvaluateAsInt(INT_Result, this->ci->getASTContext())) {
				lhsIsNum=true;
				lhsNum=atoi(INT_Result.toString(10).c_str());
			}

			if (rhs->EvaluateAsInt(INT_Result, this->ci->getASTContext())) {
				rhsIsNum=true;
				rhsNum=atoi(INT_Result.toString(10).c_str());
			}

			/////////////////////////////////////////////////////
			if (lhsIsNum && rhsIsNum)
			{
				int rankNum=compute(op,lhsNum,rhsNum);
				return Condition(Range(rankNum,rankNum));
			}

			
			if(op=="+"){
				if(this->hasAssociatedWithCondition(lhsStr) && rhsIsNum){
					return this->extractCondFromTargetExpr(lhs).addANumber(rhsNum);
				}

	

				if(this->hasAssociatedWithCondition(rhsStr) && lhsIsNum){
					return this->extractCondFromTargetExpr(rhs).addANumber(lhsNum);
				}


				throw new MPI_TypeChecking_Error(errInfo3);
			}

			// minus
			else{
				if(this->hasAssociatedWithCondition(lhsStr) && rhsIsNum){
					return this->extractCondFromTargetExpr(lhs).addANumber(-rhsNum);
				}

				throw new MPI_TypeChecking_Error(errInfo4);
			}
		
		}

		
		
		throw new MPI_TypeChecking_Error(errInfo);

	}

	throw new MPI_TypeChecking_Error("Cannot find a proper condition for the target.");
}

bool CommManager::hasAssociatedWithCondition(string varName){
	if(this->isVarRelatedToRank(varName))
		return true;

	if(this->nonRankVarAndStackOfCondMapping.count(varName)>0){
		if(this->nonRankVarAndStackOfCondMapping[varName].size()>0)
			return true;
	}

	return false;
}


int CommManager::getOffSet4RankRelatedVar(string varName){
	if(this->rankVarOffsetMapping.count(varName)>0){
		return rankVarOffsetMapping[varName];
	}

	else{
		throw new MPI_TypeChecking_Error("The var "+varName+" is not related to rank but the method getOffSet4RankRelatedVar() is called");
	}
}
/********************************************************************/
//Class CommManager impl end										****
/********************************************************************/




/********************************************************************/
//Class Role impl start										****
/********************************************************************/
	Role::Role(Range ran){
		this->paramRoleName=WORLD;
		range=ran;
	}

	Role::Role(string paramRoleName0, Range ran){
		this->paramRoleName=paramRoleName0;
		this->range=ran;
	}

string Role::getRoleName(){
	string name=this->paramRoleName+"[";
	name.append(convertIntToStr(this->range.getStart()));
	name.append("..");
	name.append(convertIntToStr(this->range.getEnd()));
	name.append("]");

	return name;
}

/********************************************************************/
//Class Role impl end										****
/********************************************************************/


/********************************************************************/
//Class ParamRole impl start										****
/********************************************************************/

	ParamRole::ParamRole(Condition cond){
		this->paramRoleName=cond.getGroupName();
		
		this->addAllTheRangesInTheCondition(cond);
	}

	bool ParamRole::hasARoleSatisfiesRange(Range ran){
		for (int i = 0; i < actualRoles.size(); i++)
		{
			Role* r=actualRoles[i];
			if(r->hasRangeEqualTo(ran))
				return true;
		}
	
		return false;
	}


	void ParamRole::addAllTheRangesInTheCondition(Condition cond){
		vector<Range> ranList=cond.getRangeList();
		for (int i = 0; i < ranList.size(); i++)
		{
			Range r=ranList[i];
			if(this->hasARoleSatisfiesRange(r))
				continue;

			else
				this->insertActualRole(new Role(r));
			
		}
	}

/********************************************************************/
//Class ParamRole impl end										****
/********************************************************************/