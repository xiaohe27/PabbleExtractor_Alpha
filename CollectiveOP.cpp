#include "Comm.h"
using namespace llvm;
using namespace clang;
using namespace std;


void CollectiveOPManager::insertCollectiveOP(MPIOperation* op){
	if (op==nullptr)
		return;

	if (!op->isCollectiveOp())
		return;

	
	string opName=op->getOpName();
	Condition execCond=op->getExecutor();
	Condition participants=op->getTargetCond();


	//every process in every range of the execCond is an independent executor
	//for each executor, generate a corresponding record to trace the current
	//progress of the op and also the involved nodes.
	for (int i = 0; i < execCond.getRangeList().size(); i++)
	{
		Range ran=execCond.getRangeList().at(i);

		if (ran.isSpecialRange())
		{
			for (int i = 0; i <= ran.getEnd(); i++)
			{
				this->insertCollectiveOPAndCondPair(opName,i,participants,op->theNode);
			}

			for (int i = ran.getStart(); i < InitEndIndex; i++)
			{
				this->insertCollectiveOPAndCondPair(opName,i,participants,op->theNode);
			}
		} else{
			for (int i = ran.getStart(); i <= ran.getEnd(); i++)
			{
				this->insertCollectiveOPAndCondPair(opName,i,participants,op->theNode);
			}
		}
	}

}


void CollectiveOPManager::insertCollectiveOPAndCondPair(string opName, int rank, Condition cond, CommNode* node){
	opName=opName+"_"+convertIntToStr(rank);
	
	//record the involved comm node.
	if(this->participatingCommNodesMap.count(opName)){
		vector<CommNode*> *vec=this->participatingCommNodesMap.at(opName);
		bool found=false;

		for (int i = 0; i < vec->size(); i++)
		{
			if (vec->at(i)==node)
			{
				found=true;
				break;
			}
		}

		if (!found)
		{
			vec->push_back(node);
		}
	}

	else{
		vector<CommNode*> *vec=new vector<CommNode*>();
		vec->push_back(node);
		this->participatingCommNodesMap[opName]=vec;
	}

	//update participants condition
	if (this->collectiveOPFinishInfoMap.count(opName)>0)
	{
		Condition curCond=this->collectiveOPFinishInfoMap.at(opName);
		Condition repeatedCond=curCond.AND(cond);
		if (!repeatedCond.isIgnored())
		{
			throw new MPI_TypeChecking_Error
				("The processes with condition "+
				repeatedCond.printConditionInfo()+
				" perform collective operations multiple times!");
		}

		this->collectiveOPFinishInfoMap[opName]=curCond.OR(cond);
	}

	else
	{
		this->collectiveOPFinishInfoMap[opName]=cond;
	}

	//check whether the condition has reached the fire condition of the collective op
	if (this->collectiveOPFinishInfoMap.at(opName).isComplete())
	{
		this->collectiveOPFinishInfoMap.erase(opName);

		vector<CommNode*> *vec=this->participatingCommNodesMap.at(opName);
		for (unsigned int i = 0; i < vec->size(); i++)
		{
			vec->at(i)->setMarked();
		}

		this->participatingCommNodesMap.erase(opName);
	}

}