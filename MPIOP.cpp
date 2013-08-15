#include "MPITypeCheckingConsumer.h"

using namespace llvm;
using namespace clang;
using namespace std;




/********************************************************************/
//Class MPIOperation impl start										****
/********************************************************************/
MPIOperation::MPIOperation(string opName0,int opType0, string dataType0,Condition executor0, Condition target0, string tag0, string group0){
	this->opName=opName0;
	this->opType=opType0;
	this->dataType=dataType0;
	this->executor=executor0;
	this->target=target0;
	this->tag=tag0;
	this->group=group0;

	this->isTargetDependOnExecutor=false;
	this->isInPendingList=false;
	this->isBothCastAndGather=false;

	this->setTargetExprStr("");
	this->theWaitNode=nullptr;

}

MPIOperation::MPIOperation(string opName0,int opType0, string dataType0,Condition executor0, Expr *targetExpr0, string tag0, string group0){
	this->opName=opName0;
	this->opType=opType0;
	this->dataType=dataType0;
	this->executor=executor0;
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
	,"MPI_Allgather","MPI_allreduce"};
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

//test whether this op is a complementary op of the other op.
bool MPIOperation::isComplementaryOpOf(MPIOperation *otherOP){
	//if the two ops are not in the same branch, then they are not 
	//possible to meet...
	if (this->theNode->getBranID() != otherOP->theNode->getBranID())
	{
		return false;
	}


	if (this->isCollectiveOp() && otherOP->isCollectiveOp())
	{
		//for the collective ops, the op name and executor must be exactly the same 
		//in order to fire the operation...........................................
		return this->getOpName()==otherOP->getOpName() && 
			this->executor.isSameAsCond(otherOP->executor);
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
/********************************************************************/
//Class MPIOperation impl end										****
/********************************************************************/





/*******************************visitCallExpr******************************************************/
bool MPITypeCheckingConsumer::VisitCallExpr(CallExpr *E){
	//TODO

	//if we haven't started to visit the main function, then do nothing.
	if(!this->visitStart)
		return true;

	string funcSrcCode=stmt2str(&ci->getSourceManager(),ci->getLangOpts(),E);

	//	cout <<"The function going to be called is "<< funcSrcCode<<endl;


	int numOfArgs=E->getNumArgs();


	vector<string> args(numOfArgs+1);


	//get all the args of the function call
	for(int i=0; i<numOfArgs; i++)
	{		
		args[i]=expr2str(&ci->getSourceManager(),ci->getLangOpts(),E->getArg(i));
	}


	//	Decl *decl=E->getCalleeDecl();


	FunctionDecl *funcCall=E->getDirectCallee();
	//perform a check. If the decl has been visited before, then throw err info
	//else traverse the decl.
	if (funcCall)
	{
		string funcName=funcCall->getQualifiedNameAsString();

		/////////////////////////////////////////////////////////////////////////////////////
		/*******************Enum the possible MPI OPs**************************************/

		//get the var storing number of processes
		if(funcName=="MPI_Comm_size"){
			this->numOfProcessesVar=args[1].substr(1,args[1].length()-1);

			cout<<"Var storing num of Processes is "<<this->numOfProcessesVar<<endl;
		}

		if(funcName=="MPI_Comm_rank"){
			string commGroup=args[0];

			RANKVAR=args[1].substr(1,args[1].length()-1);

			this->commManager->insertRankVarAndOffset(RANKVAR,0);

			this->commManager->insertRankVarAndCommGroupMapping(RANKVAR,commGroup);

			cout<<"Rank var is "<<RANKVAR <<".\t AND it is associated with group "<<commGroup<<endl;
		}

		if(funcName=="MPI_Send" || funcName=="MPI_Ssend" || funcName=="MPI_Isend"){
			this->genSendingOP(args,E,funcName,funcSrcCode);
		}

		if(funcName=="MPI_Recv" || funcName=="MPI_Irecv"){
			this->genRecvingOP(args,E,funcName,funcSrcCode);
		}


		if(funcName=="MPI_Bcast"){

			string dataType=args[2];
			string root=args[3];
			string group=args[4];

			MPIOperation *mpiOP;

			if (this->commManager->containsRankStr(root))
			{
				throw new MPI_TypeChecking_Error("The root of bcast should NOT contain rank var!");
			}

			else{
				Condition bcaster=this->commManager->extractCondFromTargetExpr(E->getArg(3),
					this->mpiSimulator->getCurExecCond());

				mpiOP=new MPIOperation(	funcName,
					ST_NODE_SEND, 
					dataType,
					bcaster, //the unique performer
					Condition(true), //the participants of the collective op.
					"", 
					group);

			}

			mpiOP->setSrcCode(funcSrcCode);
			mpiOP->execExprStr=root;
			CommNode *bcastNode=new CommNode(mpiOP);

			this->mpiSimulator->insertNode(bcastNode);


			cout <<"\n\n\n\n\nThe dest of mpi bcast is"
				<< mpiOP->getDestCond().printConditionInfo()<<"\n\n\n\n\n" <<endl;

		}

		/////////////////////////////////////////////////////////////////////////////////////////////
		if (funcName=="MPI_Wait" || funcName=="MPI_Waitall" || funcName=="MPI_Waitany")
		{
			WaitNode *waitNode;
			if(funcName=="MPI_Wait")
				waitNode=new WaitNode(args[0]);
			
			if (funcName=="MPI_Waitall")
				waitNode==new WaitNode(WaitNode::waitAll);

			if (funcName=="MPI_Waitany")
				waitNode=new WaitNode();

			this->mpiSimulator->insertNode(waitNode);
		}



		/*******************Enum the possible MPI OPs end***********************************/
		/////////////////////////////////////////////////////////////////////
		if(funcCall->hasBody())
			this->analyzeDecl(funcCall);
	}

	else{
		this->TraverseDecl(E->getCalleeDecl());
	}


	args.clear();
	return true;
}











void MPITypeCheckingConsumer::genSendingOP(vector<string> args,CallExpr *E,string funcName,string funcSrcCode){
	string dataType=args[2];
	string dest=args[3];
	string tag=args[4];
	string group=args[5];

	MPIOperation *mpiOP;

	if (this->commManager->containsRankStr(dest))
	{
		//if the target is related to the executor of the op,
		//then the change of executor will affect the target condition
		//so use the target expr instead of target condition in constructor
		mpiOP=new MPIOperation(			funcName,
			ST_NODE_SEND, 
			dataType,
			this->mpiSimulator->getCurExecCond(), //the performer
			E->getArg(3), //the dest
			tag, 
			group);

		mpiOP->setTargetExprStr(stmt2str(&ci->getSourceManager(),ci->getLangOpts(),E->getArg(3)));

	}

	else{
		Condition execCond=this->mpiSimulator->getCurExecCond();

		mpiOP=new MPIOperation(		funcName,
			ST_NODE_SEND, 
			dataType,
			execCond, //the performer
			this->commManager->extractCondFromTargetExpr(E->getArg(3),execCond), //the dest
			tag, 
			group);

	}

	mpiOP->setSrcCode(funcSrcCode);

	mpiOP->setTargetExprStr(dest);

	if(funcName=="MPI_Isend")
		mpiOP->reqVarName=args[6];

	CommNode *sendNode=new CommNode(mpiOP);

	this->mpiSimulator->insertNode(sendNode);


	cout <<"\n\n\n\n\nThe dest of mpi send is"
		<< mpiOP->getDestCond().printConditionInfo()<<"\n\n\n\n\n" <<endl;
}


void MPITypeCheckingConsumer::genRecvingOP(vector<string> args,CallExpr *E,string funcName,string funcSrcCode){
	string dataType=args[2];
	string src=args[3];
	string tag=args[4];
	string group=args[5];

	MPIOperation *mpiOP;

	if (this->commManager->containsRankStr(src)){
		mpiOP=new MPIOperation(funcName,ST_NODE_RECV, dataType,							
			this->mpiSimulator->getCurExecCond(), 
			E->getArg(3),
			tag, group);

		mpiOP->setTargetExprStr(stmt2str(&ci->getSourceManager(),ci->getLangOpts(),E->getArg(3)));
	}

	else{
		Condition execCond=this->mpiSimulator->getCurExecCond();
		mpiOP=new MPIOperation(funcName,ST_NODE_RECV, dataType,								
			execCond, 
			this->commManager->extractCondFromTargetExpr(E->getArg(3),execCond),
			tag, group);
	}



	mpiOP->setSrcCode(funcSrcCode);
	mpiOP->setTargetExprStr(src);
	if(funcName=="MPI_Irecv")
		mpiOP->reqVarName=args[6];

	CommNode *recvNode=new CommNode(mpiOP);

	this->mpiSimulator->insertNode(recvNode);


	cout <<"\n\n\n\n\nThe src of mpi recv is"<<
		mpiOP->getSrcCond().printConditionInfo()<<"\n\n\n\n\n" <<endl;

}
