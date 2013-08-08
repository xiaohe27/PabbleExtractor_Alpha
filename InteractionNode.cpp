#include "Comm.h"

using namespace llvm;
using namespace clang;


/********************************************************************/
//Class CommNode impl start										****
/********************************************************************/


void CommNode::init(int type, vector<MPIOperation*> *theOPs){

	this->nodeType=type;
	this->depth=0;
	this->posIndex=0;
	this->branchID="";
	this->srcCodeInfo="";
	this->ops=theOPs;
	this->marked=false;
	this->isMasterNode=false;

	this->parent=nullptr;
	this->sibling=nullptr;

	switch(type){
	case ST_NODE_CHOICE: this->nodeName="CHOICE"; break;

	case ST_NODE_RECUR: this->nodeName="RECUR"; break;

	case ST_NODE_ROOT: this->nodeName="ROOT"; break;

	case ST_NODE_PARALLEL: this->nodeName="PARALLEL"; break;

	case ST_NODE_SEND: this->nodeName="SEND"; break;

	case ST_NODE_RECV: this->nodeName="RECV"; break;

	case ST_NODE_SENDRECV: this->nodeName="SENDRECV"; break;

	case ST_NODE_CONTINUE: this->nodeName="CONTINUE"; break;

	default: this->nodeName="UNKNOWN";

	}

}

CommNode::CommNode(int type, Condition cond){
	init(type, nullptr);
	this->setCond(cond);
}

CommNode::CommNode(MPIOperation *op0){
	if (op0)
	{
		op0->theNode=this;

		vector<MPIOperation*> *theOPs=new vector<MPIOperation*>();
		theOPs->push_back(op0);

		init(op0->getOPType(),theOPs);
		this->setCond(Condition(true));
	}

	else{
		throw new MPI_TypeChecking_Error("Trying to construct a MPI node via empty mpi op!");
	}
}

CommNode* CommNode::getClosestNonRankAncestor(){
	if (this->isMaster())
	{
		return this;
	}

	else{
		return this->getParent()->getClosestNonRankAncestor();
	}
}

bool CommNode::isNonRankChoiceNode(){
	if (this->nodeType!=ST_NODE_CHOICE)
		return false;

	for (int i = 0; i < this->children.size(); i++)
	{
		if (!this->children.at(i)->getCond().isComplete())
		{
			return false;
		}
	}

	return true;
}

void CommNode::setNodeType(int type){
	this->nodeType=type;
}

int CommNode::indexOfTheMPIOP(MPIOperation* op){
	for (int i = 0; i < this->ops->size(); i++)
	{
		if (this->ops->at(i)==op)
		{
			return i;
		}
	}

	return -1;
}

bool CommNode::isMarked() {
	if(this->marked)
		return true;

	if(!this->isLeaf()){

		for (int i = 0; i < this->children.size(); i++)
		{
			bool markI=this->children.at(i)->isMarked();
			if (!markI)
			{
				return false;
			}
		}

		this->marked=true;
		return true;
	}

	else{
		return false;
	}
}

void CommNode::setMarked(){
	this->marked=true;

	if (this->getOPs())
	{
		delete this->ops;
	}
}

bool CommNode::isNegligible() {
	if(this->isMarked() || this->condition.isIgnored()){
		this->marked=true;
		return true;
	}

	else if(!this->isLeaf()){
		for (int i = 0; i < this->children.size(); i++)
		{
			bool negI=this->children.at(i)->isNegligible();
			if (!negI)
			{
				return false;
			}
		}

		this->marked=true;
		return true;
	}

	else if(this->ops){
		for (int i = 0; i < this->ops->size(); i++)
		{
			if (this->ops->at(i)==nullptr|| this->ops->at(i)->isFinished())
			{
				this->ops->erase(this->ops->begin()+i);
				i--;
				continue;
			}

			else
				return false;
		}

		this->marked=true;
		return true;
	}

	else
		return false;
}

void CommNode::initTheBranchId(){
	if (this->isNonRankChoiceNode())
	{
		string newBranStr=convertIntToStr(this->posIndex);
		this->branchID=newBranStr+"_Choice!";
		for (int i = 0; i < children.size(); i++)
		{
			children.at(i)->branchID=newBranStr+"_branch"+convertIntToStr(i);
			children.at(i)->setMaster();
		}
	}

	else{
		for (int i = 0; i < children.size(); i++)
		{
			children.at(i)->branchID=this->branchID;
		}
	}

	//mk it recursive 
	for (int i = 0; i < children.size(); i++)
	{
		children.at(i)->initTheBranchId();
	}
}

//print the tree rooted at this node
string CommNode::printTheNode(){
	string output="";

	string indent="";

	for (int i = 0; i < this->depth; i++)
	{
		indent+="\t";
	}

	output+=indent+this->nodeName+": "+(this->nodeType==ST_NODE_ROOT?
		this->condition.printConditionInfo():"");

	output+="\n";

	//print the children
	for (int i = 0; i < this->children.size(); i++)
	{
		CommNode *childNode=this->children[i];
		output+=childNode->printTheNode()+"\n";
	}

	return output;
}

bool CommNode::isLeaf() const{
	if (children.size()==0)
	{
		return true;
	}

	return false;
}

void CommNode::insert(CommNode *child){
	child->parent=this; 

	child->depth=this->depth+1;

	child->posIndex= this->posIndex+this->sizeOfTheNode();

	if(this->children.size()!=0){
		this->children.back()->sibling=child;
	}

	this->children.push_back(child);

}

CommNode* CommNode::goDeeper(){
	if(this->children.size()==0)
	{return this->skipToNextNode();}

	else{
		return children[0];
	}
}

CommNode* CommNode::skipToNextNode(){
	if (!this->parent)
	{
		return nullptr;
	}

	if (this->sibling)
	{
		return this->sibling;
	}

	else
	{
		return this->parent->skipToNextNode();
	}
}


int CommNode::sizeOfTheNode(){
	if (this->isLeaf())
		return 1;

	int sum=0;
	for (int i = 0; i < this->children.size(); i++)
	{
		sum+=children[i]->sizeOfTheNode();
	}

	return sum+1;
}


RecurNode* CommNode::getInnerMostRecurNode(){
	if (this->getNodeType()==ST_NODE_RECUR)
	{
		return (RecurNode*)this;
	}

	else{
		return this->getParent()->getInnerMostRecurNode();
	}
}

/********************************************************************/
//Class CommNode impl end										****
/********************************************************************/



/********************************************************************/
//Class RecurNode impl start										****
/********************************************************************/
void RecurNode::visitOnce()
{
	if(remainingNumOfIterations>0)remainingNumOfIterations--;

	if(remainingNumOfIterations==0){
		this->setMarked();
	}
}

/********************************************************************/
//Class RecurNode impl end										****
/********************************************************************/


/********************************************************************/
//Class ContNode impl start										****
/********************************************************************/
ContNode::ContNode(RecurNode *node):CommNode(ST_NODE_CONTINUE,Condition(true))
{
	this->refNode=node;

	this->setInfo(this->refNode->getSrcCodeInfo());
}

/********************************************************************/
//Class ContNode impl end										****
/********************************************************************/