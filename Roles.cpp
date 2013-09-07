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
	return this->paramRoleName+this->getRange().printRangeInfo();
}

void Role::setCurVisitNode(CommNode *node){
	this->curVisitNode=node;
}

//perform one visit to the comm tree. It stops when encounter a doable op.
VisitResult* Role::visit(){
	//TODO
	if(this->curVisitNode && this->curVisitNode->isNegligible()){
		this->blocked=false;
		this->curVisitNode=this->curVisitNode->skipToNextNode();
	}

	if (blocked)
		return nullptr;

	if (finished)
		return nullptr;


	while (true)
	{

		vector<Role*> escapedRoles;

		if (this->curVisitNode==nullptr)
		{
			this->finished=true;

			cout<<"The role "<<this->getRoleName()<<" has visited all the nodes in the comm tree!"<<endl;

			return nullptr; 
		}

		string nodeName=this->curVisitNode->getNodeName();

		if (this->curVisitNode->isNegligible())
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
		
		escapedCond=escapedCond.AND(Condition(true));

		Condition stayHereCond=myRoleCond.Diff(escapedCond);

		//get all the roles who are able to escape
		for (int i = 0; i < escapedCond.getRangeList().size(); i++)
		{
			Role *runaway=new Role(escapedCond.getRangeList()[i]);
			runaway->setCurVisitNode(this->curVisitNode->skipToNextNode());
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
		vector<MPIOperation*> *theOPs=this->curVisitNode->getOPs();
		//The cur node is an MPI op node
		if (theOPs)
		{
			cout<<this->getRoleName()<<" visits the MPI "<<curVisitNode->getNodeName()<<" node"<<endl;
			bool isBlocking=false;
			MPIOperation *doableOP=nullptr;

			for (int i = 0; i < theOPs->size(); i++)
			{
				if(theOPs->at(i)==nullptr){
					theOPs->erase(theOPs->begin()+i);
					i--;
					continue;
				}

				if (theOPs->at(i)->isBlockingOP())
					isBlocking=true;


				if (theOPs->at(i)->isInPendingList)
					continue;

				//the doable op is the unfinished task
				doableOP=theOPs->at(i);

				//the task is picked (maybe partially) by the role with cond "doableCond"
				if(doableOP->isCollectiveOp()){
					Condition doableCond=stayHereCond.AND(doableOP->getTargetCond());
					Condition remainingTaskCond=doableOP->getTargetCond().Diff(doableCond);
					doableOP->isInPendingList=true;

					if(!remainingTaskCond.isIgnored()){
						MPIOperation* remainingOP=new MPIOperation(*doableOP);
						remainingOP->setTargetCond(remainingTaskCond);
						remainingOP->isInPendingList=false;
						theOPs->push_back(remainingOP);
					}

					doableOP->setTargetCond(doableCond);
				}

				else{
					Condition doableCond=stayHereCond.AND(doableOP->getExecutor());
					Condition remainingTaskCond=doableOP->getExecutor().Diff(doableCond);
					doableOP->isInPendingList=true;

					if(!remainingTaskCond.isIgnored()){
						MPIOperation* remainingOP=new MPIOperation(*doableOP);
						remainingOP->setExecutorCond(remainingTaskCond);
						remainingOP->isInPendingList=false;
						theOPs->push_back(remainingOP);
					}

					
					doableOP->setExecutorCond(doableCond);
				}

				break;
			}


			if (isBlocking)
			{
				this->blocked=true;
			}

			else{
				this->blocked=false;
				//if it is non-blocking op, then the whole role can goto next node
				this->curVisitNode= this->curVisitNode->skipToNextNode();
				escapedRoles.clear();
			}

			return new VisitResult(doableOP,escapedRoles);
		}

		else{
			//enumerate the node types
			cout<<this->getRoleName()<<" visits the "<<curVisitNode->getNodeName()<<" node"<<endl;
			if(curVisitNode->getNodeType()==MPI_Wait){
				WaitNode *wn=(WaitNode*)curVisitNode;
				if(wn->type==WaitNode::waitNormal){
					this->blocked=true;

					return new VisitResult(nullptr,escapedRoles);
				}
			}

			else if(curVisitNode->getNodeType()==MPI_Barrier){
				BarrierNode* bar=(BarrierNode*)curVisitNode;
				this->blocked=true;
				bar->visit(Condition(this->getRange()));
				escapedRoles.clear();

				return new VisitResult(nullptr,escapedRoles);
			}

			else if (curVisitNode->isLeaf())
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
					Role *goDeeperRole=new Role(stayHereCond.getRangeList()[i]);
					goDeeperRole->setCurVisitNode(this->curVisitNode->goDeeper());
					escapedRoles.push_back(goDeeperRole);
				}

				//the role is split into two parts, it will be blocked unless all the parts combine
				this->blocked=true;
				return new VisitResult(nullptr,escapedRoles);
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

	this->actualRoles=new vector<Role*>();
}


ParamRole::ParamRole(Condition cond){

	this->paramRoleName=cond.getGroupName();

	this->addAllTheRangesInTheCondition(cond);

	this->actualRoles=new vector<Role*>();
}

bool ParamRole::hasARoleSatisfiesRange(Range ran){
	for (int i = 0; i < actualRoles->size(); i++)
	{
		Role* r=actualRoles->at(i);
		if(r->hasRangeEqualTo(ran))
			return true;
	}

	return false;
}


string ParamRole::getFullParamRoleName(){
	return this->paramRoleName+"[0..N-1]";
}

//used in constructing the comm tree
void ParamRole::addAllTheRangesInTheCondition(Condition cond){
	cond=cond.AND(Condition(true));
	vector<Range> ranList=cond.getRangeList();
	for (int i = 0; i < ranList.size(); i++)
	{
		Range r=ranList[i];
		if(this->hasARoleSatisfiesRange(r))
			continue;

		else
			this->insertActualRole(new Role(paramRoleName,r),false);

	}
}


void ParamRole::insertActualRole(Role *r, bool forceUpdate){
	if (!r)
	{
		return;
	}

	int size=this->actualRoles->size();
	for (int i = 0; i <size ; i++)
	{
		if (actualRoles->at(i)->hasRangeEqualTo(r->getRange()))
		{
			if (!forceUpdate)
			{
				return;
			}

			if (actualRoles->at(i)->hasFinished())
			{
				delete r;
				return;
			}


			CommNode *curVisitNodeOfRoleI=actualRoles->at(i)->getCurVisitNode();
			if (curVisitNodeOfRoleI==nullptr)
			{
				delete r;
				return;
			}

			int curPosForI=curVisitNodeOfRoleI->getPosIndex();

			if (r->getCurVisitNode()==nullptr)
			{
				delete r;
				return;
			}

			int curPosForRoleR=r->getCurVisitNode()->getPosIndex();

			if(curPosForRoleR >= curPosForI || !r->IsBlocked()){
				delete  actualRoles->at(i);
				actualRoles->at(i)=r;
			}

			else{
				cout<<"The cur role i has pos "<<curPosForI<<", which is newer than role r's "<<
					curPosForRoleR<<"; so do nothing"<<endl;

				delete r;
			}

			return;
		}
	}

	this->actualRoles->push_back(r);
}
/********************************************************************/
//Class ParamRole impl end										****
/********************************************************************/