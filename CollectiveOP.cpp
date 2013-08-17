#include "Comm.h"
using namespace llvm;
using namespace clang;


vector<MPIOperation*> CollectiveOPManager::insertCollectiveOP(MPIOperation* op){
	vector<MPIOperation*> vecOfMPIOPs;

	if (op==nullptr)
		return vecOfMPIOPs;

	if (!op->isCollectiveOp())
		return vecOfMPIOPs;


	string opName=op->getOpName();
	Condition execCond=op->getExecutor();
	Condition participants=op->getTargetCond();
	CommNode *node=op->theNode;

	if(execCond.outsideOfBound())
		throw new MPI_TypeChecking_Error("Executor condition for the op "+
		op->printMPIOP()+" is "+execCond.printConditionInfo()+"\nRank outside of bound.");

	//record the comm node and its initial executor cond
	if(this->commNodeAndInitExecutorCondMap.count(node->getPosIndex()))
	this->commNodeAndInitExecutorCondMap[node->getPosIndex()]=
	participants.OR(this->commNodeAndInitExecutorCondMap[node->getPosIndex()]);

	else
		this->commNodeAndInitExecutorCondMap[node->getPosIndex()]=participants;

	//the key is the op name and the val is the vec of participants
	string theOPName=opName+":"+node->getBranID()+execCond.printConditionInfo();
	//record the involved comm node.
	if(this->participatingCommNodesMap.count(theOPName)){
		vector<CommNode*> *vec=this->participatingCommNodesMap.at(theOPName);
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
		this->participatingCommNodesMap[theOPName]=vec;
		this->remainingTimesNeedToDoTheOP[theOPName]=execCond.size();
	}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

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
				MPIOperation* tmpOP=this->insertCollectiveOPAndCondPair(opName,i,participants,op);
				if (tmpOP){
					vecOfMPIOPs.push_back(tmpOP);
					this->dec(theOPName);
				}
			}

			for (int i = ran.getStart(); i < InitEndIndex; i++)
			{
				MPIOperation* tmpOP=this->insertCollectiveOPAndCondPair(opName,i,participants,op);
				if (tmpOP){
					vecOfMPIOPs.push_back(tmpOP);
					this->dec(theOPName);
				}
			}
		} else{
			for (int i = ran.getStart(); i <= ran.getEnd(); i++)
			{
				MPIOperation* tmpOP=this->insertCollectiveOPAndCondPair(opName,i,participants,op);
				if (tmpOP){
					vecOfMPIOPs.push_back(tmpOP);
					this->dec(theOPName);
				}
			}
		}
	}

	return vecOfMPIOPs;
}

void CollectiveOPManager::dec(string opNameKey){
	if (this->remainingTimesNeedToDoTheOP.count(opNameKey)>0)
	{
		this->remainingTimesNeedToDoTheOP[opNameKey]--;

		if (this->remainingTimesNeedToDoTheOP[opNameKey]<=0)
		{
			this->unlockCollectiveOP(opNameKey);
			this->remainingTimesNeedToDoTheOP.erase(opNameKey);
		}
	}

	else{
		throw new MPI_TypeChecking_Error("Unrecognized operation "+opNameKey);
	}
}

MPIOperation* CollectiveOPManager::insertCollectiveOPAndCondPair(string opName, int rank, Condition cond, MPIOperation* op){
	CommNode *node=op->theNode;
	string opNameKey=opName+"_"+convertIntToStr(rank)+"("+node->getBranID()+")";

	//update participants condition
	if (this->collectiveOPFinishInfoMap.count(opNameKey)>0)
	{
		Condition curCond=this->collectiveOPFinishInfoMap.at(opNameKey);
		Condition repeatedCond=curCond.AND(cond);
		if (!repeatedCond.isIgnored())
		{
			throw new MPI_TypeChecking_Error
				("The processes with condition "+
				repeatedCond.printConditionInfo()+
				" perform collective operations multiple times!");
		}

		this->collectiveOPFinishInfoMap[opNameKey]=curCond.OR(cond);
	}

	else
	{
		this->collectiveOPFinishInfoMap[opNameKey]=cond;
	}

	//check whether the condition has reached the fire condition of the collective op
	if (this->collectiveOPFinishInfoMap.at(opNameKey).isComplete())
	{	
		string outToFile="\n\n\nThe actually happened MPI OP is :\n";
		outToFile.append("Process "+convertIntToStr(rank)+" is doing an "+opName+" operation");
		writeToFile(outToFile);
		cout<<outToFile<<endl;

		/////////////////////////////////////////////////////////////////////////////////////////////

		MPIOperation *tmpOP=new MPIOperation(*op);
		tmpOP->setExecutorCond(Condition(Range(rank,rank)));
		tmpOP->setTargetCond(Condition(true));
		
		/////////////////////////////////////////////////////////////////////////////////////////////

		this->collectiveOPFinishInfoMap.erase(opNameKey);

		return tmpOP;
	}

	else{
		return nullptr;
	}

}



void CollectiveOPManager::unlockCollectiveOP(string opNameKey){
	vector<CommNode*> *vec=this->participatingCommNodesMap.at(opNameKey);
		for (unsigned int i = 0; i < vec->size(); i++)
		{
			int posI=vec->at(i)->getPosIndex();
		
			Condition unlockCond=this->commNodeAndInitExecutorCondMap[posI];
			//inform the original node that the executors with these condition have finished their tasks
			vec->at(i)->reportFinished(unlockCond);
		}

		this->participatingCommNodesMap.erase(opNameKey);
	}