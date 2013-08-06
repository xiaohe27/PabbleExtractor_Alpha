#include "Comm.h"
using namespace llvm;
using namespace clang;
using namespace std;

/**************************************************************************/
//MPINode impl start
/**************************************************************************/
MPINode::MPINode(CommNode* node){
	this->index=node->getPosIndex();
	this->depth=node->getDepth();
	this->op=nullptr;
}

MPINode::MPINode(MPIOperation* theOp){
	this->index= theOp->theNode->getPosIndex();
	this->depth=theOp->theNode->getDepth();
	this->op=theOp;
}

bool MPINode::isLeaf(){
	if (this->op==nullptr)
		return false;
	
	else
		return true;
}

//insert the child to itself
void MPINode::insert(MPINode *child){
	if (this->isLeaf())
		throw new MPI_TypeChecking_Error
		("Can't insert a child to the leaf MPI op node. Fail to build MPI tree!");

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