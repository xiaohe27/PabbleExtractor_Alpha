#include "Comm.h"

using namespace llvm;
using namespace clang;



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
		if(escapableRoles[i]->getCurVisitNode())
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

	if(!this->curNode->getCond().isComplete() && nodeT==MPI_Barrier)
		throw new MPI_TypeChecking_Error("MPI Barrier can only be put under a non-rank related node.");


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

	if (node->getNodeType()==MPI_Wait)
	{
		WaitNode *wn=(WaitNode*)node;
		if(wn->req.size()!=0){
			MPIOperation *matchedOP=this->curNode->findTheClosestNonblockingOPWithReqName(wn->req);
			if(matchedOP==nullptr)
				return; //the work of this wait node has been done by some other similar wait node,no need to repeat.

			else
				matchedOP->theWaitNode=wn;
		}
	}

	//insert the node
	this->curNode->insert(node);

	if(nodeT==ST_NODE_CHOICE || nodeT==ST_NODE_RECUR 
		|| nodeT==ST_NODE_FOREACH || nodeT==ST_NODE_ROOT){
			this->curNode=node;

			VarCondMap[RANKVAR]=this->getCurExecCond();
	}

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
	map<string,ParamRole*> paramRoleMap=this->commManager->getParamRoleMapping();

	vector<Role*> *worldRoles=paramRoleMap[WORLD]->getTheRoles();
	for (int i = 0; i < worldRoles->size(); i++)
		delete worldRoles->at(i);

	worldRoles->clear();
	Role *initRole=new Role(Range(0,N-1));
	initRole->setCurVisitNode(this->root);
	paramRoleMap[WORLD]->getTheRoles()->push_back(initRole);

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
	this->root->optimize();

	VarCondMap[RANKVAR]=Condition(true);

	cout<<"Ready to simulate the execution of the MPI program now!"<<endl;
	const map<string,ParamRole*> paramRoleMap=this->commManager->getParamRoleMapping();
	cout<<"There are "<<paramRoleMap.size()<<" communicator groups involved"<<endl;

	this->printTheRoles();

	//only the role [0..N-1] is needed at first.
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

	else if(isDeadLockDetected()){
		cout<<"\n\n\nDeadlock occurs!!!\n";
		string errOut="The current pending operations are:\n";
		for (int i = 0; i < this->pendingOPs.size(); i++)
		{
			errOut+=this->pendingOPs.at(i)->srcCode+"\n";
		}

		errOut+="\n"+this->collectiveOpMgr.getCurPendingCollectiveOPName()+"\n";

		cout<<errOut<<endl;
	}
}


Condition MPISimulator::getTarCondFromExprAndExecCond(Expr *expr, Condition execCond){
	Condition rankCondBak=VarCondMap[RANKVAR];
	VarCondMap[RANKVAR]=execCond;
	Condition newTarget=this->commManager->extractCondFromTargetExpr(expr);

	VarCondMap[RANKVAR]=rankCondBak;
	return newTarget;
}


//need to improve precision
Condition MPISimulator::getExecCondFromExprAndProcCond(Expr *expr, string procVar,Condition procCond){
	Condition procCondBak=VarCondMap[procVar];
	VarCondMap[procVar]=procCond;
	Condition newExecCond=this->commManager->extractCondFromBoolExpr(expr);

	VarCondMap[procVar]=procCondBak;
	return newExecCond;
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
		if(op->isDependentOnExecutor() && op->getTargetExpr()){
			Condition tarCond=this->getTarCondFromExprAndExecCond(op->getTargetExpr(),op->getExecutor());

			if(tarCond.outsideOfBound())
				throw new MPI_TypeChecking_Error("\nTarget condition for the op "+
				op->srcCode+" is "+tarCond.printConditionInfo()+"\nRank outside of bound.");

			op->setTargetCond(tarCond);

		}

		//if the op is collective op, then the only thing needs to do is
		//accumulate the executors. Once every process checked in, 
		//the collective op can actually happen.
		if (op->isCollectiveOp())
		{
			MPIOperation* happenedCollectiveOP=this->collectiveOpMgr.insertCollectiveOP(op);

			if(happenedCollectiveOP){
				this->insertMPIOpToMPITree(happenedCollectiveOP);

				//optimise the role list
				ParamRole *globalGroup=this->commManager->getParamRoleWithName(WORLD);
				globalGroup->getTheRoles()->clear();
				Role *newStartRole=new Role(Range(0,N-1));
				newStartRole->setCurVisitNode(happenedCollectiveOP->theNode->skipToNextNode());
				globalGroup->getTheRoles()->push_back(newStartRole);
			}

			return;
		}

		else{
			int execSize=op->getExecutor().size();
			if(op->isUnicast() || op->isMulticast() || op->isGatherOp())
				this->insertOpToPendingList(op);

			else{
				for (int i = 0; i < op->getExecutor().getRangeList().size(); i++)
				{
					Range ran=op->getExecutor().getRangeList().at(i);	
					if (ran.isSpecialRange()){
						for (int j = 0; j <= ran.getEnd(); j++){
							MPIOperation *opJ=new MPIOperation(*op);
							opJ->setExecutorCond(Condition(Range(j,j)));
							this->insertOpToPendingList(opJ);
						}

						for (int k = ran.getStart(); k < N; k++){
							MPIOperation *opK=new MPIOperation(*op);
							opK->setExecutorCond(Condition(Range(k,k)));
							this->insertOpToPendingList(opK);
						}
					}

					else{
						for (int j = ran.getStart(); j <= ran.getEnd(); j++){
							MPIOperation *opJ=new MPIOperation(*op);
							opJ->setExecutorCond(Condition(Range(j,j)));
							this->insertOpToPendingList(opJ);
						}
					}
				}
			}

		}

	}
}


void MPISimulator::insertOpToPendingList(MPIOperation *op){
	//	cout<<"The mpi op "<<op->getOpName()<<" is going to be inserted into the pending list."<<endl;

	int sizOfE=op->getExecutor().size();
	int sizOfT=op->getTargetCond().size();
	if(sizOfE>1 && sizOfT>1 && sizOfE != sizOfT)
		throw new MPI_TypeChecking_Error
		("The MPI operation with multiple executors and targets needs to be normalized before insertion.");

	for (int i = 0; i < this->pendingOPs.size(); i++)
	{
		if(op->isEmptyOP())
			return;

		MPIOperation *curVisitOP=pendingOPs[i];

		cout<<"The cur visited mpi op is "<<curVisitOP->getOpName()<<endl;
		//Avoid the same action in the same node to be performed by multiple roles
		if (op->theNode==curVisitOP->theNode)
		{
			if(!op->getExecutor().AND(curVisitOP->getExecutor()).isIgnored())
				throw new MPI_TypeChecking_Error("The same task is repeated by "+op->srcCode);
		}


		//if the operations are complementary
		if (op->isComplementaryOpOf(curVisitOP))
		{
			//assume the actual executor and target of the happened op are the same as this op.
			Condition targetOfThisOp=op->getTargetCond();
			Condition execCondOfCurVisitOp=curVisitOP->getExecutor();
			Condition actualTarget=targetOfThisOp.AND(execCondOfCurVisitOp);

			Condition execOfThisOp=op->getExecutor();
			Condition targetOfCurOp=curVisitOP->getTargetCond();
			Condition actualExecutor=execOfThisOp.AND(targetOfCurOp);

			if(!op->isUnicast() && curVisitOP->isUnicast()){
				if(execOfThisOp.size()==1){
					Condition tmp=actualExecutor;
					actualTarget=tmp.addANumber
						(-Condition::getDistBetweenTwoCond(execCondOfCurVisitOp,targetOfCurOp));
				}

				else if(targetOfThisOp.size()==1){
					Condition tmp=actualTarget;
					actualExecutor=tmp.addANumber
						(Condition::getDistBetweenTwoCond(execCondOfCurVisitOp,targetOfCurOp));
				}
			}

			/////////////////////////////////////////////////////////
			if(!curVisitOP->isUnicast() && op->isUnicast()){
				if(execCondOfCurVisitOp.size()==1){
					Condition tmp=actualTarget;
					actualExecutor=tmp.addANumber
						(-Condition::getDistBetweenTwoCond(execOfThisOp,targetOfThisOp));
				}

				else if(targetOfCurOp.size()==1){
					Condition tmp=actualExecutor;
					actualTarget=tmp.addANumber
						(Condition::getDistBetweenTwoCond(execOfThisOp,targetOfThisOp));
				}
			}

			////////////////////////////////////////////////////////////////////////////////////////

			Condition remainingExecCond4CurVisitOp=execCondOfCurVisitOp.Diff(actualTarget);
			Condition remainingExecCond4ThisOp=execOfThisOp.Diff(actualExecutor);
			Condition remainingTarCond4ThisOp=targetOfThisOp.Diff(actualTarget);
			Condition remainingTarCond4CurVisitOp=targetOfCurOp.Diff(actualExecutor);


			if(actualTarget.isIgnored() || actualExecutor.isIgnored())
				continue;

			//VERY IMPORTANT! NEED TO ENSURE THE EXPECTED TARGET IS THE SAME AS THE ACTUAL ONE!
			if(op->isDependentOnExecutor() && op->getTargetExpr()){
				Condition expectedTar=this->getTarCondFromExprAndExecCond(
					op->getTargetExpr(), actualExecutor);

				if (!expectedTar.isSameAsCond(actualTarget))
					continue;
			}

			///////////////////////////////////////////////////////////////////////////////////////////
			//test whether the matching is perfect or not.
			if(execCondOfCurVisitOp.size()==N) execCondOfCurVisitOp.setVolatile(false);
			if(execOfThisOp.size()==N) execCondOfCurVisitOp.setVolatile(false);
			if(targetOfCurOp.size()==N) targetOfCurOp.setVolatile(false);
			if(targetOfThisOp.size()==N) targetOfThisOp.setVolatile(false);

			bool isDiff1= (execOfThisOp.isVolatile || targetOfCurOp.isVolatile) &&
				!(execOfThisOp.isVolatile && targetOfCurOp.isVolatile);

			bool isDiff2= (execCondOfCurVisitOp.isVolatile || targetOfThisOp.isVolatile) &&
				!(execCondOfCurVisitOp.isVolatile && targetOfThisOp.isVolatile);


			if(isDiff1 || isDiff2)
				IsGenericProtocol=false; //we found the imperfect matching! 

			//////////////////////////////////////////////////////////////////////////////////////////
			MPIOperation *actuallyHappenedOP;
			if(op->isUnicast() && curVisitOP->isUnicast()){
				op->theNode->setCond(op->theNode->getCond().Diff(actualExecutor));
				curVisitOP->theNode->setCond(curVisitOP->theNode->getCond().Diff(actualTarget));

				if (op->theWaitNode){			
					op->theWaitNode->reportFinished(actualExecutor);
					this->unblockTheRolesWithCond(actualExecutor,op->theNode);		
				}

				if (curVisitOP->theWaitNode){				
					curVisitOP->theWaitNode->reportFinished(actualTarget);
					this->unblockTheRolesWithCond(actualTarget,curVisitOP->theNode);				
				}
			}

			else{
				if(op->getTargetCond().isSameAsCond(actualTarget))
					op->theNode->setCond(op->theNode->getCond().Diff(actualExecutor));

				if(curVisitOP->getTargetCond().isSameAsCond(actualExecutor))
					curVisitOP->theNode->setCond(curVisitOP->theNode->getCond().Diff(actualTarget));



				if (op->theWaitNode){
					if(op->getTargetCond().isSameAsCond(actualTarget)){
						op->theWaitNode->reportFinished(actualExecutor);
						this->unblockTheRolesWithCond(actualExecutor,op->theNode);
					}
				}

				if (curVisitOP->theWaitNode){
					if(curVisitOP->getTargetCond().isSameAsCond(actualExecutor)){
						curVisitOP->theWaitNode->reportFinished(actualTarget);
						this->unblockTheRolesWithCond(actualTarget,curVisitOP->theNode);
					}
				}

			}
			/////////////////////////////////////////////////////////////////////////////////////////////////////////

			if (op->isSendingOp())
				actuallyHappenedOP=new MPIOperation(*op);
			else if(curVisitOP->isSendingOp()){
				actuallyHappenedOP=new MPIOperation(*curVisitOP);
				Condition theTmpCond=actualExecutor;
				actualExecutor=actualTarget;
				actualTarget=theTmpCond;
			} else{
				throw new MPI_TypeChecking_Error
					("In order to make an interaction happen, one of the ops must be sending op!");
			}

			actuallyHappenedOP->setExecutorCond(actualExecutor);
			actuallyHappenedOP->setTargetCond(actualTarget);


			string outToFile="\n\n\nThe actually happened MPI OP is :\n";
			outToFile.append(actuallyHappenedOP->printMPIOP());
			writeToFile(outToFile);

			cout<<outToFile<<endl;

			/////////////////////////////////////////////////////////////////////////////////////////////////
			Condition unblockedRoleCond(false);

			if(op->isUnicast() && curVisitOP->isUnicast()){
				//After the comm happens, some roles may be unblocked
				unblockedRoleCond= actuallyHappenedOP->getTargetCond();
			}

			else if(op->isSendingOp() && actualExecutor.isSameAsCond(targetOfCurOp))
				unblockedRoleCond=actualTarget;

			else if(curVisitOP->isSendingOp() && actualExecutor.isSameAsCond(targetOfThisOp))
				unblockedRoleCond=actualTarget;

			cout<<"The cond "<<unblockedRoleCond.printConditionInfo()<<" is unblocked!"<<endl;
			////////////////////////////////////////////////////////////////////////////////////////////////////////////


			CommNode *unblockingNode= op->isRecvingOp()?
				op->theNode:curVisitOP->theNode;


			this->unblockTheRolesWithCond(unblockedRoleCond,unblockingNode);

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

				curVisitOP->setExecutorCond(remainingExecCond4CurVisitOp);
				curVisitOP->setTargetCond(remainingTarCond4CurVisitOp);

				if (curVisitOP->isEmptyOP())
				{
					int index=curVisitOP->theNode->indexOfTheMPIOP(curVisitOP);
					CommNode *tmpNode=curVisitOP->theNode;
					delete curVisitOP;

					if(index>=0)
						tmpNode->getOPs()->at(index)=nullptr;

					this->pendingOPs.erase(this->pendingOPs.begin()+i);
					i--;
				}

				if (op->isEmptyOP())
				{
					int index=op->theNode->indexOfTheMPIOP(op);
					CommNode *thisTmpNode=op->theNode;
					delete op;
					if(index>=0)
						thisTmpNode->getOPs()->at(index)=nullptr;

					return;
				}
			}

			else{//one of the two ops is not unicast
				MPIOperation *thisOP1, *thisOP2;
				if(!op->isUnicast()){
					thisOP1=new MPIOperation(*op);
					thisOP1->setExecutorCond(op->getExecutor().Diff(remainingExecCond4ThisOp));
					thisOP1->setTargetCond(remainingTarCond4ThisOp);
					if(thisOP1->isEmptyOP())
					{
						delete thisOP1;
						thisOP1=nullptr;
					}

					thisOP2=new MPIOperation(*op);
					thisOP2->setExecutorCond(remainingExecCond4ThisOp);
					thisOP2->setTargetCond(targetOfThisOp);
					if(thisOP2->isEmptyOP())
					{
						delete thisOP2;
						thisOP2=nullptr;
					}
				} else{

					thisOP1=new MPIOperation(*op);
					thisOP1->setExecutorCond(remainingExecCond4ThisOp);
					thisOP1->setTargetCond(remainingTarCond4ThisOp);
					if(thisOP1->isEmptyOP())
					{
						delete thisOP1;
						thisOP1=nullptr;
					}

					thisOP2=nullptr;
				}


				MPIOperation *curOP1, *curOP2;

				if(!curVisitOP->isUnicast()){
					curOP1=new MPIOperation(*curVisitOP);
					curOP1->setExecutorCond(curVisitOP->getExecutor().Diff(remainingExecCond4CurVisitOp));
					curOP1->setTargetCond(remainingTarCond4CurVisitOp);
					if(curOP1->isEmptyOP())
					{
						delete curOP1;
						curOP1=nullptr;
					}

					curOP2=new MPIOperation(*curVisitOP);
					curOP2->setExecutorCond(remainingExecCond4CurVisitOp);
					curOP2->setTargetCond(targetOfCurOp);
					if(curOP2->isEmptyOP())
					{
						delete curOP2;
						curOP2=nullptr;
					}
				} else{
					curOP1=new MPIOperation(*curVisitOP);
					curOP1->setExecutorCond(remainingExecCond4CurVisitOp);
					curOP1->setTargetCond(remainingTarCond4CurVisitOp);
					if(curOP1->isEmptyOP())
					{
						delete curOP1;
						curOP1=nullptr;
					}
					curOP2=nullptr;
				}

				///////////////////////////////////////////////////////////////////////////////////////////////////////
				if (curOP1 && curOP2)
				{
					int index=curVisitOP->theNode->indexOfTheMPIOP(curVisitOP);
					CommNode *tmpNode=curVisitOP->theNode;
					delete curVisitOP;

					if(index>=0)
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

					if(index>=0)
						tmpNode->getOPs()->at(index)=curOP1;

					curVisitOP=curOP1;

					this->pendingOPs[i]=curVisitOP;
				}

				else if(curOP2){
					int index=curVisitOP->theNode->indexOfTheMPIOP(curVisitOP);
					CommNode *tmpNode=curVisitOP->theNode;
					delete curVisitOP;

					if(index>=0)
						tmpNode->getOPs()->at(index)=curOP2;
					curVisitOP=curOP2;
					this->pendingOPs[i]=curVisitOP;
				}

				else{
					int index=curVisitOP->theNode->indexOfTheMPIOP(curVisitOP);
					CommNode *tmpNode=curVisitOP->theNode;
					delete curVisitOP;

					if(index>=0)
						tmpNode->getOPs()->at(index)=nullptr;

					this->pendingOPs.erase(this->pendingOPs.begin()+i);
					i--;
				}


				///////////////////////////////////////////////////////////////////////////////////////////////////////////////
				if (thisOP1 && thisOP2)
				{
					int index=op->theNode->indexOfTheMPIOP(op);
					CommNode *tmpNode=op->theNode;
					delete op;

					if(index>=0)
						tmpNode->getOPs()->at(index)=thisOP1;
					tmpNode->insertMPIOP(thisOP2);

					//the time of insertion is vital, 
					//if insert too early, then the cur visit ops
					//will not have not been updated
					this->insertOpToPendingList(thisOP1);
					this->insertOpToPendingList(thisOP2);

					return;
				}

				else if(thisOP1){
					int index=op->theNode->indexOfTheMPIOP(op);
					CommNode *tmpNode=op->theNode;
					delete op;

					if(index>=0)
						tmpNode->getOPs()->at(index)=thisOP1;

					op=thisOP1;

				}

				else if(thisOP2){
					int index=op->theNode->indexOfTheMPIOP(op);
					CommNode *tmpNode=op->theNode;
					delete op;

					if(index>=0)
						tmpNode->getOPs()->at(index)=thisOP2;

					op=thisOP2;

				}

				else{
					int index=op->theNode->indexOfTheMPIOP(op);
					CommNode *tmpNode=op->theNode;
					delete op;

					if(index>=0)
						tmpNode->getOPs()->at(index)=nullptr;

					return;
				}

				///////////////////////////////////////////////////////////////////////////////////////////////////////////////


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
	//make sure the MPI operation does not incur runtime error
	if(!actuallyHappenedOP->isCollectiveOp()) {
		Condition execCond=actuallyHappenedOP->getExecutor();
		Condition tarCond=actuallyHappenedOP->getTargetCond();
		if(execCond.isSameAsCond(tarCond) ||
			execCond.Diff(tarCond).isIgnored() ||
			tarCond.Diff(execCond).isIgnored())

			if(execCond.execStr != tarCond.execStr && (execCond.isVolatile || tarCond.isVolatile))
			throw new MPI_TypeChecking_Error("\n\n"+actuallyHappenedOP->srcCode+" \n\n"
			+"involves action of sending data to itself...Will incur runtime error!");
	}

	CommNode *masterNode=actuallyHappenedOP->theNode->getClosestNonRankAncestor();
	MPINode *theParentMPINode=this->posIndexAndMPINodeMapping[masterNode->getPosIndex()];
	if (theParentMPINode)
	{
		theParentMPINode->insert(new MPINode(actuallyHappenedOP));
	}

}


void MPISimulator::unblockTheRolesWithCond(Condition unblockedRoleCond, CommNode *unblockingNode){
	unblockedRoleCond=unblockedRoleCond.AND(Condition(true));
	for (int i = 0; i < unblockedRoleCond.getRangeList().size(); i++)
	{
		Role *unblockedRole=new Role(unblockedRoleCond.getRangeList()[i]);
		unblockedRole->setCurVisitNode(unblockingNode);
		ParamRole *paramRoleI=this->commManager->getParamRoleWithName(unblockedRole->getParamRoleName());
		paramRoleI->insertActualRole(unblockedRole,true);
	}
}
/********************************************************************/
//Class MPISimulator impl end									****
/********************************************************************/