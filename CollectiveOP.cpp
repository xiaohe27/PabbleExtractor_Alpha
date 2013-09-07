#include "Comm.h"
using namespace llvm;
using namespace clang;


CollectiveOPManager::CollectiveOPManager(){
this->curCollectiveOP=nullptr;
}

string CollectiveOPManager::getCurPendingCollectiveOPName(){
	return this->curCollectiveOP==nullptr ? "No pending collective operation!" :
			this->curCollectiveOP->srcCode;
}

MPIOperation* CollectiveOPManager::insertCollectiveOP(MPIOperation* op){

	if (op==nullptr)
		return nullptr;


	Condition execCond=op->getExecutor();

	if(execCond.outsideOfBound())
		throw new MPI_TypeChecking_Error("Executor condition for the op "+
		op->printMPIOP()+" is "+execCond.printConditionInfo()+"\nRank outside of bound.");

	CommNode *node=op->theNode;
	Condition participants=op->getTargetCond();

	//record the comm node and its initial executor cond
	if(this->commNodeAndInitExecutorCondMap.count(node->getPosIndex()))
	this->commNodeAndInitExecutorCondMap[node->getPosIndex()]=
	participants.OR(this->commNodeAndInitExecutorCondMap[node->getPosIndex()]);

	else
		this->commNodeAndInitExecutorCondMap[node->getPosIndex()]=participants;


	if(this->curCollectiveOP==nullptr){
		this->curCollectiveOP=op;
		this->participatingCommNodesVec.push_back(node);
	}

	else{
		if(!this->curCollectiveOP->isComplementaryOpOf(op))
			throw new MPI_TypeChecking_Error("The collective op:\n"+
			this->curCollectiveOP->srcCode+"\n\nis not compatible with the op:\n\n"+
			op->srcCode+"\n\nDeadlock will happen!");

		bool isDiff=(this->curCollectiveOP->getExecutor().isVolatile || execCond.isVolatile)
			&& !(this->curCollectiveOP->getExecutor().isVolatile && execCond.isVolatile);
		
		if (isDiff)
			IsGenericProtocol=false; //we found the imperfect matching! 

		bool found=false;

		for (int i = 0; i < this->participatingCommNodesVec.size(); i++)
		{
			if (this->participatingCommNodesVec.at(i)->getPosIndex()==node->getPosIndex()){
				found=true;
				break;
			}	
		}

		if(!found)
			this->participatingCommNodesVec.push_back(node);


		Condition registeredParticipants=this->curCollectiveOP->getTargetCond();
		Condition repeatedCond=registeredParticipants.AND(op->getTargetCond());
		if(!repeatedCond.isIgnored())
			throw new MPI_TypeChecking_Error
			("The processes with condition "+
			repeatedCond.printConditionInfo()+
			" perform collective operation \n"+op->srcCode+"\nmultiple times!");

		this->curCollectiveOP->setTargetCond(registeredParticipants.OR(op->getTargetCond()));
	}

	if(this->curCollectiveOP->getTargetCond().isComplete()){
		this->curCollectiveOP->setTargetCond(Condition(true));

		MPIOperation *actuallyHappenedOP=this->curCollectiveOP;

		CommNode *initNode=this->participatingCommNodesVec.at(this->getInitNodePos());
		this->unlockCollectiveOP();

		actuallyHappenedOP->theNode=initNode;
		return actuallyHappenedOP;
	}

	else{
		return nullptr;
	}
}


void CollectiveOPManager::unlockCollectiveOP(){

	for (unsigned int i = 0; i < this->participatingCommNodesVec.size(); i++)
	{
		int posI=this->participatingCommNodesVec.at(i)->getPosIndex();

		Condition unlockCond=this->commNodeAndInitExecutorCondMap[posI];
		//inform the original node that the executors with these condition have finished their tasks
		this->participatingCommNodesVec.at(i)->reportFinished(unlockCond);
	}

	this->participatingCommNodesVec.clear();
	this->commNodeAndInitExecutorCondMap.clear();
	this->curCollectiveOP=nullptr;
}

//in order to find the node with min index among candidates. 
int CollectiveOPManager::getInitNodePos(){
	int initNodePos=0;

	int curMinNodeIndex=this->participatingCommNodesVec.at(0)->getPosIndex();
	for (int i = 1; i < this->participatingCommNodesVec.size(); i++)
	{
		if (this->participatingCommNodesVec.at(i)->getPosIndex() < curMinNodeIndex)
		{
			curMinNodeIndex=this->participatingCommNodesVec.at(i)->getPosIndex();
			initNodePos=i;
		}
	}

	return initNodePos;
}