#include "Comm.h"

using namespace llvm;
using namespace clang;
using namespace std;


//////////////////////VisitResult//////////////////////////////////////////
VisitResult::VisitResult(MPIOperation *op, vector<Role*> roles){
	this->doableOP=op;
	this->escapableRoles=roles;
}

void VisitResult::printVisitInfo(){
	if (this->doableOP)
	{
		cout<<"\n\n\nThe doable MPI operation in this node is "<<endl;
		cout<<this->doableOP->printMPIOP()<<endl;

		cout<<"It is "<<(doableOP->isBlockingOP()?"":"non-")<<"blocking operation!\n\n\n"<<endl;
	}


	for (int i = 0; i < escapableRoles.size(); i++)
	{
		cout<<"The role "<<escapableRoles[i]->getRoleName()<<" is able to escape to the node with index "
			<<escapableRoles[i]->getCurVisitNode()->getPosIndex()<<endl;
	}

}
/////////////////////VisitResult Ends///////////////////////////////////////



/********************************************************************/
//Class MPISimulator impl start										****
/********************************************************************/


MPISimulator::MPISimulator(CommManager *commMgr, MPITree *tree){
	this->commManager=commMgr;
	this->mpiTree=tree;
	this->posIndexAndMPINodeMapping[0]=this->mpiTree->getRoot();
	root=new CommNode(ST_NODE_ROOT,Condition(true));
	root->setMaster(); 
	curNode=root;

}


void MPISimulator::insertToTmpContNodeList(MPINode *contNode){
	if (contNode && contNode->getNodeType()==ST_NODE_CONTINUE)
	{
		this->listOfContMPINodes.push_back(contNode);
	}
}

void MPISimulator::insertAllTheContNodesWithLabel(string loopLabel){
	for (int i = 0; i < this->listOfContMPINodes.size(); i++)
	{
		MPINode* curCont=this->listOfContMPINodes.at(i);
		if(curCont->getNodeType()!=ST_NODE_CONTINUE)
			throw new MPI_TypeChecking_Error("Non cont node found in cont node list...");

		if (curCont->getLabelInfo()!=loopLabel)
			continue;

		this->mpiTree->insertNode(curCont);

		this->listOfContMPINodes.erase(this->listOfContMPINodes.begin()+i);
		i--;
	}
}


void MPISimulator::insertPosAndMPINodeTuple(int pos, MPINode *mpiNode){
	if (this->posIndexAndMPINodeMapping.count(pos)>0)
	{
		throw new MPI_TypeChecking_Error("Error in constructing MPI tree, duplicate nodes!");
	}

	this->posIndexAndMPINodeMapping[pos]=mpiNode;
}


void MPISimulator::forbidMPIOP(CommNode *node){
	if (node->getOPs())
		return;

	if (node->isRankRelatedChoice() || this->curNode->isRankRelatedChoice())
		return;

	if(node->getNodeType()==ST_NODE_FOREACH)
		return;


	node->forbidTheMPIOP();
}

void MPISimulator::insertNode(CommNode *node){
	Condition curCond=this->curNode->getCond();
	Condition nodeCond=node->getCond();

	if(this->curNode->HasMpiOPBeenForbidden()){
		if(node->getOPs()){
			string errInfo="The current node is not allowed to insert MPI operation!";
			throw new MPI_TypeChecking_Error(errInfo);
		}

		node->forbidTheMPIOP();
	}

	if (!this->curNode->HasMpiOPBeenForbidden() && !curCond.isComplete())
	{
		//if the condition is satisfied, then the node needs
		//to forbid the mpi op to be inserted into the tree
		forbidMPIOP(node);
	}


	//set the condition for the node.
	Condition newCond=curCond.AND(nodeCond);
	newCond.setNonRankVarName(nodeCond.getNonRankVarName());
	node->setCond(newCond);

	string commGroupName=node->getCond().getGroupName();
	if(this->commManager->getParamRoleMapping().count(commGroupName)>0)
		this->commManager->getParamRoleMapping().at(commGroupName)->addAllTheRangesInTheCondition(node->getCond());

	else
		this->commManager->getParamRoleMapping()[commGroupName]=new ParamRole(node->getCond());



	int nodeT=node->getNodeType();


	if(node->getNodeType()==ST_NODE_CONTINUE){

		ContNode *contNode=(ContNode*)node;
		CommNode *loopNode=node->getInnerMostRecurNode();
		contNode->setRefNode(loopNode);

	}

	if (node->getOPs())
	{
		MPIOperation* mpiOP=node->getOPs()->back();
		if (mpiOP->isCollectiveOp())
		{
			mpiOP->setTargetCond(mpiOP->getTargetCond().AND(node->getCond()));
		}
	}


	//insert the node
	this->curNode->insert(node);

	if(nodeT==ST_NODE_CHOICE || nodeT==ST_NODE_RECUR 
		|| nodeT==ST_NODE_FOREACH || nodeT==ST_NODE_ROOT)
			this->curNode=node;

}


void MPISimulator::gotoParent(){
	this->curNode= this->curNode->getParent();
}

void MPISimulator::makeItTidy(){
	for (auto &x:this->commManager->getParamRoleMapping())
	{
		int size=x.second ->getTheRoles()->size();

		for (int i = 0; i < size; i++)
		{
			if (x.second->getTheRoles()->at(i)->IsBlocked())
			{
				CommNode* theNode=x.second->getTheRoles()->at(i)->getCurVisitNode();
				if (theNode->isNegligible())
				{
					x.second->getTheRoles()->at(i)->unblock();
				}
			}
		}
	}
}

bool MPISimulator::isDeadLockDetected(){
	this->makeItTidy();

	bool blocking=false;

	for (auto &x:this->commManager->getParamRoleMapping())
	{
		int size=x.second ->getTheRoles()->size();
		for (int i = 0; i < size; i++)
		{
			if (x.second->getTheRoles()->at(i)->hasFinished())
			{
				continue;
			}

			blocking=x.second->getTheRoles()->at(i)->IsBlocked();

			if (!blocking)
			{
				return false;
			}
		}
	}

	return blocking;
}

void MPISimulator::initTheRoles(){
	const map<string,ParamRole*> paramRoleMap=this->commManager->getParamRoleMapping();

	for (auto &x: paramRoleMap)
	{

		ParamRole *paramRole=x.second;

		vector<Role*> *roles= paramRole->getTheRoles();

		for (int i = 0; i < roles->size(); i++)
		{
			roles->at(i)->setCurVisitNode(this->root);
		}
	}

}

void MPISimulator::printTheRoles(){
	const map<string,ParamRole*> paramRoleMap=this->commManager->getParamRoleMapping();

	for (auto &x: paramRoleMap)
	{
		string paramRoleName=x.first;
		cout<<"The param role name is "<<paramRoleName<<endl;
		cout<<"The actual roles for this param role are:\n";

		ParamRole *paramRole=x.second;

		vector<Role*> *roles= paramRole->getTheRoles();

		for (int i = 0; i < roles->size(); i++)
		{
			cout<<"The role "<<i<<" is "<< roles->at(i)->getRoleName()<<endl;
		}
	}

}

bool MPISimulator::areAllRolesDone(){
	map<string,ParamRole*> paramRoleMap=this->commManager->getParamRoleMapping();

	for (auto &x: paramRoleMap)
	{
		ParamRole *paramRole=x.second;

		vector<Role*> *roles= paramRole->getTheRoles();

		for (int i = 0; i < roles->size(); i++)
		{
			if (!roles->at(i)->hasFinished())
				return false;
		}
	}

	return true;
}



////////////////////////////////////////////////////////////////////////////
void MPISimulator::simulate(){

	cout<<"Ready to simulate the execution of the MPI program now!"<<endl;
	const map<string,ParamRole*> paramRoleMap=this->commManager->getParamRoleMapping();
	cout<<"There are "<<paramRoleMap.size()<<" communicator groups involved"<<endl;

	this->printTheRoles();

	this->initTheRoles();

	this->root->initTheBranchId();

	if(paramRoleMap.size()==0){
		cout<<"No group was constructed..."<<endl;
		return;
	}


	while (!this->root->isMarked() && !isDeadLockDetected())
	{
		for (auto &x: commManager->getParamRoleMapping())
		{
			string paramRoleName=x.first;			

			ParamRole *paramRole=x.second;


			for (int i = 0; i < paramRole->getTheRoles()->size(); i++)
			{
				if (paramRole->getTheRoles()->at(i)->hasFinished())
				{
					delete paramRole->getTheRoles()->at(i);
					paramRole->getTheRoles()->erase(paramRole->getTheRoles()->begin()+i);

					i--;
					continue;
				}

				cout<<"It is "<< paramRole->getTheRoles()->at(i)->getRoleName()<<" visiting the tree now."<<endl;
				VisitResult *vr=paramRole->getTheRoles()->at(i)->visit();

				if (vr){
					vr->printVisitInfo();

					this->analyzeVisitResult(vr);
				}

			}
		}

		//////////////////////////////////////////////////////////////////////////////////
		if (areAllRolesDone())
		{
			//if all the roles finished their work, then simulation completes successfully
			this->root->setMarked();
		}
	}

	if (root->isNegligible())
	{
		cout<<"\n\n\nThe comm tree has been traversed successfully!!!"<<endl;
	}

	else if(isDeadLockDetected())
		cout<<"\n\n\nDeadlock occurs!!!";
}


Condition MPISimulator::getTarCondFromExprAndExecCond(Expr *expr, Condition execCond){

	Condition newTarget=this->commManager->extractCondFromTargetExpr(expr, execCond);

	return newTarget;
}

/**
Analyse the visit result. push the prospective MPI op into the proper stack,
manage the newly created roles.
*/
void MPISimulator::analyzeVisitResult(VisitResult *vr){
	//TODO
	int size=vr->escapableRoles.size();
	if (size>0)
	{
		string paramName=vr->escapableRoles.at(0)->getParamRoleName();
		ParamRole *paramRole=this->commManager->getParamRoleWithName(paramName);

		for (int i = 0; i <size ; i++)
		{
			paramRole->insertActualRole(vr->escapableRoles.at(i),false);
		}
	}

	MPIOperation *op=vr->doableOP;
	if (op)
	{
		//if the target condition has not been built, then build it
		if(op->isDependentOnExecutor()){

			op->setTargetCond(this->getTarCondFromExprAndExecCond(op->getTargetExpr(),op->getExecutor()));

		}

		this->insertOpToPendingList(op);
	}

}


void MPISimulator::insertOpToPendingList(MPIOperation *op){
	//	cout<<"The mpi op "<<op->getOpName()<<" is going to be inserted into the pending list."<<endl;

	//if the op is collective op, then the only thing needs to do is
	//accumulate the executors. Once every process checked in, 
	//the collective op can actually happen.
	if (op->isCollectiveOp())
	{
		vector<MPIOperation*> happenedCollectiveOPs=this->collectiveOpMgr.insertCollectiveOP(op);
		for (int i = 0; i < happenedCollectiveOPs.size(); i++)
		{
			this->insertMPIOpToMPITree(happenedCollectiveOPs.at(i));
		}
		return;
	}


	for (int i = 0; i < this->pendingOPs.size(); i++)
	{
		if(op->getExecutor().isIgnored())
			return;

		MPIOperation *curVisitOP=pendingOPs[i];

		cout<<"The cur visited mpi op is "<<curVisitOP->getOpName()<<endl;
		//Avoid the same action in the same node to be performed by multiple roles
		if (op->theNode==curVisitOP->theNode)
		{
			Condition workNeedsDoing=op->getExecutor().Diff(curVisitOP->getExecutor());
			op->setExecutorCond(workNeedsDoing);
			if (op->isDependentOnExecutor())
			{
				op->setTargetCond(this->getTarCondFromExprAndExecCond(op->getTargetExpr(),workNeedsDoing));
			}

			continue;
		}


		//if the operations are complementary
		if (op->isComplementaryOpOf(curVisitOP))
		{

			Condition targetOfThisOp=op->getTargetCond();
			Condition execCondOfCurVisitOp=curVisitOP->getExecutor();
			Condition actualTarget=targetOfThisOp.AND(execCondOfCurVisitOp);

			Condition execOfThisOp=op->getExecutor();
			Condition targetOfCurOp=curVisitOP->getTargetCond();
			Condition actualExecutor=execOfThisOp.AND(targetOfCurOp);

			Condition remainingExecCond4CurVisitOp=execCondOfCurVisitOp.Diff(actualTarget);
			Condition remainingExecCond4ThisOp=execOfThisOp.Diff(actualExecutor);
			Condition remainingTarCond4ThisOp=targetOfThisOp.Diff(actualTarget);
			Condition remainingTarCond4CurVisitOp=targetOfCurOp.Diff(actualExecutor);



			if(actualTarget.isIgnored())
				continue;


			MPIOperation *actuallyHappenedOP=new MPIOperation(*op);
			actuallyHappenedOP->setExecutorCond(actualExecutor);
			actuallyHappenedOP->setTargetCond(actualTarget);


			string outToFile="\n\n\nThe actually happened MPI OP is :\n";
			outToFile.append(actuallyHappenedOP->printMPIOP());
			writeToFile(outToFile);

			cout<<outToFile<<endl;

			/////////////////////////////////////////////////////////////////////////////////////////////////
			Condition unblockedRoleCond(false);

			if(op->isUnicast()){
				//After the comm happens, some roles may be unblocked
				unblockedRoleCond= actuallyHappenedOP->isBlockingOP()?
					actuallyHappenedOP->getExecutor():actuallyHappenedOP->getTargetCond();
			}

			else{
				if (actuallyHappenedOP->isBlockingOP())
				{
					if(actualTarget.isSameAsCond(targetOfThisOp))
						unblockedRoleCond=actualExecutor;
				}

				else{
					if(actualExecutor.isSameAsCond(targetOfCurOp))
						unblockedRoleCond=actualTarget;
				}

			}


			cout<<"The cond "<<unblockedRoleCond.printConditionInfo()<<" is unblocked!"<<endl;

			Condition nonBlockingRoleCond= actuallyHappenedOP->isBlockingOP()?
				actuallyHappenedOP->getTargetCond():actuallyHappenedOP->getExecutor();

			CommNode *unblockingNode= op->isBlockingOP()?
				op->theNode:curVisitOP->theNode;

			CommNode *nonBlockedNode= op->isBlockingOP()?
				curVisitOP->theNode:op->theNode;

			unblockingNode->setCond(unblockingNode->getCond().Diff(unblockedRoleCond));
			nonBlockedNode->setCond(nonBlockedNode->getCond().Diff(nonBlockingRoleCond));

			for (int i = 0; i < unblockedRoleCond.getRangeList().size(); i++)
			{
				Role *unblockedRole=new Role(unblockedRoleCond.getRangeList()[i]);
				unblockedRole->setCurVisitNode(unblockingNode);
				ParamRole *paramRoleI=this->commManager->getParamRoleWithName(unblockedRole->getParamRoleName());
				paramRoleI->insertActualRole(unblockedRole,true);
			}



			//////////////////////////////////////////////////////////////////////////////////////////////////////////////
			this->insertMPIOpToMPITree(actuallyHappenedOP);
			/////////////////////////////////////////////////////////////////////////////////////////////////////////////


			///////////////////////////////////////////////////////////////////////////////////////////////////////
			//update the inserted op and cur visit op

			if (op->isUnicast() && curVisitOP->isUnicast())
			{
				op->setExecutorCond(remainingExecCond4ThisOp);
				op->setTargetCond(remainingTarCond4ThisOp);

				//if both ops are unicast.
				MPIOperation *updatedCurOP=new MPIOperation(*curVisitOP);
				updatedCurOP->setExecutorCond(remainingExecCond4CurVisitOp);
				updatedCurOP->setTargetCond(remainingTarCond4CurVisitOp);

				if (updatedCurOP->isEmptyOP())
				{
					delete updatedCurOP;
					updatedCurOP=nullptr;
				}

				if (updatedCurOP)
				{
					int index=curVisitOP->theNode->indexOfTheMPIOP(curVisitOP);
					CommNode *tmpNode=curVisitOP->theNode;
					delete curVisitOP;

					tmpNode->getOPs()->at(index)=updatedCurOP;

					this->pendingOPs[i]=updatedCurOP;
				}

				else{
					curVisitOP=nullptr;

					this->pendingOPs.erase(this->pendingOPs.begin()+i);
					i--;
				}
			}

			else{//one of the two ops is not unicast

				MPIOperation* thisOP1=new MPIOperation(*op);
				thisOP1->setExecutorCond(actualExecutor);
				thisOP1->setTargetCond(remainingTarCond4ThisOp);
				if(thisOP1->isEmptyOP())
				{
					delete thisOP1;
					thisOP1=nullptr;
				}

				MPIOperation* thisOP2=new MPIOperation(*op);
				thisOP2->setExecutorCond(remainingExecCond4ThisOp);
				thisOP2->setTargetCond(targetOfThisOp);
				if(thisOP2->isEmptyOP())
				{
					delete thisOP2;
					thisOP2=nullptr;
				}

				MPIOperation* curOP1=new MPIOperation(*curVisitOP);
				curOP1->setExecutorCond(actualTarget);
				curOP1->setTargetCond(remainingTarCond4CurVisitOp);
				if(curOP1->isEmptyOP())
				{
					delete curOP1;
					curOP1=nullptr;
				}


				MPIOperation* curOP2=new MPIOperation(*curVisitOP);
				curOP2->setExecutorCond(remainingExecCond4CurVisitOp);
				curOP2->setTargetCond(targetOfCurOp);
				if(curOP2->isEmptyOP())
				{
					delete curOP2;
					curOP2=nullptr;
				}


				if (thisOP1 && thisOP2)
				{
					int index=op->theNode->indexOfTheMPIOP(op);
					CommNode *tmpNode=op->theNode;
					delete op;

					tmpNode->getOPs()->at(index)=thisOP1;
					tmpNode->insertMPIOP(thisOP2);

					this->insertOpToPendingList(thisOP1);
					this->insertOpToPendingList(thisOP2);

					return;
				}

				else if(thisOP1){
					int index=op->theNode->indexOfTheMPIOP(op);
					CommNode *tmpNode=op->theNode;
					delete op;

					tmpNode->getOPs()->at(index)=thisOP1;

					op=thisOP1;
					continue;
				}

				else if(thisOP2){
					int index=op->theNode->indexOfTheMPIOP(op);
					CommNode *tmpNode=op->theNode;
					delete op;

					tmpNode->getOPs()->at(index)=thisOP2;

					op=thisOP2;
					continue;
				}

				else{
					int index=op->theNode->indexOfTheMPIOP(op);
					CommNode *tmpNode=op->theNode;
					delete op;

					tmpNode->getOPs()->at(index)=nullptr;

					return;
				}
				//////////////////////////////////////////////////////////////////////////////////////////////////////////

				////////////////////////////////////////////////////////
				if (curOP1 && curOP2)
				{
					int index=curVisitOP->theNode->indexOfTheMPIOP(curVisitOP);
					CommNode *tmpNode=curVisitOP->theNode;
					delete curVisitOP;

					tmpNode->getOPs()->at(index)=curOP1;
					curVisitOP=curOP1;

					this->pendingOPs[i]=curVisitOP;
					curOP2->theNode->insertMPIOP(curOP2);
					this->pendingOPs.push_back(curOP2);
				}

				else if(curOP1){
					int index=curVisitOP->theNode->indexOfTheMPIOP(curVisitOP);
					CommNode *tmpNode=curVisitOP->theNode;
					delete curVisitOP;

					tmpNode->getOPs()->at(index)=curOP1;
					curVisitOP=curOP1;

					this->pendingOPs[i]=curVisitOP;
				}

				else if(curOP2){
					int index=curVisitOP->theNode->indexOfTheMPIOP(curVisitOP);
					CommNode *tmpNode=curVisitOP->theNode;
					delete curVisitOP;

					tmpNode->getOPs()->at(index)=curOP2;
					curVisitOP=curOP2;
					this->pendingOPs[i]=curVisitOP;
				}

				else{
					int index=curVisitOP->theNode->indexOfTheMPIOP(curVisitOP);
					CommNode *tmpNode=curVisitOP->theNode;
					delete curVisitOP;

					tmpNode->getOPs()->at(index)=nullptr;

					this->pendingOPs.erase(this->pendingOPs.begin()+i);
					i--;
				}
			}

		}
	}

	if(op->getExecutor().isIgnored()){
		bool tmpB=op->theNode->isNegligible();
		return;
	}
	//non of the existing ops have anything to do with the inserted op.
	this->pendingOPs.push_back(op);

	cout<<"Insert "<<op->getOpName()<<" to the back of the pending list."<<endl;

}


void MPISimulator::insertMPIOpToMPITree(MPIOperation *actuallyHappenedOP){
	CommNode *masterNode=actuallyHappenedOP->theNode->getClosestNonRankAncestor();
	MPINode *theParentMPINode=this->posIndexAndMPINodeMapping[masterNode->getPosIndex()];
	if (theParentMPINode)
	{
		theParentMPINode->insert(new MPINode(actuallyHappenedOP));
	}

}
/********************************************************************/
//Class MPISimulator impl end									****
/********************************************************************/