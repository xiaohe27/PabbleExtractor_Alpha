#ifndef Comm_H
#define Comm_H

#include <iostream>
#include "include/st_node.h"
#include <vector>
#include <stack>
#include <array>
#include <set>
#include <map>
#include <string>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <climits>

#include "clang/Frontend/CompilerInstance.h"
#include "clang/AST/Stmt.h"
#include "llvm/Support/Host.h"
#include "llvm/ADT/IntrusiveRefCntPtr.h"
#include "clang/Basic/DiagnosticOptions.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Basic/TargetOptions.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/Basic/LangOptions.h"
#include "clang/Parse/Parser.h"
#include "clang/Parse/ParseAST.h"
#include "clang/Analysis/CFG.h"
#include "clang/AST/Stmt.h"
#include "clang/Lex/HeaderSearch.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Analysis/CallGraph.h"
#include "clang/Analysis/AnalysisContext.h"
#include "clang/AST/DeclVisitor.h"
#include "clang/AST/StmtVisitor.h"
#include "clang/Frontend/Utils.h"

using namespace std;
using namespace clang;



#define ST_NODE_FOREACH 8
#define MPI_Wait 9
#define MPI_Barrier 10

class CommNode;
class WaitNode;
class ForEachNode;
class Condition;
class MPIOperation;
class Role;
class MPINode;
class MPITree;

extern int N;
extern bool STRICT;
extern string MPI_FILE_NAME;
extern string RANKVAR;
extern map<string,Condition> VarCondMap;

/////////////////////////////////////////////////
extern int LargestKnownRank; //the LFP is thisInt+2
extern void updateLargestKnownRank(int num);
extern int getLFP(); // return the least stable point

extern bool IsGenericProtocol;
extern bool IsProtocolStable();

extern set<string> unboundVarList;
////////////////////////////////////////////////

#define InitStartIndex INT_MIN
#define InitEndIndex INT_MAX

#define WORLD "MPI_COMM_WORLD"




class MPI_TypeChecking_Error{

public:
	string errInfo;

	MPI_TypeChecking_Error(string err){
		this->errInfo="\n\n"+err+"\n\n";
	}

	void printErrInfo(){
		cout<<errInfo<<endl;
	}
};

string convertIntToStr(int number);

string convertDoubleToStr(double number);

string stmt2str(SourceManager *sm, LangOptions lopt,clang::Stmt *stmt);

bool evalIntCmpTruth(int arg1, int arg2, string op);

void writeToFile(string content);

void writeProtocol(string protocol);


int min(int a, int b);
int max(int a, int b);

int compute(string op, int operand1, int operand2);
bool areTheseTwoNumsAdjacent(int a, int b);


class Range{
private:
	int startPos;
	int endPos;

public:

	int getStart(){return startPos;}
	int getEnd(){return endPos;}

	Range();

	Range(int s,int e);

	bool isIgnored(){return this->size()==0;}

	bool isAllRange();

	bool isThisNumInside(int num);

	bool isSpecialRange(){return getEnd() < getStart();}

	bool isSuperSetOf(Range ran);

	static Range createByOp(string op, int num);
	static Range createByStartIndex(int start);
	static Range createByEndIndex(int end);
	Condition AND(Range other);
	bool hasIntersectionWith(Range other);

	Condition OR(Range other);

	bool isEqualTo(Range ran);

	static Condition negateOfRange(Range ran);

	void addNumber(int num);

	int size();

	int getLargestNum();

	string printRangeInfo();
};

class Condition{
private:
	vector<Range> rangeList;
	string groupName;

	//the non-rank var related condition might be important in determining the target of MPI op!
	string nonRankVarName;

	bool mixed;
	bool isTrivialCond;

public:
	int offset;
	string execStr; //the expr str of the executor.
	bool isVolatile;

	Condition();

	Condition(bool val);

	vector<Range> getRangeList(){return this->rangeList;}

	int size();

	bool isIgnored();

	bool isMixtureCond(){return this->mixed;}

	void setAsMixedCond(){this->mixed=true;}

	bool isComplete();

	Condition(Range ran);

	Condition(Range ran1, Range ran2);

	static Condition negateCondition(Condition cond);

	static Condition createCondByOp(string op, int num);

	static int getDistBetweenTwoCond(Condition cond1,Condition cond2);

	void normalize();

	bool isRangeConsecutive(){return this->rangeList.size()==1;}

	Condition AND(Condition other);

	Condition OR(Condition other);

	Condition Diff(Condition other);

	Condition addANumber(int num);

	Condition setVolatile(bool bv){this->isVolatile=bv; return *this;}

	string getGroupName(){return this->groupName;}

	void setCommGroup(string group){this->groupName=group;}

	bool hasSameGroupComparedTo(Condition other);

	string getNonRankVarName(){return this->nonRankVarName;}

	bool isRelatedToRank(){return this->nonRankVarName=="";}

	bool hasSameRankNature(Condition other);//if both are (NOT) related to rank, then ret true 

	void setNonRankVarName(string name){this->nonRankVarName=name;}

	bool isRankInside(int rankNum);

	bool isRangeInside(Range ran);

	Range getTheRangeContainingTheNum(int num);

	bool isSameAsCond(Condition other);

	bool isTrivial(){return this->isTrivialCond;}

	void setAsTrivial(){this->isTrivialCond=true;}

	static bool areTheseTwoCondAdjacent(Condition cond1, Condition cond2);

	bool outsideOfBound(){return this->isRankInside(N) || this->isRankInside(-1);}

	int getLargestNum();

	string printConditionInfo();
};


class VisitResult{
public:
	MPIOperation *doableOP;
	vector<Role*> escapableRoles; //the role is escapable if it is not captured by the current node

	VisitResult(MPIOperation *mpiOP, vector<Role*> escapedRoles);

	void printVisitInfo();
};


class Role{
private:
	Range range;

	//the naming convention is 
	//ParamRoleName[startIndex..endIndex]
	string paramRoleName;

	CommNode *curVisitNode;

	bool blocked;

	bool finished;

public:

	Role(Range ran);

	Role(string paramRoleName0, Range ran);

	Range getRange(){return this->range;}

	bool hasRangeEqualTo(Range ran){return this->range.isEqualTo(ran);}

	string getRoleName();

	string getParamRoleName(){return this->paramRoleName;}

	void setCurVisitNode(CommNode *node);

	VisitResult* visit();

	CommNode* getCurVisitNode(){return this->curVisitNode;}

	bool IsBlocked(){return this->blocked;}

	void unblock(){this->blocked=false;}

	bool hasFinished(){return finished;}

};

class ParamRole{
private:
	//the paramRole name is the communicator group name
	string paramRoleName;

	vector<Role*> *actualRoles;


public:

	ParamRole();

	ParamRole(Condition cond);

	~ParamRole(){delete actualRoles;}

	string getFullParamRoleName();

	bool hasARoleSatisfiesRange(Range ran);

	void addAllTheRangesInTheCondition(Condition cond);

	vector<Role*>* getTheRoles(){return this->actualRoles;}

	void insertActualRole(Role *r, bool forceUpdate);

};


////////////////////////////////////////////////////////////////////////////
class MPIOperation{
private:
	int opType;
	string dataType;
	Condition executor;
	Condition target;
	Expr *targetExpr;
	string targetExprStr;
	string tag;
	string group;
	string opName;

	static set<string> blockingOPSet;
	static set<string> sendingOPSet;
	static set<string> recvingOPSet;
	static set<string> collectiveOPSet;


public:
	CommNode *theNode;
	string execExprStr;
	string reqVarName;
	WaitNode *theWaitNode; 
	string operatorStr;
	string srcCode;
	bool isInPendingList;
	bool isTargetDependOnExecutor;
	bool isBothCastAndGather;


	MPIOperation(string opName0,int opType0, string dataType0,Condition executor0, Condition target0, string tag0, string group0);
	MPIOperation(string opName0,int opType0, string dataType0,Condition executor0, Expr *targetExpr0, string tag0, string group0);

	bool isStable();
	string getOpName(){return this->opName;}
	int getOPType(){return this->opType;}
	string getDataType(){return this->dataType;}
	Condition getSrcCond();
	Condition getDestCond();
	Condition getExecutor();
	Condition getTargetCond();
	Expr* getTargetExpr(){return this->targetExpr;}
	string getTag(){return this->tag;}
	string getGroup(){return this->group;}

	static bool isOpBlocking(string opStr);
	static bool areTheseTwoOpsCompatible(MPIOperation* op1, MPIOperation* op2);
	bool isBlockingOP();
	bool isNonBlockingOPWithReqVar(string reqName);
	bool isDependentOnExecutor();
	void setSrcCode(string srcC){this->srcCode=srcC;}
	void setExecutorCond(Condition execCond){this->executor=execCond.AND(Condition(true));}
	void setTargetCond(Condition target0){this->target=target0.AND(Condition(true));}
	void setTargetExprStr(string str){this->targetExprStr=str;}

	bool isSendingOp();
	bool isRecvingOp();
	bool isCollectiveOp();

	bool isComplementaryOpOf(MPIOperation *otherOP);
	bool isSameKindOfOp(MPIOperation *other);
	bool isTheSameMPIOP(MPIOperation *other);

	bool isUnicast(); // n to n
	bool isMulticast(); // 1 to n
	bool isGatherOp(); // n to 1

	bool isFinished();
	bool isEmptyOP();

	//	void transformToSendingOP();
	string printMPIOP();
	string getTarExprStr();
};



class CommNode{
private:
	//use the session node type here. but the CommTree is not the session tree
	//the CommNode is the node of the CommTree which reflects the charateristics of MPI program.
	int nodeType;

	string nodeName;

	vector<CommNode*> children;

	CommNode *parent;

	vector<MPIOperation*> *ops;

	int depth;

	//the branch id specifies the executable path
	string branchID;

	bool marked;

	bool mpiOPIsForbidden;

	bool isRelatedToRank;

	void init(int type,vector<MPIOperation*> *ops);

	void deleteItsTree();

	CommNode *sibling;

	string srcCodeInfo;

	//////////////////////////////////
	//A node is master if it is the root of a master.
	bool isMasterNode;

protected:
	//the pos number indicates the time of insertion, the smaller the earlier
	int posIndex;

	//the role visitor will check whether it can 
	//satisfy the condition in the node.
	//if the condition is satisfied, then it will goto visit the 
	//first child of the cur node.
	//else it will skip to the next sibling.
	//If the condition in the src code does not contain a rank related var,
	//then the condition here will be a condition whose "complete" field is true
	Condition condition;

public:
	Expr* execExpr;
	Condition rawCond;

	//construct an intermediate node
	CommNode(int type, Condition cond);

	//construct a leaf; 
	CommNode(MPIOperation *op0);

	~CommNode(){
		delete ops;
	}

	bool isStructureNode();

	Expr* getExecExpr();

	bool isLeaf() const;

	bool isMaster(){return this->isMasterNode;}

	bool HasMpiOPBeenForbidden(){return this->mpiOPIsForbidden;}

	void forbidTheMPIOP(){this->mpiOPIsForbidden=true;}

	bool isNegligible();

	bool isBelowARankSpecificForLoop();

	MPIOperation* findTheClosestNonblockingOPWithReqName(string reqName);

	vector<ForEachNode*> getAllTheSurroundingRankSpecificForLoops();

	void reportFinished(Condition executor);

	void setAsRelatedToRank(){this->isRelatedToRank=true;}

	void initTheBranchId();

	void setMaster(){this->isMasterNode=true;}

	string getBranID(){return this->branchID;}

	int getNodeType(){return this->nodeType;}

	string getNodeName(){return this->nodeName;}

	vector<MPIOperation*>* getOPs(){return this->ops;}

	Condition getCond(){return this->condition;}

	void setCond(Condition cond){this->condition=cond;}

	void setInfo(string info0){this->srcCodeInfo=info0;}

	string getSrcCodeInfo(){return this->srcCodeInfo;}

	void setMarked();

	bool isMarked();

	CommNode* getClosestNonRankAncestor();

	CommNode* getInnerMostRecurNode();

	CommNode* getParent()const{return this->parent;}

	void setNodeType(int nodeType);

	void insert(CommNode *child);

	string getCommInfoAsString();

	int getDepth(){return this->depth;}

	CommNode* goDeeper();//get first child

	CommNode* skipToNextNode();

	string printTheNode();

	int sizeOfTheNode();

	int indexOfTheMPIOP(MPIOperation* op);

	void insertMPIOP(MPIOperation* theOP){this->ops->push_back(theOP);}

	int getPosIndex() const{return this->posIndex;}

	bool isRankRelatedChoice();

	////////////////////////////////////////////////////////////////////////
	//only used after the traversal of AST of the mpi pgm
	bool isNonRankChoiceNode();

	void optimize();
};

class ForEachNode: public CommNode{
private:
	string iterVarName;
	int startingIndex;
	int endingIndex;

public:
	ForEachNode(string varName,int start,int end);

	string getIterVarName(){return this->iterVarName;}
	int getStartingIndex(){return this->startingIndex;}
	int getEndingIndex(){return this->endingIndex;}

};

class ContNode: public CommNode{
private:
	CommNode* refNode;

public:
	string label;
	ContNode(CommNode *node);

	CommNode* getRefNode(){return this->refNode;}
	void setRefNode(CommNode* refNode0){this->refNode=refNode0;}

	void setToLastContNode(){this->posIndex=INT_MAX;}
};

class WaitNode: public CommNode{
public:
	static const int waitNormal=0;
	static const int waitAny=1;
	static const int waitAll=2;

	int type;
	string req;
	int numOfWait;

	WaitNode():CommNode(MPI_Wait,Condition(true)){this->type=waitAny;}
	WaitNode(string req0):CommNode(MPI_Wait,Condition(true)){this->type=waitNormal; this->req=req0;}
	WaitNode(int tmp):CommNode(MPI_Wait,Condition(true)){this->type=waitAll;this->numOfWait=tmp;}

};

class BarrierNode: public CommNode{
private:
	ParamRole *commGroup;

public:
	Condition curArrivedProcCond;

	BarrierNode(ParamRole *group);//the cur pgm only support the barrier of world communicator

	void visit(Condition visitorCond);
};

class CommManager{

private:

	map<string,ParamRole*>  paramRoleNameMapping;

	//mapping between the rank var name and the comm group name 
	map<string,string> rankVarCommGroupMapping;

	map<string,stack<Condition>> nonRankVarAndStackOfCondMapping;

	map<string,stack<Condition>> tmpNonRankVarCondMap;

	CompilerInstance *ci;

	int numOfProc;

	vector<string> varNames;

	map<string,int> rankVarOffsetMapping;

public:

	CommManager(CompilerInstance *ci0, int numOfProc0);

	void insertRankVarAndCommGroupMapping(string rankVar, string commGroup);

	void insertNonRankVarAndCondtion(string nonRankVar, Condition cond);

	void insertTmpNonRankVarCond(string nonRankVar, Condition cond);

	void clearTmpNonRankVarCondStackMap();

	map<string,stack<Condition>> getTmpNonRankVarCondStackMap();

	Condition getTopCond4NonRankVar(string nonRankVar);

	void removeTopCond4NonRankVar(string nonRankVar);

	string getCommGroup4RankVar(string rankVar);

	Condition extractCondFromBoolExpr(Expr *expr);

	Condition extractCondFromTargetExpr(Expr *expr);


	void insertRankVarAndOffset(string varName, int offset);

	int getOffSet4RankRelatedVar(string varName);

	bool isVarRelatedToRank(string varName);

	void cancelRelation(string varName);

	void insertVarName(string varName);

	bool isAVar(string str);

	//test whether a string contains a rank-related var name
	bool containsRankStr(string str);

	bool hasAssociatedWithCondition(string varName);


	map<string,ParamRole*> getParamRoleMapping() const;
	ParamRole* getParamRoleWithName(string name) const;
};

//////////////////////////////////////////////////////////////////////////////
class CollectiveOPManager{
private:
	map<int,Condition> commNodeAndInitExecutorCondMap;
	vector<CommNode*> participatingCommNodesVec;
	MPIOperation *curCollectiveOP;

	int getInitNodePos();
public:
	CollectiveOPManager();
	//if the collective op fires, then relevant nodes will be unblocked
	//The actually happened collective op will be returned if there is any
	MPIOperation* insertCollectiveOP(MPIOperation* op);
	void unlockCollectiveOP();

	string getCurPendingCollectiveOPName();
};


//////////////////////////////////////////////////////////////////////////////
class MPISimulator{
private:

	CommNode *root;
	CommNode *curNode;

	CommManager *commManager;
	CollectiveOPManager collectiveOpMgr;
	MPITree *mpiTree;

	vector<MPIOperation*> pendingOPs;

	map<int, MPINode*> posIndexAndMPINodeMapping;

	vector<MPINode*> listOfContMPINodes;

public:
	MPISimulator(CommManager *commManager, MPITree *theMPITree);
	CommNode* getCurNode(){return this->curNode;}
	void insertNode(CommNode *node);
	void gotoParent();
	Condition getCurExecCond(){return this->curNode->getCond();}

	void forbidMPIOP(CommNode *node);

	void simulate();

	void printTheRoles();

	void initTheRoles();

	void makeItTidy(); //check whether there exists any blocking role can be unblocked

	string printTheTree(){return this->root->printTheNode();}

	bool areAllRolesDone();
	bool isDeadLockDetected();


	Condition getTarCondFromExprAndExecCond(Expr *expr, Condition execCond);
	Condition getExecCondFromExprAndProcCond(Expr *expr, string procVar, Condition procCond);

	void analyzeVisitResult(VisitResult *vr);

	void insertOpToPendingList(MPIOperation *op);

	void insertPosAndMPINodeTuple(int pos, MPINode *mpiNode);

	void insertMPIOpToMPITree(MPIOperation *op);

	void insertToTmpContNodeList(MPINode *contNode);
	void insertAllTheContNodesWithLabel(string loopLabel);

	void unblockTheRolesWithCond(Condition unblockingCond, CommNode *unblockedNode);
};


/////////////////////////////////////////////////////////////////////////////////////////
//The classes used to build MPI tree and protocol 
class MPINode{
private:
	int index;
	int depth;
	int nodeType;
	string labelInfo;
	MPIOperation* op;
	vector<MPINode*> children;
	MPINode *parent;

public:
	bool isInRankSpecificForLoop;
	vector<ForEachNode*> rankSpecificForLoops;


	MPINode(CommNode* node);
	MPINode(MPIOperation* theOp);

	void initMPIOpNode(MPIOperation* op);
	bool isLeaf();
	MPINode* getParent(){return this->parent;}
	int getNodeType(){return this->nodeType;}
	MPIOperation* getMPIOP(){return this->op;}
	void insert(MPINode *child);
	void insertToProperNode(MPINode *node);
	vector<MPINode*> getChildren(){return this->children;}
	string getLabelInfo(){return this->labelInfo;}
	void insertToEndOfChildrenList(MPINode *lastContNode);
	static MPIOperation* combineMPIOPs(MPIOperation* op1, MPIOperation* op2);
};

class MPIForEachNode: public MPINode{
public:
	string iterVarName;
	int startPos;
	int endPos;

	MPIForEachNode(ForEachNode *forEach);
};

class MPITree{
private:
	MPINode *root;


public:
	MPITree(MPINode *rtNode);
	MPINode* getRoot(){return this->root;}
	void insertNode(MPINode* mpiNode);

};


class ProtocolGenerator{
private:
	MPITree *mpiTree;
	map<string,ParamRole*>  paramRoleNameMapping;

	////////////////////////////////////////////////////////////////////////////////////
	//helper methods
	string genRoleName(string paramRoleName, Range ran);
	string insertRankInfoToRangeStr(string rangeInfoStr);
	string genRoleByVar(string paramRoleName, string varName);
	string selfCreatedLoop(MPINode *mpinode);
	////////////////////////////////////////////////////////////////////////////////////
	//The component methods corresponding to the BNF spec
	//The header part
	string protocolName();
	string globalProtocolHeader();
	string localProtocolHeader(string roleName);
	string roleDeclList();
	string roleDecl(string roleName);

	////////////////////////////////////////////////////////////////////////////////////
	//The definition part
	string globalDef();
	string globalInteractionBlock(MPINode *theRoot);
	string globalInteractionSeq(MPINode *theRoot);
	string globalInteraction(MPINode *node);

	string globalMsgTransfer(MPINode *node);
	string globalChoice(MPINode *node);
	string globalRecur(MPINode *node);
	string globalContinue(MPINode *node);
	string globalForeach(MPIForEachNode *node);


	string message(MPIOperation* mpiOP);
	string msgOperator(){return "Data";}
	string payLoad(MPIOperation* mpiOP);

	string getReceiverRoles(MPIOperation *mpiOP, int pos);

	string generateGlobalProtocol();
	string generateLocalProtocol(ParamRole* paramRole);

public:

	ProtocolGenerator(MPITree *tree, map<string,ParamRole*>  paramRoleNameMapping0);

	void generateTheProtocols();

};


#endif