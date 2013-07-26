#include "Comm.h"

using namespace llvm;
using namespace clang;
using namespace std;




/********************************************************************/
//Class MPIOperation impl start										****
/********************************************************************/
MPIOperation::MPIOperation(int opType0, string dataType0,Condition executor0, Condition target0, string tag0, string group0){
	this->opType=opType0;
	this->dataType=dataType0;
	this->executor=executor0;
	this->target=target0;
	this->tag=tag0;
	this->group=group0;

	this->isTargetDependOnExecutor=false;
}

MPIOperation::MPIOperation(int opType0, string dataType0,Condition executor0, Expr *targetExpr0, string tag0, string group0){
	this->opType=opType0;
	this->dataType=dataType0;
	this->executor=executor0;
	this->targetExpr=targetExpr0;
	this->tag=tag0;
	this->group=group0;

	this->isTargetDependOnExecutor=true;

}

void MPIOperation::printMPIOP(){
	cout<<this->srcCode<<endl;

	cout<<"Its op type is "<<opType<<endl;

	cout<<"Its src proc are "<<this->getSrcCond().printConditionInfo()<<endl;

	cout<<"Its dest proc are "<<this->getDestCond().printConditionInfo()<<endl;

	cout<<"The type of transmitted data is "<<dataType<<endl;

	cout<<"The tag of transmitted data is "<<tag<<endl;

	cout<<"The communication happens in group "<<group<<endl;
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

}

Condition MPIOperation::getTargetCond(){
	if (this->isTargetDependOnExecutor)
	{
		return this->analyzeTargetCondFromExecutorCond(this->executor,this->targetExpr);
	}

	else{
		return this->target;
	}
}


Condition MPIOperation::analyzeTargetCondFromExecutorCond(Condition execCond, Expr *tarExpr){
	//TODO



	return Condition(false);
}
/********************************************************************/
//Class MPIOperation impl end										****
/********************************************************************/
