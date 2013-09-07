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
	this->mpiOPIsForbidden=false;
	this->isMasterNode=false;
	this->isRelatedToRank=false;

	this->parent=nullptr;
	this->sibling=nullptr;
	this->execExpr=nullptr;

	switch(type){
	case ST_NODE_CHOICE: this->nodeName="CHOICE"; break;

	case ST_NODE_RECUR: this->nodeName="RECUR"; break;

	case ST_NODE_FOREACH: this->nodeName="FOREACH"; break;

	case ST_NODE_ROOT: this->nodeName="ROOT"; break;

	case ST_NODE_PARALLEL: this->nodeName="PARALLEL"; break;

	case ST_NODE_SEND: this->nodeName="SEND"; break;

	case ST_NODE_RECV: this->nodeName="RECV"; break;

	case ST_NODE_SENDRECV: this->nodeName="SENDRECV"; break;

	case ST_NODE_CONTINUE: this->nodeName="CONTINUE"; break;

	case MPI_Wait: this->nodeName="WAIT"; break;

	case MPI_Barrier: this->nodeName="BARRIER"; break;

	default: this->nodeName="UNKNOWN";

	}

}

CommNode::CommNode(int type, Condition cond){
	init(type, nullptr);
	this->setCond(cond);
	this->rawCond=cond;
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

Expr* CommNode::getExecExpr(){
	if (this->execExpr)
		return this->execExpr;
	
	else
		return this->parent->getExecExpr();
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

//used when the comm tree has been constructed.
bool CommNode::isBelowARankSpecificForLoop(){
	if (this->isMaster())
		return false;

	if (this->nodeType==ST_NODE_FOREACH)
		return true;

	return this->parent->isBelowARankSpecificForLoop();
}

vector<ForEachNode*> CommNode::getAllTheSurroundingRankSpecificForLoops(){

	vector<ForEachNode*> forList;
	if(!this->isLeaf())
		return forList;

	CommNode *cur=this;
	while(true){
		if (cur->isMaster())	
			return forList;


		if (cur->nodeType==ST_NODE_FOREACH)		
			forList.push_back((ForEachNode*)cur);

		cur=cur->parent;
	}

}


void CommNode::reportFinished(Condition executor){
	Condition repeatedCond=executor.Diff(this->condition);
	if (!repeatedCond.isIgnored())
	{
		throw new MPI_TypeChecking_Error("Error! The processes with condition "
			+repeatedCond.printConditionInfo()+" repeat the mpi op in "+
			this->printTheNode());
	}

	this->condition=this->condition.Diff(executor);

	if (this->condition.isIgnored())
	{
		this->setMarked();
	}

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
		this->ops=nullptr;
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

	else if(this->isLeaf() && this->isStructureNode()){
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


bool CommNode::isRankRelatedChoice(){
	if (this->getNodeType()==ST_NODE_CHOICE && this->isRelatedToRank)
	{
		return true;
	}

	else
		return false;
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

bool CommNode::isStructureNode(){
	if(	this->nodeType==ST_NODE_ROOT || 
		this->nodeType==ST_NODE_RECUR ||
		this->nodeType==ST_NODE_FOREACH ||
		this->nodeType==ST_NODE_CHOICE)
		return true;

	else
		return false;
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


CommNode* CommNode::getInnerMostRecurNode(){
	if (this->getNodeType()==ST_NODE_RECUR)
	{
		return this;
	}

	else{
		return this->getParent()->getInnerMostRecurNode();
	}
}

//this method is executed by the parent of the wait node.
MPIOperation* CommNode::findTheClosestNonblockingOPWithReqName(string reqName){
	for (int i = this->children.size()-1; i >=0 ; i--)
	{
		CommNode* curChild=this->children.at(i);
		if (curChild->ops && curChild->ops->at(0)->isNonBlockingOPWithReqVar(reqName))
		{
			return curChild->ops->at(0);
		}

		if (curChild->getNodeType()==MPI_Wait)
		{
			WaitNode *waitN=(WaitNode*)curChild;
			if(waitN->type==WaitNode::waitAll)
				return nullptr;
			if(waitN->type==WaitNode::waitNormal && waitN->req==reqName)
				return nullptr;
		}
	}

	throw new MPI_TypeChecking_Error("Cannot Find the corresponding non-blocking op with req var "+reqName);
}


void CommNode::optimize(){
	for (int i = 0; i < this->children.size(); ++i)
	{
		CommNode *childI=this->children.at(i);
		if (childI->condition.isIgnored())
		{
			childI->deleteItsTree();

			this->children.erase(this->children.begin()+i);
			--i;
			if(i>=0){
					this->children.at(i)->sibling=nullptr;

				if(i+1<children.size())
					this->children.at(i)->sibling=this->children.at(i+1);
			}
		}

		else if(childI->isStructureNode() && childI->children.size()==0)
		{
			childI->deleteItsTree();

			this->children.erase(this->children.begin()+i);
			--i;
			if(i>=0){
		
					this->children.at(i)->sibling=nullptr;
				
				if(i+1<children.size())
					this->children.at(i)->sibling=this->children.at(i+1);
			}
		}
		else{
			childI->optimize();
			if(childI->isStructureNode() && childI->children.size()==0)
			{
				childI->deleteItsTree();

				this->children.erase(this->children.begin()+i);
				--i;
				if(i>=0){

						this->children.at(i)->sibling=nullptr;
					
					if(i+1<children.size())
					this->children.at(i)->sibling=this->children.at(i+1);
				}
			}
		}
	}

}

void CommNode::deleteItsTree(){
	for (int i = 0; i < this->children.size(); i++)
	{
		this->children.at(i)->deleteItsTree();
	}

	delete this;
}
/********************************************************************/
//Class CommNode impl end										****
/********************************************************************/





/********************************************************************/
//Class ContNode impl start										****
/********************************************************************/
ContNode::ContNode(CommNode *node):CommNode(ST_NODE_CONTINUE,Condition(true))
{
	this->refNode=node;

	this->setInfo(this->refNode->getSrcCodeInfo());
}

/********************************************************************/
//Class ContNode impl end										****
/********************************************************************/


/********************************************************************/
//Class ForEachNode impl start										****
/********************************************************************/

ForEachNode::ForEachNode(string iterVar,int start,int end):CommNode(ST_NODE_FOREACH,Condition(true))
{
	this->iterVarName=iterVar;
	this->startingIndex=start;
	this->endingIndex=end;
}

/********************************************************************/
//Class ForEachNode impl end										****
/********************************************************************/


/********************************************************************/
//Class BarrierNode impl start										****
/********************************************************************/

BarrierNode::BarrierNode(ParamRole *group):CommNode(MPI_Barrier,Condition(true)){
	this->commGroup=group;
	this->curArrivedProcCond=Condition(false);
}

void BarrierNode::visit(Condition visitorCond){
	this->curArrivedProcCond=this->curArrivedProcCond.OR(visitorCond);

	if (this->curArrivedProcCond.isComplete())
	{
		this->setMarked();
		this->commGroup->getTheRoles()->clear();
		Role *allRanRole=new Role(Range(0,N-1));
		allRanRole->setCurVisitNode(this->skipToNextNode());
		this->commGroup->getTheRoles()->push_back(allRanRole);
	}
}

/********************************************************************/
//Class BarrierNode impl end										****
/********************************************************************/