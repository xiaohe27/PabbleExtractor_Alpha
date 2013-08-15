#include "Comm.h"
using namespace llvm;
using namespace clang;
using namespace std;





ProtocolGenerator::ProtocolGenerator(MPITree *tree, map<string,ParamRole*>  paramRoleNameMapping0){
	this->mpiTree=tree;
	this->paramRoleNameMapping=paramRoleNameMapping0;

	if (this->paramRoleNameMapping.size()==0)
		throw new MPI_TypeChecking_Error("Empty Role List Error. Unable to generate the protocol in such situation!");

}



////////////////////////////////////////////////////////////////////////////////////////////
string ProtocolGenerator::genRoleName(string paramRoleName, Range ran){
	return paramRoleName+ran.printRangeInfo();
}


string ProtocolGenerator::insertRankInfoToRangeStr(string rangeInfoStr){

	rangeInfoStr.insert(1,RANKVAR+":");

	return rangeInfoStr;
}

string ProtocolGenerator::genRoleByVar(string paramRoleName, string varName){
	return paramRoleName+"["+varName+"]";
}


//TODO
string ProtocolGenerator::selfCreatedLoop(MPINode *mpinode){
	mpinode->isInRankSpecificForLoop=false;

	ForEachNode *theFinalForEachNode, *cur;

	vector<ForEachNode*> forloops=mpinode->rankSpecificForLoops;
	cur=forloops.at(0);
	MPIForEachNode *mpi4Each=new MPIForEachNode(cur);

////////////////////////////////////////////////////////////////////////////	
	mpinode->getMPIOP()->isBothCastAndGather=true;
////////////////////////////////////////////////////////////////////////////

	mpi4Each->insert(mpinode);

	for (int i = 1; i < forloops.size(); i++)
	{
		MPIForEachNode* tmp=new MPIForEachNode(forloops.at(i));
		tmp->insert(mpi4Each);
		mpi4Each=tmp;
	}

	return this->globalForeach(mpi4Each);
}
///////////////////////////////////////////////////////////////////////////////////////////



//TODO
void ProtocolGenerator::generateTheProtocols(){
	string globalP=this->generateGlobalProtocol();

	//	cout<<"\n\n\n"<<globalP<<"\n\n\n"<<endl;
	writeProtocol(globalP);

	for (auto &x: paramRoleNameMapping)
	{
		string localP=this->generateLocalProtocol(x.second);

		cout<<"\n\n\n"<<localP<<"\n\n\n"<<endl;
	}
}

string ProtocolGenerator::protocolName(){
	return fileName+"_ProToCoL";
}

string ProtocolGenerator::globalProtocolHeader(){
	return "global protocol "+protocolName()+" "+roleDeclList();
}

string ProtocolGenerator::localProtocolHeader(string roleName){
	return "local protocol "+protocolName()+" at "+roleName+" "+roleDeclList();
}

string ProtocolGenerator::roleDeclList(){
	string roleDeclListStr="(";

	map<string,ParamRole*>::iterator it;
	it=this->paramRoleNameMapping.begin();
	string firstRoleName=it->second->getFullParamRoleName();
	roleDeclListStr+= roleDecl(firstRoleName);

	++it;
	for(; it != this->paramRoleNameMapping.end(); ++it) {
		roleDeclListStr+=", "+roleDecl(it->second->getFullParamRoleName());
	}

	roleDeclListStr+=")";

	return roleDeclListStr;
}


string ProtocolGenerator::roleDecl(string roleName){
	return "role "+roleName;
}


//////////////////////////////////////////////////////////////////////////////////
string ProtocolGenerator::globalDef(){
	return globalInteractionBlock(this->mpiTree->getRoot());
}


string ProtocolGenerator::globalInteractionBlock(MPINode *theRoot){
	string block="{";
	block+=globalInteractionSeq(theRoot);
	block+="}";
	return block;
}


string ProtocolGenerator::globalInteractionSeq(MPINode *rootNode){

	string seq="";
	for (int i = 0; i < rootNode->getChildren().size(); i++)
	{
		seq+=globalInteraction(rootNode->getChildren().at(i))+"\n";
	}

	return seq;
}


string ProtocolGenerator::globalInteraction(MPINode *node){
	if (node->isInRankSpecificForLoop)
	{
		return this->selfCreatedLoop(node);
	}

	//if the node is a leaf node, then it must be an MPI op node.
	if (node->isLeaf())
	{
		return globalMsgTransfer(node);
	}

	if (node->getNodeType()==ST_NODE_ROOT)
	{
		return globalInteractionBlock(node);
	}

	if (node->getNodeType()==ST_NODE_CHOICE)
	{
		return globalChoice(node);
	}

	if (node->getNodeType()==ST_NODE_RECUR)
	{
		return globalRecur(node);
	}

	if (node->getNodeType()==ST_NODE_FOREACH)
	{
		return globalForeach((MPIForEachNode*)node);
	}

	if (node->getNodeType()==ST_NODE_CONTINUE)
	{
		return globalContinue(node);
	}


	throw new MPI_TypeChecking_Error("Encounter an un-supported request. Program abort unexpectedly.");
}

//Not fully conform to the BNF...
string ProtocolGenerator::payLoad(MPIOperation* mpiOP){
	//The MPI data type.
	return mpiOP->getDataType();
}

string ProtocolGenerator::message(MPIOperation* mpiOP){
	string sig=msgOperator();
	sig+="("+payLoad(mpiOP)+")";

	return sig;
}



string ProtocolGenerator::getReceiverRoles(MPIOperation *mpiOP, int pos){
	string out="";

	if (mpiOP->isUnicast())
	{
		Range ran=mpiOP->getDestCond().getRangeList().at(pos);
		out=genRoleName(WORLD,ran);

		if (mpiOP->isDependentOnExecutor())
		{
			out=genRoleByVar(WORLD,mpiOP->getTarExprStr());
		}
	}

	else if (mpiOP->isMulticast())
	{
		Condition recvCond=mpiOP->getTargetCond();
		Range firstRan=recvCond.getRangeList().at(0);
		out=genRoleName(WORLD,firstRan);     

		for (int i = 1; i < recvCond.getRangeList().size(); i++)
		{
			Range ran=recvCond.getRangeList().at(i);
			out+=", "+genRoleName(WORLD,ran);
		}
	}

	else if (mpiOP->isGatherOp())
	{
		Range theSingleOne=mpiOP->getTargetCond().getRangeList().at(0);
		out=genRoleName(WORLD,theSingleOne);
	}

	else{
		throw new MPI_TypeChecking_Error("NOT supported MPI op...");
	}

	return out;
}

string ProtocolGenerator::globalMsgTransfer(MPINode *node){
	MPIOperation *theMPIOP=node->getMPIOP();
	if (!theMPIOP)
		throw new MPI_TypeChecking_Error("Expect an MPI operation but found nothing. Program abort unexpectedly.");

	string msg=message(theMPIOP);
	string output="\n";
	Condition execCond=theMPIOP->getExecutor();
	for (int i = 0; i < execCond.getRangeList().size(); i++)
	{
		Range ran=execCond.getRangeList().at(i);

		//the mpi op which is both cast and gather needs to be analyzed in rank level.
		if (theMPIOP->isBothCastAndGather)
		{
			MPINode *parentNode=node->getParent();
			if (parentNode->getNodeType()==ST_NODE_FOREACH)
			{
				if (theMPIOP->isCollectiveOp())
				{
					if(theMPIOP->execExprStr.size()==0){
						output+=msg+" from "+genRoleName(WORLD,Range(0,InitEndIndex-1));
						output+=" to "+genRoleByVar(WORLD,theMPIOP->getTarExprStr())+";\n";
					}

					else{
						output+=msg+" from "+genRoleByVar(WORLD,theMPIOP->execExprStr);
						output+=" to "+genRoleName(WORLD,Range(0,InitEndIndex-1))+";\n";
					}
				}

				else
				{
					if (theMPIOP->execExprStr.size()==0)
					{
						output+=msg+" from "+genRoleName(WORLD,ran);
						output+=" to "+genRoleByVar(WORLD,theMPIOP->getTarExprStr())+";\n";
					}

					else{
						output+=msg+" from "+genRoleByVar(WORLD,theMPIOP->execExprStr);
						output+=" to "+this->getReceiverRoles(theMPIOP,i)+";\n";
					}
				}

			}

			//if the parent is NOT foreach node, then we might need to generate a 
			//foreach output from scratch.
			else{
			//TODO
			}
		}

		else{
			string senderRoleName=genRoleName(WORLD,ran);
			if (theMPIOP->isUnicast() && theMPIOP->isDependentOnExecutor())
			{
				senderRoleName=WORLD+this->insertRankInfoToRangeStr(ran.printRangeInfo());
			}

			output+=msg+" from "+senderRoleName+" to "+getReceiverRoles(theMPIOP, i)+";\n";
		}
	}

	return output;
}

string ProtocolGenerator::globalChoice(MPINode *node){
	string output="\nchoice at MPI_PROGRAM \n";

	MPINode* theFirstBranch=node->getChildren().at(0);
	output+=globalInteraction(theFirstBranch);

	for (int i = 1; i < node->getChildren().size(); i++)
	{
		MPINode *curChild=node->getChildren().at(i);
		output+="\n\n\tor\n\n"+globalInteraction(curChild);
	}

	return output;
}

string ProtocolGenerator::globalRecur(MPINode *node){
	vector<MPINode*> childrenOfTheNode=node->getChildren();

	if (childrenOfTheNode.back()->getNodeType()!=ST_NODE_CONTINUE)
	{
		CommNode *recurNode=new CommNode(ST_NODE_RECUR,Condition(true));
		recurNode->setInfo(node->getLabelInfo());
		ContNode *tmpContNode=new ContNode(recurNode);
		tmpContNode->setToLastContNode();
		MPINode *implicitContNode=new MPINode(tmpContNode);

		node->insertToEndOfChildrenList(implicitContNode);
	}

	string out="\nrec "+node->getLabelInfo()+" ";
	out+=globalInteractionBlock(node);

	return out;
}

string ProtocolGenerator::globalContinue(MPINode *node){
	string out="\ncontinue "+node->getLabelInfo()+";";

	return out;
}


string ProtocolGenerator::globalForeach(MPIForEachNode *node){
	string out="\nforeach ("+node->iterVarName+":";
	out+=node->startPos==InitEndIndex-1 ? "N-1" : convertIntToStr(node->startPos);
	out+="..";
	out+=node->endPos==InitEndIndex-1 ? "N-1" : convertIntToStr(node->endPos);
	out+=")";

	out+=globalInteractionBlock(node);

	return out;
}







string ProtocolGenerator::generateGlobalProtocol(){
	string protocol=globalProtocolHeader();

	protocol+="\n"+globalDef()+"\n";

	return protocol;
}

string ProtocolGenerator::generateLocalProtocol(ParamRole* paramRole){
	string localProtocol=localProtocolHeader(paramRole->getFullParamRoleName());

	return localProtocol;
}