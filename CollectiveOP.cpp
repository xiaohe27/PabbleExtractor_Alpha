#include "Comm.h"
using namespace llvm;
using namespace clang;


CollectiveOPManager::CollectiveOPManager(){
this->curCollectiveOP=nullptr;
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

		this->unlockCollectiveOP();

		return actuallyHappenedOP;
	}

	else{
		return nullptr;
	}
}


void CollectiveOPManager::unlockCollectiveOP(){
	/*
	string outToFile="\n\n\nThe actually happened MPI OP is :\n";
		outToFile.append(this->curCollectiveOP->srcCode);
		writeToFile(outToFile);
		cout<<outToFile<<endl;
	*/

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