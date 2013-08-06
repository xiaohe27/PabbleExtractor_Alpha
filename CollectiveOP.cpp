#include "Comm.h"
using namespace llvm;
using namespace clang;
using namespace std;


vector<MPIOperation*> CollectiveOPManager::insertCollectiveOP(MPIOperation* op){
	vector<MPIOperation*> vecOfMPIOPs;

	if (op==nullptr)
		return vecOfMPIOPs;

	if (!op->isCollectiveOp())
		return vecOfMPIOPs;


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
				MPIOperation* tmpOP=this->insertCollectiveOPAndCondPair(opName,i,participants,op->theNode);
				if (tmpOP)
					vecOfMPIOPs.push_back(tmpOP);
			}

			for (int i = ran.getStart(); i < InitEndIndex; i++)
			{
				MPIOperation* tmpOP=this->insertCollectiveOPAndCondPair(opName,i,participants,op->theNode);
				if (tmpOP)
					vecOfMPIOPs.push_back(tmpOP);
			}
		} else{
			for (int i = ran.getStart(); i <= ran.getEnd(); i++)
			{
				MPIOperation* tmpOP=this->insertCollectiveOPAndCondPair(opName,i,participants,op->theNode);
				if (tmpOP)
					vecOfMPIOPs.push_back(tmpOP);
			}
		}
	}

	return vecOfMPIOPs;
}


MPIOperation* CollectiveOPManager::insertCollectiveOPAndCondPair(string opName, int rank, Condition cond, CommNode* node){
	string opNameKey=opName+"_"+convertIntToStr(rank)+"("+node->getBranID()+")";

	//record the involved comm node.
	if(this->participatingCommNodesMap.count(opNameKey)){
		vector<CommNode*> *vec=this->participatingCommNodesMap.at(opNameKey);
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
		this->participatingCommNodesMap[opNameKey]=vec;
	}

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

		MPIOperation *tmpOP=new MPIOperation(*(node->getOPs()->back()));
		tmpOP->setExecutorCond(Condition(Range(rank,rank)));
		tmpOP->setTargetCond(Condition(true));
		
		/////////////////////////////////////////////////////////////////////////////////////////////

		this->collectiveOPFinishInfoMap.erase(opNameKey);

		vector<CommNode*> *vec=this->participatingCommNodesMap.at(opNameKey);
		for (unsigned int i = 0; i < vec->size(); i++)
		{
			vec->at(i)->setMarked();
		}

		this->participatingCommNodesMap.erase(opNameKey);

		return tmpOP;
	}

	else{
		return nullptr;
	}

}