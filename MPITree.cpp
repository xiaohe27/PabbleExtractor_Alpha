#include "Comm.h"
using namespace llvm;
using namespace clang;
using namespace std;

/**************************************************************************/
//MPINode impl start
/**************************************************************************/

MPINode::MPINode(CommNode* node){
	this->index=node->getPosIndex();
	this->nodeType=node->getNodeType();
	this->depth=node->getDepth();
	this->op=nullptr;
	this->parent=nullptr;
	this->labelInfo=node->getSrcCodeInfo();

	this->isInRankSpecificForLoop=false;
}


void MPINode::initMPIOpNode(MPIOperation* theOp){
	this->index= theOp->theNode->getPosIndex();
	this->nodeType=theOp->theNode->getNodeType();
	this->depth=theOp->theNode->getDepth();
	this->op=theOp;
	this->parent=nullptr;
	this->labelInfo="";
}

MPINode::MPINode(MPIOperation* theOp){
	this->initMPIOpNode(theOp);

	if (!theOp->theNode->isBelowARankSpecificForLoop())
		this->isInRankSpecificForLoop=false;


	else{
		this->isInRankSpecificForLoop=true;
		this->rankSpecificForLoops=theOp->theNode->getAllTheSurroundingRankSpecificForLoops();
	}

}


bool MPINode::isLeaf(){
	if (this->op==nullptr)
		return false;

	else
		return true;
}

void MPINode::insertToEndOfChildrenList(MPINode *lastContNode)
{
	this->children.push_back(lastContNode);
}

//TODO
MPIOperation* MPINode::combineMPIOPs(MPIOperation* op1, MPIOperation* op2){
	/*
	bool op1B=op1->isStable();
	bool op2B=op2->isStable();

	bool imperfect=(op1B || op2B) && !(op1B && op2B);
	*/

	bool imperfect=false;
	if(op1->execExprStr.size()>0 && op2->execExprStr.size()==0 
		|| op2->execExprStr.size()>0 && op1->execExprStr.size()==0 )
		imperfect=true;

	if(imperfect)
		return nullptr;

	//because every op has been transformed to sending op before inserted to the node
	//we can assume both ops are sending ops
	int span1=Condition::getDistBetweenTwoCond(op1->getExecutor(),op1->getTargetCond());
	int span2=Condition::getDistBetweenTwoCond(op2->getExecutor(),op2->getTargetCond());

	if (op1->isUnicast() && op2->isUnicast() && span1==span2)
	{

		if (Condition::areTheseTwoCondAdjacent(op1->getExecutor(),op2->getExecutor()) &&
			Condition::areTheseTwoCondAdjacent(op1->getTargetCond(),op2->getTargetCond()))
		{
			MPIOperation* combi=new MPIOperation(*op2);
			combi->setExecutorCond(op1->getExecutor().OR(op2->getExecutor()));
			combi->setTargetCond(op1->getTargetCond().OR(op2->getTargetCond()));

			////////////////////////////////
			if(op1->getExecutor().isVolatile && op2->getExecutor().isVolatile &&
				op1->execExprStr.size()>0 && op2->execExprStr.size()>0){
				if(op1->getExecutor().getRangeList().at(0).getStart() <
					op2->getExecutor().getRangeList().at(0).getStart())
				{
					combi->execExprStr=op1->execExprStr+".."+op2->execExprStr;
				}

				else{
					combi->execExprStr=op2->execExprStr+".."+op1->execExprStr;
				}

			}
			///////////////////////////////
			return combi;
		}

		else if (op1->getExecutor().isSameAsCond(op2->getExecutor()) && 
			Condition::areTheseTwoCondAdjacent(op1->getTargetCond(),op2->getTargetCond()))
		{
			MPIOperation* combi=new MPIOperation(*op2);
			combi->isTargetDependOnExecutor=false;
			combi->isBothCastAndGather=true;
			combi->setExecutorCond(op1->getExecutor());
			combi->setTargetCond(op1->getTargetCond().OR(op2->getTargetCond()));
			return combi;
		}

		else if(op1->getTargetCond().isSameAsCond(op2->getTargetCond()) && 
			Condition::areTheseTwoCondAdjacent(op1->getExecutor(),op2->getExecutor())){
				MPIOperation* combi=new MPIOperation(*op2);
				combi->isTargetDependOnExecutor=false;
				combi->isBothCastAndGather=true;
				combi->setExecutorCond(op1->getExecutor().OR(op2->getExecutor()));
				combi->setTargetCond(op1->getTargetCond());
				return combi;
		}

		else
			return nullptr;
	}

	if (op1->isMulticast() && op2->isMulticast() || 
		op1->isGatherOp() && op2->isGatherOp())
	{
		if (op1->getExecutor().isSameAsCond(op2->getExecutor()) && 
			Condition::areTheseTwoCondAdjacent(op1->getTargetCond(),op2->getTargetCond()))
		{
			MPIOperation* combi=new MPIOperation(*op2);
			combi->isTargetDependOnExecutor=false;
			combi->isBothCastAndGather=true;
			combi->setTargetCond(op1->getTargetCond().OR(op2->getTargetCond()));
			return combi;
		}

		else if (op1->getTargetCond().isSameAsCond(op2->getTargetCond()) && 
			Condition::areTheseTwoCondAdjacent(op1->getExecutor(),op2->getExecutor()))
		{
			MPIOperation* combi=new MPIOperation(*op2);
			combi->isTargetDependOnExecutor=false;
			combi->isBothCastAndGather=true;
			combi->setExecutorCond(op1->getExecutor().OR(op2->getExecutor()));
			return combi;
		}

		else nullptr;
	}

	return nullptr;
}


//insert the child to itself
void MPINode::insert(MPINode *child){
	if (this->isLeaf())
		throw new MPI_TypeChecking_Error
		("Can't insert a child to the leaf MPI op node. Fail to build MPI tree!");

	child->parent=this;

	//if the inserted node is mpi op node, then some combination may happen
	MPIOperation *childOP=child->op;
	if (childOP)
	{
		bool hasCombined=false;
		MPINode* theLivingNode=nullptr;//use this node to put the final combined mpi op.

		for (int i = 0; i < children.size(); i++)
		{
			MPINode *curVNode=children.at(i);
			if (!curVNode->op)
				continue;

			MPIOperation *curOp=curVNode->op;

			MPIOperation *theCombinedOP=MPINode::combineMPIOPs(childOP,curOp);
			if (theCombinedOP)
			{
				hasCombined=true;
				delete childOP;
				delete curOp;
				curVNode->op=nullptr;
				childOP=theCombinedOP;

				if (theLivingNode==nullptr)
					theLivingNode=curVNode;

				else{
					this->children.erase(children.begin()+i);
				}

				i=-1;
			}
		}

		if (hasCombined)
		{
			theLivingNode->op=childOP;
			return; //the op has been combined with an existing op, so return.
		}
	}


	bool inserted=false;

	for (int i = 0; i < children.size(); i++)
	{
		MPINode *curChild=children.at(i);

		if(child->index < curChild->index){
			this->children.insert(this->children.begin()+i,child);
			inserted=true;
			break;
		}
	}

	if (!inserted)
	{
		this->children.push_back(child);
	}

	return;
}


void MPINode::insertToProperNode(MPINode *node){
	if (node->depth <= this->depth)
	{
		string errInfo="\nError occured during the MPI tree construction.";
		errInfo+="\nTry to insert a higher level node to a lower level node\n";
		throw new MPI_TypeChecking_Error(errInfo);
	}

	if (node->depth==this->depth+1)
	{
		//the inserted node is the child of this node
		this->insert(node);
		return;
	}

	else{
		for (int i = 0; i < this->children.size(); i++)
		{
			if (node->index < this->children.at(i)->index)
			{
				if (i-1<0)
					throw new MPI_TypeChecking_Error("Fail to build the MPI Tree!");

				this->children.at(i-1)->insertToProperNode(node);
				return;
			}
		}

		if (this->children.size()==0)
			throw new MPI_TypeChecking_Error("Fail to build the MPI Tree!");

		this->children.back()->insertToProperNode(node);


	}
}

/**************************************************************************/
//MPINode impl end
/**************************************************************************/


/**************************************************************************/
//MPIForEach impl start
/**************************************************************************/

MPIForEachNode::MPIForEachNode(ForEachNode *forEach):
	MPINode(forEach){
		this->iterVarName=forEach->getIterVarName();
		this->startPos=forEach->getStartingIndex();
		this->endPos=forEach->getEndingIndex();
}

/**************************************************************************/
//MPIForEach impl end
/**************************************************************************/


/**************************************************************************/
//MPITree impl start
/**************************************************************************/
MPITree::MPITree(MPINode *rtNode){
	this->root=rtNode;
}

void MPITree::insertNode(MPINode* mpiNode){
	this->root->insertToProperNode(mpiNode);
}


/**************************************************************************/
//MPITree impl end
/**************************************************************************/