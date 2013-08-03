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
}

void MPIOperation::printMPIOP(){
	cout<<this->srcCode<<endl;

	cout<<"Its op type is "<<opType<<endl;

	if (this->isTargetDependOnExecutor)
	{
		if (this->getSrcCond().isIgnored())
			cout<<"Its src proc are "<<targetExprStr<<endl;

		else
			cout<<"Its src proc are "<<this->getSrcCond().printConditionInfo()<<endl;

		if (this->getDestCond().isIgnored())
			cout<<"Its dest proc are "<<targetExprStr<<endl;

		else
			cout<<"Its dest proc are "<<this->getDestCond().printConditionInfo()<<endl;
	}

	else{
		cout<<"Its src proc are "<<this->getSrcCond().printConditionInfo()<<endl;

		cout<<"Its dest proc are "<<this->getDestCond().printConditionInfo()<<endl;
	}

	cout<<"The type of transmitted data is "<<dataType<<endl;

	cout<<"The tag of transmitted data is "<<tag<<endl;

	cout<<"The communication happens in group "<<group<<endl;
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
	set<string>::iterator it;
	it=MPIOperation::sendingOPSet.find(this->getOpName());
	if (it!=sendingOPSet.end())
	{
		return true;
	}

	else
		return false;
}


bool MPIOperation::isRecvingOp(){
	set<string>::iterator it;
	it=MPIOperation::recvingOPSet.find(this->getOpName());
	if (it!=recvingOPSet.end())
	{
		return true;
	}

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


bool MPIOperation::isUnicast(){
	if(this->isDependentOnExecutor())
		return true;

	return this->executor.size()==this->target.size();
}

bool MPIOperation::isMulticast(){
	if (this->getSrcCond().size()==1 && this->getDestCond().size()>1)
	{
		return true;
	}

	return false;
} 

bool MPIOperation::isGatherOp(){
	if (this->getSrcCond().size()>1 && this->getDestCond().size()==1)
	{
		return true;
	}

	return false;
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

			this->rankVar=args[1].substr(1,args[1].length()-1);

			this->commManager->insertRankVarAndOffset(rankVar,0);

			this->commManager->insertRankVarAndCommGroupMapping(this->rankVar,commGroup);

			cout<<"Rank var is "<<this->rankVar <<".\t AND it is associated with group "<<commGroup<<endl;
		}

		if(funcName=="MPI_Send"){

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
					this->commManager->getTopCondition(), //the performer
					E->getArg(3), //the dest
					tag, 
					group);

				mpiOP->setTargetExprStr(stmt2str(&ci->getSourceManager(),ci->getLangOpts(),E->getArg(3)));
			}

			else{

				mpiOP=new MPIOperation(		funcName,
					ST_NODE_SEND, 
					dataType,
					this->commManager->getTopCondition(), //the performer
					this->commManager->extractCondFromTargetExpr(E->getArg(3)), //the dest
					tag, 
					group);

			}

			mpiOP->setSrcCode(funcSrcCode);

			CommNode *sendNode=new CommNode(mpiOP);

			this->mpiSimulator->insertNode(sendNode);


			cout <<"\n\n\n\n\nThe dest of mpi send is"
				<< mpiOP->getDestCond().printConditionInfo()<<"\n\n\n\n\n" <<endl;

		}

		if(funcName=="MPI_Recv"){

			string dataType=args[2];
			string src=args[3];
			string tag=args[4];
			string group=args[5];

			MPIOperation *mpiOP;

			if (this->commManager->containsRankStr(src)){
				mpiOP=new MPIOperation(funcName,ST_NODE_RECV, dataType,							
					this->commManager->getTopCondition(), 
					E->getArg(3),
					tag, group);

				mpiOP->setTargetExprStr(stmt2str(&ci->getSourceManager(),ci->getLangOpts(),E->getArg(3)));
			}

			else{
				mpiOP=new MPIOperation(funcName,ST_NODE_RECV, dataType,								
					this->commManager->getTopCondition(), 
					this->commManager->extractCondFromTargetExpr(E->getArg(3)),
					tag, group);
			}



			mpiOP->setSrcCode(funcSrcCode);
			CommNode *recvNode=new CommNode(mpiOP);

			this->mpiSimulator->insertNode(recvNode);


			cout <<"\n\n\n\n\nThe src of mpi recv is"<<
				mpiOP->getSrcCond().printConditionInfo()<<"\n\n\n\n\n" <<endl;

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
				Condition bcaster=this->commManager->extractCondFromTargetExpr(E->getArg(3));


				mpiOP=new MPIOperation(	funcName,
					ST_NODE_SEND, 
					dataType,
					bcaster, //the unique performer
					Condition(true), //the participants of the collective op.
					"", 
					group);

			}

			mpiOP->setSrcCode(funcSrcCode);

			CommNode *bcastNode=new CommNode(mpiOP);

			this->mpiSimulator->insertNode(bcastNode);


			cout <<"\n\n\n\n\nThe dest of mpi bcast is"
				<< mpiOP->getDestCond().printConditionInfo()<<"\n\n\n\n\n" <<endl;

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