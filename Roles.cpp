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
	this->blocked=false;
}

Role::Role(string paramRoleName0, Range ran){
	this->paramRoleName=paramRoleName0;
	this->range=ran;
	this->finished=false;
	this->blocked=false;
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

//perform one visit to the comm tree. It stops when encounter a doable op.
VisitResult* Role::visit(){
	//TODO

	if (blocked)
		return nullptr;

	if (finished)
		return nullptr;

	

	while (true)
	{
		vector<Role> escapedRoles;

		if (this->curVisitNode==nullptr)
		{
			this->finished=true;

			cout<<"The role "<<this->getRoleName()<<" has visited all the nodes in the comm tree!"<<endl;

			return nullptr; 
		}

		string nodeName=this->curVisitNode->getNodeName();

		if (this->curVisitNode->isMarked())
		{
			//if the cur node has been marked, then skip
			this->curVisitNode=curVisitNode->skipToNextNode();

			cout<<"The current "<<nodeName<<" node has been marked;"<<endl;

			if (curVisitNode){
				cout<<this->getRoleName()<<" skip to the next "<<curVisitNode->getNodeName()<<" node."<<endl;
			}

			continue;
		}


		Condition curNodeCond=curVisitNode->getCond();
		Condition myRoleCond=Condition(this->getRange());

		Condition escapedCond=myRoleCond.Diff(curNodeCond);
		Condition stayHereCond=myRoleCond.Diff(escapedCond);

		//get all the roles who are able to escape
		for (int i = 0; i < escapedCond.getRangeList().size(); i++)
		{
			Role runaway=Role(escapedCond.getRangeList()[i]);
			runaway.setCurVisitNode(this->curVisitNode->skipToNextNode());
			escapedRoles.push_back(runaway);
		}

		if (escapedCond.isSameAsCond(myRoleCond))
		{
			//the complete role escaped from the cur node
			this->curVisitNode=curVisitNode->skipToNextNode();

			cout<<"The current "<<nodeName<<" node has nothing to do with the role "<<this->getRoleName()<<endl;

			if (curVisitNode){
				cout<<this->getRoleName()<<" skip to the next "<<curVisitNode->getNodeName()<<" node."<<endl;
			}

			continue;
		}


		////////some can escape and some may need to stay////////
		/********************************************************/
		MPIOperation *theOP=this->curVisitNode->getOP();
		//The cur node is an MPI op node
		if (theOP)
		{
			cout<<this->getRoleName()<<" visits the MPI "<<curVisitNode->getNodeName()<<" node"<<endl;

			MPIOperation *doableOP=new MPIOperation(*theOP);
			doableOP->setExecutorCond(stayHereCond);

			bool isBlocking=doableOP->isBlockingOP();

			if (isBlocking)
			{
				this->blocked=true;
			}

			return new VisitResult(isBlocking,doableOP,escapedRoles);
		}

		else{
			//enumerate the node types
			cout<<this->getRoleName()<<" visits the "<<curVisitNode->getNodeName()<<" node"<<endl;

			if (curVisitNode->isLeaf())
			{
				curVisitNode->setMarked();

				this->curVisitNode=curVisitNode->skipToNextNode();

				cout<<"The current "<<nodeName<<" node has nothing to do with the role "<<this->getRoleName()<<endl;

				if (curVisitNode){
					cout<<this->getRoleName()<<" skip to the next "<<curVisitNode->getNodeName()<<" node."<<endl;
				}

				continue;
			}

			else
			{
				//there are two branches, some processes in the role will go deeper
				//and the others will go to the next sibling.
				//BOTH runaway and goDeeper roles escape successfully!

				/******************************************************************/
				//if everyone needs to go deeper
				if (stayHereCond.isSameAsCond(myRoleCond))
				{
					this->curVisitNode=this->curVisitNode->goDeeper();

					if (curVisitNode)
					{
						cout<<this->getRoleName()<<" goes deeper to "<<curVisitNode->getNodeName()<<endl;
					}

					continue;
				}

				//get all the roles who are able to go deeper
				for (int i = 0; i < stayHereCond.getRangeList().size(); i++)
				{
					Role goDeeperRole=Role(stayHereCond.getRangeList()[i]);
					goDeeperRole.setCurVisitNode(this->curVisitNode->goDeeper());
					escapedRoles.push_back(goDeeperRole);
				}

				return new VisitResult(false,nullptr,escapedRoles);
			}
		}

	}

	return nullptr;
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


void ParamRole::insertActualRole(Role *r){
	if (!r)
	{
		return;
	}

	int size=this->actualRoles.size();
	for (int i = 0; i <size ; i++)
	{
		if (actualRoles[i]->hasRangeEqualTo(r->getRange()))
		{
			actualRoles[i]=r;
			return;
		}
	}

	this->actualRoles.push_back(r);
}
/********************************************************************/
//Class ParamRole impl end										****
/********************************************************************/