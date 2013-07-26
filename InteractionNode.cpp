#include "Comm.h"

using namespace llvm;
using namespace clang;


/********************************************************************/
//Class CommNode impl start										****
/********************************************************************/


void CommNode::init(int type, MPIOperation *mpiOP){

	this->nodeType=type;
	this->depth=0;
	this->op=mpiOP;
	this->marked=false;

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

CommNode::CommNode(int type){
	init(type, nullptr);
}

CommNode::CommNode(MPIOperation *op0){

	init(op0->getOPType(),op0);

}

void CommNode::setNodeType(int type){
	this->nodeType=type;
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

bool CommNode::isLeaf(){
	if (children.size()==0)
	{
		return true;
	}

	return false;
}

void CommNode::insert(CommNode *child){
	child->parent=this; 

	child->depth=this->depth+1;

	if(this->children.size()!=0){
		this->children.back()->sibling=child;
	}

	this->children.push_back(child);

}

CommNode* CommNode::goDeeper(){
	if(this->children.size()==0)
	{return this->sibling;}

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