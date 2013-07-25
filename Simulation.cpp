#include "Comm.h"

using namespace llvm;
using namespace clang;
using namespace std;


//////////////////////VisitResult//////////////////////////////////////////
VisitResult::VisitResult(bool b, MPIOperation *op, Role *r){
		this->isBlocking=b;
		this->doableOP=op;
		this->escapableRole=r;
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

			for (int i = 0; i < roles.size(); i++)
			{
				cout<<"The role "<<i<<" is "<< roles.at(i)->getRoleName()<<endl;
			}
		}
	}
}







/********************************************************************/
//Class MPISimulator impl end									****
/********************************************************************/