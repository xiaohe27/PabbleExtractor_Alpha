#include "MPITypeCheckingConsumer.h"

using namespace llvm;
using namespace clang;


/********************************************************************/
//Class MPIOperation impl start										****
/********************************************************************/
MPIOperation::MPIOperation(	string opName0,int opType0, string dataType0,
						   Condition executor0, Condition target0, string tag0, string group0){
							   this->opName=opName0;
							   this->opType=opType0;
							   this->dataType=dataType0;
							   this->executor=executor0.AND(Condition(true));
							   this->target=target0.AND(Condition(true));
							   this->target.execStr=target0.execStr;
							   this->tag=tag0;
							   this->group=group0;

							   this->isTargetDependOnExecutor=false;
							   this->isInPendingList=false;
							   this->isBothCastAndGather=false;

							   this->setTargetExprStr("");
							   this->theWaitNode=nullptr;
							   this->targetExpr=nullptr;

}

MPIOperation::MPIOperation(string opName0,int opType0, string dataType0,
						   Condition executor0, Expr *targetExpr0, string tag0, string group0){
							   this->opName=opName0;
							   this->opType=opType0;
							   this->dataType=dataType0;
							   this->executor=executor0.AND(Condition(true));
							   this->targetExpr=targetExpr0;
							   this->tag=tag0;
							   this->group=group0;

							   this->isTargetDependOnExecutor=true;
							   this->isInPendingList=false;
							   this->isBothCastAndGather=false;

							   this->theWaitNode=nullptr;
}

//calc the target expr 
string MPIOperation::getTarExprStr(){
	return this->targetExprStr;
}


string MPIOperation::printMPIOP(){
	string out="\n";
	out+=this->srcCode;

	out+=":\nIts op type is "+opType;

	if (this->isTargetDependOnExecutor)
	{
		if (this->getSrcCond().isIgnored())
			out+="\nIts src proc are "+this->targetExprStr;

		else
			out+="\nIts src proc are "+this->getSrcCond().printConditionInfo();

		if (this->getDestCond().isIgnored())
			out+="\nIts dest proc are "+this->targetExprStr;

		else
			out+="\nIts dest proc are "+this->getDestCond().printConditionInfo();
	}

	else{
		out+="\nIts src proc are "+this->getSrcCond().printConditionInfo();

		out+="\nIts dest proc are "+this->getDestCond().printConditionInfo();
	}

	out+="\nThe type of transmitted data is "+dataType;

	if(this->getOpName()=="MPI_Reduce" || this->getOpName()=="MPI_Allreduce")
		out+="\nThe reduce operator is "+this->operatorStr;

	out+="\nThe tag of transmitted data is "+tag;

	out+="\nThe communication happens in group "+group;

	return out;
}

bool MPIOperation::isFinished(){
	return this->executor.isIgnored();
}

bool MPIOperation::isEmptyOP(){
	return this->executor.isIgnored() || this->target.isIgnored();
}

Condition MPIOperation::getSrcCond(){
	if (this->getOPType()==ST_NODE_SEND)
	{
		return this->executor;
	}


	if (this->getOPType()==ST_NODE_RECV)
	{
		return this->getTargetCond();
	}

	return Condition(false);
}


Condition MPIOperation::getDestCond(){
	if (this->getOPType()==ST_NODE_SEND)
	{
		return this->getTargetCond();
	}


	if (this->getOPType()==ST_NODE_RECV)
	{
		return this->executor;
	}

	return Condition(false);
}

Condition MPIOperation::getTargetCond(){

	return this->target;

}

Condition MPIOperation::getExecutor(){

	return this->executor;

}

//////////////////////////////////////////////////////////////////////////////////////////////////
//ToDo
string tmp[]= {"MPI_Recv","MPI_Ssend"};
set<string> MPIOperation::blockingOPSet(begin(tmp),end(tmp));

bool MPIOperation::isOpBlocking(string opStr){

	set<string>::iterator it;
	it=MPIOperation::blockingOPSet.find(opStr);
	if (it!=blockingOPSet.end())
	{
		return true;
	}

	else
		return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////
bool MPIOperation::isBlockingOP(){
	if (this->isCollectiveOp())
		return true;

	return MPIOperation::isOpBlocking(this->opName);
}


string tmp1[]= {"MPI_Send","MPI_Ssend","MPI_Rsend","MPI_Isend"};
set<string> MPIOperation::sendingOPSet(begin(tmp1),end(tmp1));

string tmp2[]={"MPI_Recv","MPI_Irecv"};
set<string> MPIOperation::recvingOPSet(begin(tmp2),end(tmp2));

string tmp3[]={"MPI_Bcast","MPI_Gather","MPI_Reduce","MPI_Scatter","MPI_Barrier"
	,"MPI_Allgather","MPI_Allreduce"};
set<string> MPIOperation::collectiveOPSet(begin(tmp3),end(tmp3));

bool MPIOperation::isSendingOp(){

	if (this->getOPType()==ST_NODE_SEND)
		return true;


	else
		return false;
}


bool MPIOperation::isRecvingOp(){
	if (this->getOPType()==ST_NODE_RECV)	
		return true;


	else
		return false;
}


bool MPIOperation::isCollectiveOp(){
	set<string>::iterator it;
	it=MPIOperation::collectiveOPSet.find(this->getOpName());
	if (it!=collectiveOPSet.end())
	{
		return true;
	}

	else
		return false;
}


bool MPIOperation::isStable(){
	if(!this->executor.isVolatile && !this->target.isVolatile)
		return true;

	if(this->executor.isVolatile && this->target.isVolatile)
		return true;

	return false;
}

bool MPIOperation::isDependentOnExecutor(){
	return this->isTargetDependOnExecutor;
}


//test whether this op is a complementary op of the other op.
bool MPIOperation::isComplementaryOpOf(MPIOperation *otherOP){
	//if the two ops are not in the same branch, then they are not 
	//possible to meet...
	if (this->theNode->getBranID() != otherOP->theNode->getBranID())
	{
		return false;
	}

	//if the data type is not the same, then it is type mismatch
	if(STRICT && this->getDataType() != otherOP->getDataType())
		return false;


	if (this->isCollectiveOp() && otherOP->isCollectiveOp())
	{
		if(this->getOpName()=="MPI_Reduce" || this->getOpName()=="MPI_Allreduce")
		{
			if(this->operatorStr != otherOP->operatorStr)
				return false;
		}

		//for the collective ops, the op name and executor must be exactly the same 
		//in order to fire the operation...........................................
		return this->getOpName()==otherOP->getOpName() && 
			this->executor.isSameAsCond(otherOP->executor);

	}

	if(this->opType == otherOP->opType)
		return false;

	if (this->isUnicast() && !otherOP->isUnicast() && (otherOP->isMulticast() || otherOP->isGatherOp()))
	{
		return MPIOperation::areTheseTwoOpsCompatible(this,otherOP);
	}

	if (otherOP->isUnicast() && !this->isUnicast() && (this->isMulticast() || this->isGatherOp()))
	{
		return MPIOperation::areTheseTwoOpsCompatible(this,otherOP);
	}


	if (this->isRecvingOp())
	{
		return otherOP->isSendingOp();
	}

	if (this->isSendingOp())
	{
		return otherOP->isRecvingOp();
	}


	return false;
}

bool MPIOperation::isSameKindOfOp(MPIOperation *other){
	bool ThisIsCollective=this->isCollectiveOp();
	bool OtherIsCollective=other->isCollectiveOp();

	bool isDiff= (ThisIsCollective || OtherIsCollective) && !(ThisIsCollective && OtherIsCollective);
	if (isDiff)
		return false;


	bool ThisIsUniCast=this->isUnicast();
	bool OtherIsUniCast=other->isUnicast();

	isDiff= (ThisIsUniCast || OtherIsUniCast) && !(ThisIsUniCast && OtherIsUniCast);
	if (isDiff)
		return false;

	bool ThisIsMultiCast=this->isMulticast();
	bool OtherIsMultiCast=other->isMulticast();
	isDiff= (ThisIsMultiCast || OtherIsMultiCast) && !(ThisIsMultiCast && OtherIsMultiCast);
	if (isDiff)
		return false;


	bool ThisIsGather=this->isGatherOp();
	bool OtherIsGather=other->isGatherOp();
	isDiff= (ThisIsGather || OtherIsGather) && !(ThisIsGather && OtherIsGather);
	if (isDiff)
		return false;



	return true;
}


bool MPIOperation::isTheSameMPIOP(MPIOperation *other){

	return this->getOpName()==other->getOpName();

}


bool MPIOperation::isUnicast(){
	if (this->isBothCastAndGather)
		return false;

	if(this->isDependentOnExecutor())
		return true;

	return this->executor.size()==this->target.size();
}

bool MPIOperation::isMulticast(){
	if (this->isBothCastAndGather)
		return true;

	if (this->getSrcCond().size()==1 && this->getDestCond().size()>=1)
		return true;

	return false;
} 

bool MPIOperation::isGatherOp(){
	if (this->isBothCastAndGather)
		return true;

	if (this->getSrcCond().size()>=1 && this->getDestCond().size()==1)
		return true;


	return false;
}


bool MPIOperation::isNonBlockingOPWithReqVar(string reqName){
	if(this->reqVarName.size()==0)
		return false;

	return this->reqVarName==reqName;
}

//compare whether a unicast op is compatible with a multicast or gather op.
bool MPIOperation::areTheseTwoOpsCompatible(MPIOperation* op1, MPIOperation* op2){
	if (op1->isUnicast() && op2->isUnicast() || 
		!op1->isUnicast() && !op2->isUnicast())
		return op1->isComplementaryOpOf(op2);

	MPIOperation *unicast=op1->isUnicast()?op1:op2;
	MPIOperation *nonUnicast=op1->isUnicast()?op2:op1;

	int offsetFromExecToTar=Condition::getDistBetweenTwoCond(unicast->getExecutor(),unicast->getTargetCond());
	Condition execOfNonU=nonUnicast->getExecutor().addANumber(-offsetFromExecToTar);

	if (execOfNonU.AND(nonUnicast->getTargetCond()).isIgnored())
		return false;

	else
		return true;
}
/********************************************************************/
//Class MPIOperation impl end										****
/********************************************************************/


