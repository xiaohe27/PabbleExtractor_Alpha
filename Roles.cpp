#include "Comm.h"

using namespace llvm;
using namespace clang;
using namespace std;

/********************************************************************/
//Class Role impl start										****
/********************************************************************/
Role::Role(Range ran){
	this->paramRoleName=WORLD;
	range=ran;
	this->finished=false;
}

Role::Role(string paramRoleName0, Range ran){
	this->paramRoleName=paramRoleName0;
	this->range=ran;
	this->finished=false;
}

string Role::getRoleName(){
	string name=this->paramRoleName+"[";
	name.append(convertIntToStr(this->range.getStart()));
	name.append("..");
	name.append(convertIntToStr(this->range.getEnd()));
	name.append("]");

	return name;
}

void Role::setCurVisitNode(CommNode *node){
	this->curVisitNode=node;
}

//perform one visit to the comm tree. It stops when encounter a blocking op.
VisitResult Role::visit(){
	//TODO

	while (true)
	{
		vector<Role> escapedRoles;
		
		Condition curNodeCond=curVisitNode->getCond();
		Condition myRoleCond=Condition(this->getRange());

		Condition escapedCond=myRoleCond.Diff(curNodeCond);
		Condition stayHereCond=myRoleCond.Diff(escapedCond);

		//get all the roles who are able to escape
		for (int i = 0; i < escapedCond.getRangeList().size(); i++)
		{
			escapedRoles.push_back(Role(escapedCond.getRangeList()[i]));
		}

		if (escapedCond.isSameAsCond(myRoleCond))
		{
			//the complete role escaped from the cur node
			this->curVisitNode=
		}


	}


	vector<Role> roles;
	return VisitResult(true,nullptr,roles);
}
/********************************************************************/
//Class Role impl end										****
/********************************************************************/


/********************************************************************/
//Class ParamRole impl start										****
/********************************************************************/
ParamRole::ParamRole(){
	this->paramRoleName=WORLD;
}


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
			this->insertActualRole(new Role(paramRoleName,r));

	}
}

/********************************************************************/
//Class ParamRole impl end										****
/********************************************************************/