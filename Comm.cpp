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

Condition CommManager::extractCondFromExpr(Expr *expr){
	cout<<"The expr "<<stmt2str(&ci->getSourceManager(),ci->getLangOpts(),expr)<<" is obtained by Comm.cpp"<<endl;
	
	cout<<"The stmt class type is "<<expr->getStmtClassName()<<endl;

	APSInt INT_Result0;

	if (expr->EvaluateAsInt(INT_Result0, this->ci->getASTContext())){
		int rankNum=atoi(INT_Result0.toString(10).c_str());
		
		return Condition(Range(rankNum,rankNum));
	}

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

	if(isa<ParenExpr>(expr)){
		ParenExpr *bracketExpr=cast<ParenExpr>(expr);
		Expr *withoutParen=bracketExpr->getSubExpr();
		cout<<"The expr without brackets is "<<stmt2str(&ci->getSourceManager(),ci->getLangOpts(),withoutParen)<<endl;

		return extractCondFromExpr(withoutParen);
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
				if(this->isVarRelatedToRank(lhsStr) && rhsIsNum){
					return this->getTopCondition().addANumber(rhsNum);
				}

				if(this->isVarRelatedToRank(rhsStr) && lhsIsNum){
					return this->getTopCondition().addANumber(lhsNum);
				}

				return Condition(false);
			}

			// minus
			else{
				if(this->isVarRelatedToRank(lhsStr) && rhsIsNum){
					return this->getTopCondition().addANumber(-rhsNum);
				}

				return Condition(false);
			}
		
		}

		if(op=="&&"){
			Condition lCond=extractCondFromExpr(lhs);
			Condition rCond=extractCondFromExpr(rhs);
			if(!lCond.hasSameGroupComparedTo(rCond))
			{
				string errInfo="ERROR: mixture of conditions in Group "+
					lCond.getGroupName()+" AND Group "+rCond.getGroupName();
				throw new MPI_TypeChecking_Error(errInfo);
			}

			Condition andCond=lCond.AND(rCond);

			cout <<"\n\n\n\n\n"<< andCond.printConditionInfo()<<"\n\n\n\n\n" <<endl;
			return andCond;
		}

		else if(op=="||"){
			Condition lCond=extractCondFromExpr(lhs);
			Condition rCond=extractCondFromExpr(rhs);
			if(!lCond.hasSameGroupComparedTo(rCond))
			{
				string errInfo="ERROR: mixture of conditions in Group "+
					lCond.getGroupName()+" AND Group "+rCond.getGroupName();
				throw new MPI_TypeChecking_Error(errInfo);
			}

			Condition orCond=lCond.OR(rCond);

			cout <<"\n\n\n\n\n"<< orCond.printConditionInfo()<<"\n\n\n\n\n" <<endl;
			return orCond;
		}
		////it is a basic op
		else {
			
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
				//if one of the vars is non-rank var, then it might be true
				//so return the "all" range.
				cout<<"Either the lvar or rvar is a non-rank var. So All Range Condition!"<<endl;
				return Condition(true);
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

this->curNode->insert(node);

int nodeT=node->getNodeType();

if(nodeT==ST_NODE_CHOICE || nodeT==ST_NODE_RECUR
   ||nodeT==ST_NODE_ROOT || nodeT==ST_NODE_PARALLEL){

	   this->curNode=node;
}


}


void CommManager::insertCondition(Expr *expr){
		
			Condition cond=this->extractCondFromExpr(expr);

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