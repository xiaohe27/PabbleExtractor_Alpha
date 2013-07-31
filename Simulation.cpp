#include "Comm.h"

using namespace llvm;
using namespace clang;
using namespace std;


//////////////////////VisitResult//////////////////////////////////////////
VisitResult::VisitResult(bool b, MPIOperation *op, vector<Role*> roles){
	this->isBlocking=b;
	this->doableOP=op;
	this->escapableRoles=roles;
}

void VisitResult::printVisitInfo(){
	if (this->doableOP)
	{
		cout<<"\n\n\nThe doable MPI operation in this node is "<<endl;
		this->doableOP->printMPIOP();

		cout<<"It is "<<(this->isBlocking?"":"non-")<<"blocking operation!\n\n\n"<<endl;
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


MPISimulator::MPISimulator(CommManager *commMgr){
	this->commManager=commMgr;

	root=new CommNode(ST_NODE_ROOT);
	root->setCond(true);

	curNode=root;

}

void MPISimulator::insertNode(CommNode *node){

	//set the condition for the node.
	node->setCond(this->commManager->getTopCondition());

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


void MPISimulator::gotoParent(){
	this->curNode= this->curNode->getParent();
}


bool MPISimulator::isDeadLockDetected(){
	bool blocking=false;

	for (auto &x:this->commManager->getParamRoleMapping())
	{
		int size=x.second ->getTheRoles().size();
		for (int i = 0; i < size; i++)
		{
			if (x.second->getTheRoles()[i]->hasFinished())
			{
				continue;
			}

			blocking=x.second->getTheRoles()[i]->IsBlocked();

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

		vector<Role*> roles= paramRole->getTheRoles();

		for (int i = 0; i < roles.size(); i++)
		{
			roles[i]->setCurVisitNode(this->root);
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

		vector<Role*> roles= paramRole->getTheRoles();

		for (int i = 0; i < roles.size(); i++)
		{
			cout<<"The role "<<i<<" is "<< roles.at(i)->getRoleName()<<endl;
		}
	}

}

bool MPISimulator::areAllRolesDone(){
	map<string,ParamRole*> paramRoleMap=this->commManager->getParamRoleMapping();

	for (auto &x: paramRoleMap)
	{
		ParamRole *paramRole=x.second;

		vector<Role*> roles= paramRole->getTheRoles();

		for (int i = 0; i < roles.size(); i++)
		{
			if (!roles[i]->hasFinished())
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


			for (int i = 0; i < paramRole->getTheRoles().size(); i++)
			{
				if (paramRole->getTheRoles()[i]->hasFinished())
				{
					paramRole->getTheRoles().erase(paramRole->getTheRoles().begin()+i);

					i--;
					continue;
				}
				
				cout<<"It is "<< paramRole->getTheRoles().at(i)->getRoleName()<<" visiting the tree now."<<endl;
				VisitResult *vr=paramRole->getTheRoles()[i]->visit();

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

	if (root->isMarked())
	{
		cout<<"\n\n\nThe comm tree has been traversed successfully!!!"<<endl;
	}

	else
	cout<<"\n\n\nDeadlock occurs!!!";
}


Condition MPISimulator::getTarCondFromExprAndExecCond(Expr *expr, Condition execCond){
	this->commManager->simplyInsertCond(execCond);

	Condition newTarget=this->commManager->extractCondFromTargetExpr(expr);

	this->commManager->popCondition();

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
			paramRole->insertActualRole(vr->escapableRoles.at(i));
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
	//TODO
//	cout<<"The mpi op "<<op->getOpName()<<" is going to be inserted into the pending list."<<endl;

	
	for (int i = 0; i < this->pendingOPs.size(); i++)
	{
		if(op->getExecutor().isIgnored())
			return;
		
		MPIOperation *curVisitOP=pendingOPs[i];

		cout<<"The cur visited mpi op is "<<curVisitOP->getOpName()<<endl;
		//Avoid the same action to be performed by multiple roles
		if (op->isSameKindOfOp(curVisitOP))
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
			Condition targetOfOp=op->getTargetCond();
			Condition execCondOfCurVisitOp=curVisitOP->getExecutor();
			Condition thisTarCurExec=targetOfOp.AND(execCondOfCurVisitOp);

			Condition execOfOp=op->getExecutor();
			Condition targetOfCurOp=curVisitOP->getTargetCond();
			Condition thisExecCurTarget=execOfOp.AND(targetOfCurOp);

			Condition remainingExecCond4CurVisitOp=execCondOfCurVisitOp.Diff(thisTarCurExec);
			Condition remainingTarCond4CurVisitOp=targetOfCurOp.Diff(thisExecCurTarget);

			Condition remainingExecCond4ThisOp=execOfOp.Diff(thisExecCurTarget);
			Condition remainingTarCond4ThisOp=targetOfOp.Diff(thisTarCurExec);

//			cout<<"The condition for the target of inserted op and executor of the cur visit op is : "<<
//				thisTarCurExec.printConditionInfo()<<endl;

//			cout<<"The condition for the executor of inserted op and target of the cur visit op is : "<<
//				thisExecCurTarget.printConditionInfo()<<endl;

			if(thisTarCurExec.isIgnored())
				continue;


			MPIOperation *actuallyHappenedOP=new MPIOperation(*op);
			actuallyHappenedOP->setExecutorCond(thisExecCurTarget);
			actuallyHappenedOP->setTargetCond(thisTarCurExec);

			cout<<"\n\n\nThe actually happened MPI OP is :\n";
			actuallyHappenedOP->printMPIOP();
			cout<<"\n\n\n"<<endl;


			//After the comm happens, some roles may be unblocked
			Condition unblockedRoleCond= actuallyHappenedOP->isBlockingOP()?
					actuallyHappenedOP->getExecutor():actuallyHappenedOP->getTargetCond();

			//the non-blocked role cond is the cond for the nonblocking role.
			Condition nonBlockingRoleCond= actuallyHappenedOP->isBlockingOP()?
					actuallyHappenedOP->getTargetCond():actuallyHappenedOP->getExecutor();

			CommNode *unblockingNode= op->isBlockingOP()?
					op->theNode:curVisitOP->theNode;

			CommNode *nonBlockedNode= op->isBlockingOP()?
					curVisitOP->theNode:op->theNode;

			cout<<"The cond "<<unblockedRoleCond.printConditionInfo()<<" is unblocked!";
			
			unblockingNode->setCond(unblockingNode->getCond().Diff(unblockedRoleCond));

			nonBlockedNode->setCond(nonBlockedNode->getCond().Diff(nonBlockingRoleCond));

			for (int i = 0; i < unblockedRoleCond.getRangeList().size(); i++)
			{
				Role *unblockedRole=new Role(unblockedRoleCond.getRangeList()[i]);
				unblockedRole->setCurVisitNode(unblockingNode);

				ParamRole *paramRoleI=this->commManager->getParamRoleWithName(unblockedRole->getParamRoleName());
				paramRoleI->insertActualRole(unblockedRole);
			}


			//////////////////////////////////////////////////////////////
			//update the inserted op
			if (remainingExecCond4ThisOp.isIgnored())
			{
				if(remainingExecCond4CurVisitOp.isIgnored()){
				//remove the elem at index i
				this->pendingOPs.erase(this->pendingOPs.begin()+i);	
				}

				else{
					curVisitOP->setExecutorCond(remainingExecCond4CurVisitOp);
					curVisitOP->setTargetCond(remainingTarCond4CurVisitOp);
				}

				//no proc is going to exec the op
				return;
			}
			
			op->setExecutorCond(remainingExecCond4ThisOp);
			op->setTargetCond(remainingTarCond4ThisOp);

			//update the cur op in the vector
			if(remainingExecCond4CurVisitOp.isIgnored()){
				//remove the elem at index i
				this->pendingOPs.erase(this->pendingOPs.begin()+i);	
				i--;
				continue;
			}

			else{
			curVisitOP->setExecutorCond(remainingExecCond4CurVisitOp);
			curVisitOP->setTargetCond(remainingTarCond4CurVisitOp);
			}
		}
	}

	if(op->getExecutor().isIgnored())
		return;
	//non of the existing ops have anything to do with the inserted op.
	this->pendingOPs.push_back(op);

	cout<<"Insert "<<op->getOpName()<<" to the back of the pending list."<<endl;

}

/********************************************************************/
//Class MPISimulator impl end									****
/********************************************************************/