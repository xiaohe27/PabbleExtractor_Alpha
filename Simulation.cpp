#include "Comm.h"

using namespace llvm;
using namespace clang;
using namespace std;


//////////////////////VisitResult//////////////////////////////////////////
VisitResult::VisitResult(bool b, MPIOperation *op, vector<Role> roles){
	this->isBlocking=b;
	this->doableOP=op;
	this->escapableRoles=roles;
}

void VisitResult::printVisitInfo(){
	if (this->doableOP)
	{
		cout<<"The doable MPI operation in this node is "<<endl;
		this->doableOP->printMPIOP();

		cout<<"It is "<<(this->isBlocking?"":"non-")<<"blocking operation!"<<endl;
	}


	for (int i = 0; i < escapableRoles.size(); i++)
	{
		cout<<"The role "<<escapableRoles[i].getRoleName()<<" is able to escape to the next node!"<<endl;
	}

}
/////////////////////VisitResult Ends///////////////////////////////////////



/********************************************************************/
//Class MPISimulator impl start										****
/********************************************************************/


MPISimulator::MPISimulator(CommManager *commMgr):root(ST_NODE_ROOT){
	this->commManager=commMgr;

	root.setCond(Condition(true));

	curNode=&root;

}

void MPISimulator::insertNode(CommNode *node){

	//set the condition for the node.
	node->setCond(this->commManager->getTopCondition());

	int nodeT=node->getNodeType();


	if(node->getNodeType()==ST_NODE_CONTINUE){
		CommNode *theParent=this->curNode;
		while (theParent->getNodeType()!=ST_NODE_RECUR)
		{
			theParent=theParent->getParent();
		}

		if(theParent!=nullptr){
			ContNode *contNode=(ContNode*)node;
			RecurNode *loopNode=(RecurNode*)(theParent);

			contNode->setRefNode(loopNode);
		}

	}

	//insert the node
	this->curNode->insert(node);

	if(nodeT==ST_NODE_CHOICE || nodeT==ST_NODE_RECUR
		||nodeT==ST_NODE_ROOT || nodeT==ST_NODE_PARALLEL){

			this->curNode=node;
	}

}


void MPISimulator::gotoParent(){
	this->curNode= this->curNode->getParent();
}


bool MPISimulator::isDeadLockDetected(){
	return false;
}

void MPISimulator::initTheRoles(){
	map<string,ParamRole> paramRoleMap=this->commManager->getParamRoleMapping();

	for (auto &x: paramRoleMap)
	{

		ParamRole paramRole=x.second;

		vector<Role*> roles= paramRole.getTheRoles();

		for (int i = 0; i < roles.size(); i++)
		{
			roles[i]->setCurVisitNode(&this->root);
		}
	}

}

void MPISimulator::printTheRoles(){
	map<string,ParamRole> paramRoleMap=this->commManager->getParamRoleMapping();

	for (auto &x: paramRoleMap)
	{
		string paramRoleName=x.first;
		cout<<"The param role name is "<<paramRoleName<<endl;
		cout<<"The actual roles for this param role are:\n";

		ParamRole paramRole=x.second;

		vector<Role*> roles= paramRole.getTheRoles();

		for (int i = 0; i < roles.size(); i++)
		{
			cout<<"The role "<<i<<" is "<< roles.at(i)->getRoleName()<<endl;
		}
	}

}

bool MPISimulator::areAllRolesDone(){
	map<string,ParamRole> paramRoleMap=this->commManager->getParamRoleMapping();

	for (auto &x: paramRoleMap)
	{
		ParamRole paramRole=x.second;

		vector<Role*> roles= paramRole.getTheRoles();

		for (int i = 0; i < roles.size(); i++)
		{
			if (!roles[i]->hasFinished())
				return false;
		}
	}

	return true;
}


Condition MPISimulator::analyzeTargetCondFromExecutorCond(Condition execCond, Expr *tarExpr){


	return Condition(false);
}

////////////////////////////////////////////////////////////////////////////
void MPISimulator::simulate(){

	cout<<"Ready to simulate the execution of the MPI program now!"<<endl;
	map<string,ParamRole> paramRoleMap=this->commManager->getParamRoleMapping();

	cout<<"There are "<<paramRoleMap.size()<<" communicator groups involved"<<endl;

	this->printTheRoles();

	this->initTheRoles();

	if(paramRoleMap.size()==0){
		cout<<"No group was constructed..."<<endl;
		return;
	}



	while (!this->root.isMarked() && !isDeadLockDetected())
	{
		for (auto &x: paramRoleMap)
		{
			string paramRoleName=x.first;			

			ParamRole paramRole=x.second;

			vector<Role*> roles= paramRole.getTheRoles();

			int size=roles.size();

			for (int i = 0; i < size; i++)
			{
				cout<<"It is "<< roles.at(i)->getRoleName()<<" visiting the tree now."<<endl;
				VisitResult *vr=roles[i]->visit();

				if (vr){
					vr->printVisitInfo();

					this->analyzeVisitResult(vr);
				}

			}
		}

		//////////////////////////////////////////////////////////////////////////////////
		if (areAllRolesDone())
		{
			//if all the roles finished their work, then simulation completes successfully
			this->root.setMarked();
		}
	}
}




/**
Analyse the visit result. push the prospective MPI op into the proper stack,
manage the newly created roles.
*/
void MPISimulator::analyzeVisitResult(VisitResult *vr){
	//TODO
	vector<Role> escapedRoles=vr->escapableRoles;
	int size=escapedRoles.size();

	if (size>0)
	{
		string paramName=escapedRoles.at(0).getParamRoleName();
		ParamRole paramRole=this->commManager->getParamRoleMapping().at(paramName);

		for (int i = 0; i <size ; i++)
		{
			paramRole.insertActualRole(&escapedRoles[i]);
		}
	}

	MPIOperation *op=vr->doableOP;
	if (op)
	{
		//if the target condition has not been built, then build it
		if(op->isDependentOnExecutor()){
			Condition exec=op->getExecutor();

			this->commManager->simplyInsertCond(exec);

			Condition newTarget=this->commManager->extractCondFromTargetExpr(op->getTargetExpr());

			op->setTargetCond(newTarget);

			this->commManager->popCondition();
		}

		this->insertOpToPendingList(op);
	}

}


void MPISimulator::insertOpToPendingList(MPIOperation *op){
	//TODO
}

/********************************************************************/
//Class MPISimulator impl end									****
/********************************************************************/