#include "MPITypeCheckingConsumer.h"

using namespace llvm;
using namespace clang;


/*******************************visitCallExpr******************************************************/
bool MPITypeCheckingConsumer::VisitCallExpr(CallExpr *E){

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
			if(this->setOfWorldCommGroup.find(args[0])==this->setOfWorldCommGroup.end())
				throw new MPI_TypeChecking_Error
				("Current program does not support communicators other than MPI_COMM_WORLD.");

			this->numOfProcessesVar=args[1].substr(1,args[1].length()-1);
			this->listOfNumOfProcVars.insert(this->numOfProcessesVar);
			VarCondMap[this->numOfProcessesVar]=Condition(Range(N,N));

			cout<<"Var storing num of Processes is "<<this->numOfProcessesVar<<endl;
		}

		if(funcName=="MPI_Comm_rank"){
			string commGroup=args[0];

			if(this->setOfWorldCommGroup.find(commGroup)==this->setOfWorldCommGroup.end())
				throw new MPI_TypeChecking_Error
				("Current program does not support communicators other than MPI_COMM_WORLD.");

			RANKVAR=args[1].substr(1,args[1].length()-1);

			VarCondMap[RANKVAR]=Condition(true);

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

			if(this->setOfWorldCommGroup.find(group)==this->setOfWorldCommGroup.end())
				throw new MPI_TypeChecking_Error
				("Current program does not support communicators other than MPI_COMM_WORLD.");

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
			mpiOP->execExprStr=root;
			CommNode *bcastNode=new CommNode(mpiOP);

			this->mpiSimulator->insertNode(bcastNode);

		}



		if(funcName=="MPI_Reduce"){

			string dataType=args[3];
			string operatorStr=args[4];
			string root=args[5];
			string group=args[6];

			if(this->setOfWorldCommGroup.find(group)==this->setOfWorldCommGroup.end())
				throw new MPI_TypeChecking_Error
				("Current program does not support communicators other than MPI_COMM_WORLD.");

			MPIOperation *mpiOP;

			if (this->commManager->containsRankStr(root))
			{
				throw new MPI_TypeChecking_Error("The root of bcast should NOT contain rank var!");
			}

			else{
				Condition reducer=this->commManager->extractCondFromTargetExpr(E->getArg(5));

				mpiOP=new MPIOperation(	funcName,
					ST_NODE_RECV, 
					dataType,
					reducer, //the unique performer
					Condition(true), //the participants of the collective op.
					"", 
					group);

			}

			mpiOP->setSrcCode(funcSrcCode);
			mpiOP->execExprStr=root;
			mpiOP->operatorStr=operatorStr;
			CommNode *reduceNode=new CommNode(mpiOP);

			this->mpiSimulator->insertNode(reduceNode);

		}


		///////////////////////////////////////////////////////////////
		if(funcName=="MPI_Gather"){

			string sendDataType=args[2];
			string recvDataType=args[5];
			string root=args[6];
			string group=args[7];

			if(this->setOfWorldCommGroup.find(group)==this->setOfWorldCommGroup.end())
				throw new MPI_TypeChecking_Error
				("Current program does not support communicators other than MPI_COMM_WORLD.");

			MPIOperation *mpiOP;

			if (this->commManager->containsRankStr(root))
			{
				throw new MPI_TypeChecking_Error("The root of bcast should NOT contain rank var!");
			}

			else{
				Condition gather=this->commManager->extractCondFromTargetExpr(E->getArg(6));

				mpiOP=new MPIOperation(	funcName,
					ST_NODE_RECV, 
					recvDataType,
					gather, //the unique performer
					Condition(true), //the participants of the collective op.
					"", 
					group);

			}

			mpiOP->setSrcCode(funcSrcCode);
			mpiOP->execExprStr=root;
			CommNode *gatherNode=new CommNode(mpiOP);

			this->mpiSimulator->insertNode(gatherNode);

		}


		///////////////////////////////////////////////////////////////
		if(funcName=="MPI_Scatter"){

			string sendDataType=args[2];
			string recvDataType=args[5];
			string root=args[6];
			string group=args[7];

			if(this->setOfWorldCommGroup.find(group)==this->setOfWorldCommGroup.end())
				throw new MPI_TypeChecking_Error
				("Current program does not support communicators other than MPI_COMM_WORLD.");

			MPIOperation *mpiOP;

			if (this->commManager->containsRankStr(root))
			{
				throw new MPI_TypeChecking_Error("The root of bcast should NOT contain rank var!");
			}

			else{
				Condition scatter=this->commManager->extractCondFromTargetExpr(E->getArg(6));

				mpiOP=new MPIOperation(	funcName,
					ST_NODE_SEND, 
					sendDataType,
					scatter, //the unique performer
					Condition(true), //the participants of the collective op.
					"", 
					group);

			}

			mpiOP->setSrcCode(funcSrcCode);
			mpiOP->execExprStr=root;
			CommNode *gatherNode=new CommNode(mpiOP);

			this->mpiSimulator->insertNode(gatherNode);

		}

		//////////////////////////////////////////////////////////////////

		if(funcName=="MPI_Allreduce"){

			string dataType=args[3];
			string operatorStr=args[4];
			string group=args[5];

			if(this->setOfWorldCommGroup.find(group)==this->setOfWorldCommGroup.end())
				throw new MPI_TypeChecking_Error
				("Current program does not support communicators other than MPI_COMM_WORLD.");

			MPIOperation *mpiOP;

			Condition reducer(true);

			mpiOP=new MPIOperation(	funcName,
				ST_NODE_RECV, 
				dataType,
				reducer, //the unique performer
				Condition(true), //the participants of the collective op.
				"", 
				group);

			mpiOP->setSrcCode(funcSrcCode);
			mpiOP->execExprStr="0..N-1";
			mpiOP->operatorStr=operatorStr;
			CommNode *reduceNode=new CommNode(mpiOP);
			this->mpiSimulator->insertNode(reduceNode);

		}


		//////////////////////////////////////////////////////////////////

		if(funcName=="MPI_Allgather"){

			string sendDataType=args[2];
			string recvDataType=args[5];			
			string group=args[6];

			if(this->setOfWorldCommGroup.find(group)==this->setOfWorldCommGroup.end())
				throw new MPI_TypeChecking_Error
				("Current program does not support communicators other than MPI_COMM_WORLD.");

			MPIOperation *mpiOP;

			Condition gather(true);

			mpiOP=new MPIOperation(	funcName,
				ST_NODE_RECV, 
				recvDataType,
				gather, //the unique performer
				Condition(true), //the participants of the collective op.
				"", 
				group);



			mpiOP->setSrcCode(funcSrcCode);
			mpiOP->execExprStr="0..N-1";
			CommNode *gatherNode=new CommNode(mpiOP);
			this->mpiSimulator->insertNode(gatherNode);
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

		if(funcName=="MPI_Barrier"){
			BarrierNode *barNode=new BarrierNode();

			if(this->setOfWorldCommGroup.find(args[0])==this->setOfWorldCommGroup.end())
				throw new MPI_TypeChecking_Error
				("Current program does not support communicators other than MPI_COMM_WORLD.");

			this->mpiSimulator->insertNode(barNode);
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

	Condition execCond=this->mpiSimulator->getCurExecCond();

	if(this->setOfWorldCommGroup.find(group)==this->setOfWorldCommGroup.end())
		throw new MPI_TypeChecking_Error
		("Current program does not support communicators other than MPI_COMM_WORLD.");

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
		Condition destCond=this->commManager->extractCondFromTargetExpr(E->getArg(3));

		mpiOP=new MPIOperation(		funcName,
			ST_NODE_SEND, 
			dataType,
			execCond, //the performer
			destCond, //the dest
			tag, 
			group);	

		if(destCond.outsideOfBound())
			throw new MPI_TypeChecking_Error("Destination condition for the op "+
			mpiOP->printMPIOP()+" is "+destCond.printConditionInfo()+"\nRank outside of bound.");
	}

	mpiOP->setSrcCode(funcSrcCode);

	mpiOP->execExprStr=execCond.execStr;

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

	Condition execCond=this->mpiSimulator->getCurExecCond();

	if(this->setOfWorldCommGroup.find(group)==this->setOfWorldCommGroup.end())
		throw new MPI_TypeChecking_Error
		("Current program does not support communicators other than MPI_COMM_WORLD.");

	MPIOperation *mpiOP;

	if (this->commManager->containsRankStr(src)){
		mpiOP=new MPIOperation(funcName,ST_NODE_RECV, dataType,							
			this->mpiSimulator->getCurExecCond(), 
			E->getArg(3),
			tag, group);

		mpiOP->setTargetExprStr(stmt2str(&ci->getSourceManager(),ci->getLangOpts(),E->getArg(3)));
	}

	else{

		Condition tarCond=this->commManager->extractCondFromTargetExpr(E->getArg(3));

		mpiOP=new MPIOperation(funcName,ST_NODE_RECV, dataType,								
			execCond, 
			tarCond,
			tag, group);

		if(tarCond.outsideOfBound())
			throw new MPI_TypeChecking_Error("Src condition for the op "+
			mpiOP->printMPIOP()+" is "+tarCond.printConditionInfo()+"\nRank outside of bound.");
	}



	mpiOP->setSrcCode(funcSrcCode);
	mpiOP->setTargetExprStr(src);
	mpiOP->execExprStr=execCond.execStr;

	if(funcName=="MPI_Irecv")
		mpiOP->reqVarName=args[6];

	CommNode *recvNode=new CommNode(mpiOP);

	this->mpiSimulator->insertNode(recvNode);


	cout <<"\n\n\n\n\nThe src of mpi recv is"<<
		mpiOP->getSrcCond().printConditionInfo()<<"\n\n\n\n\n" <<endl;

}