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



#define InitStartIndex -2

extern int InitEndIndex;
extern bool strict;

#define InvalidIndex -3

#define WORLD "MPI_COMM_WORLD"

using namespace std;
using namespace clang;


class MPI_TypeChecking_Error{

public:
	string errInfo;

	MPI_TypeChecking_Error(string err){
		this->errInfo=err;
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


int min(int a, int b);
int max(int a, int b);
int minEnd(int a, int b);
int maxEnd(int a, int b);
int compute(string op, int operand1, int operand2);
bool areTheseTwoNumsAdjacent(int a, int b);


class CommNode;
class Condition;
class MPIOperation;
class Role;

class Range{
private:
	int startPos;
	int endPos;
	bool shouldBeIgnored;

public:

	int getStart(){return startPos;}
	int getEnd(){return endPos;}

	Range();

	Range(int s,int e);

	bool isIgnored(){return shouldBeIgnored;}

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

	string printRangeInfo();
};

class Condition{
private:
	vector<Range> rangeList;
	bool shouldBeIgnored;
	bool complete;
	string groupName;

	//the non-rank var related condition might be important in determining the target of MPI op!
	string nonRankVarName;

public:
	Condition();

	Condition(bool val);

	vector<Range> getRangeList(){return this->rangeList;}

	int size();

	bool isIgnored() const;

	bool isComplete()const{return this->complete;}

	Condition(Range ran);

	Condition(Range ran1, Range ran2);

	static Condition negateCondition(Condition cond);

	static Condition createCondByOp(string op, int num);

	void normalize();

	bool isRangeConsecutive(){return this->rangeList.size()==1;}

	Condition AND(Condition other);

	Condition OR(Condition other);

	Condition Diff(Condition other);

	Condition addANumber(int num);

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

	int size;
	//the paramRole name is the communicator group name
	string paramRoleName;

	st_tree localSessionTree;

	vector<Role*> *actualRoles;


public:

	ParamRole();

	ParamRole(Condition cond);

	~ParamRole(){delete actualRoles;}

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
	bool isTargetDependOnExecutor;
	string srcCode;
	string opName;

	static set<string> blockingOPSet;
	static set<string> sendingOPSet;
	static set<string> recvingOPSet;
	static set<string> collectiveOPSet;


public:
	CommNode *theNode;
	bool isInPendingList;

	MPIOperation(string opName0,int opType0, string dataType0,Condition executor0, Condition target0, string tag0, string group0);
	MPIOperation(string opName0,int opType0, string dataType0,Condition executor0, Expr *targetExpr0, string tag0, string group0);

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
	bool isBlockingOP();
	bool isDependentOnExecutor(){return this->isTargetDependOnExecutor;}
	void setSrcCode(string srcC){this->srcCode=srcC;}
	void setExecutorCond(Condition execCond){this->executor=execCond;}
	void setTargetCond(Condition target0){this->target=target0;}
	void setTargetExprStr(string str){this->targetExprStr=str;}

	bool isSendingOp();
	bool isRecvingOp();
	bool isCollectiveOp();

	bool isComplementaryOpOf(MPIOperation *otherOP);
	bool isSameKindOfOp(MPIOperation *other);

	bool isUnicast(); // n to n
	bool isMulticast(); // 1 to n
	bool isGatherOp(); // n to 1

	bool isFinished();
	bool isEmptyOP();
	string printMPIOP();

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

	//the pos number indicates the time of insertion, the smaller the earlier
	int posIndex;

	bool marked;

	void init(int type,vector<MPIOperation*> *ops);

	//the role visitor will check whether it can 
	//satisfy the condition in the node.
	//if the condition is satisfied, then it will goto visit the 
	//first child of the cur node.
	//else it will skip to the next sibling.
	//If the condition in the src code does not contain a rank related var,
	//then the condition here will be a condition whose "complete" field is true
	Condition condition;

	CommNode *sibling;

	string info;

public:
	//construct an intermediate node
	CommNode(int type);

	//construct a leaf; 
	CommNode(MPIOperation *op0);

	~CommNode(){
		delete parent;
		delete ops;
		delete sibling;
	}

	bool isLeaf() const;

	bool isNegligible();

	int getNodeType(){return this->nodeType;}

	string getNodeName(){return this->nodeName;}

	vector<MPIOperation*>* getOPs(){return this->ops;}

	Condition getCond(){return this->condition;}

	void setCond(Condition cond){this->condition=cond;}

	void setInfo(string info0){this->info=info0;}

	void setMarked();

	bool isMarked();

	CommNode* getClosestNonRankAncestor();

	CommNode* getParent()const{return this->parent;}

	void setNodeType(int nodeType);

	void insert(CommNode *child);

	string getCommInfoAsString();

	CommNode* goDeeper();//get first child

	CommNode* skipToNextNode();

	string printTheNode();

	int sizeOfTheNode();

	int indexOfTheMPIOP(MPIOperation* op);

	void insertMPIOP(MPIOperation* theOP){this->ops->push_back(theOP);}

	int getPosIndex() const{return this->posIndex;}

};

class RecurNode: public CommNode{
private:
	int remainingNumOfIterations;

public:
	RecurNode():CommNode(ST_NODE_RECUR){this->remainingNumOfIterations=-1;}
	RecurNode(int size):CommNode(ST_NODE_RECUR){this->remainingNumOfIterations=size;}
	bool hasKnownNumberOfIterations(){return this->remainingNumOfIterations!=-1;}
	void visitOnce();

};

class ContNode: public CommNode{
private:
	RecurNode* refNode;

public:
	ContNode(RecurNode *node):CommNode(ST_NODE_CONTINUE){this->refNode=node;}

	void visit(){this->refNode->visitOnce();}

	RecurNode* getRefNode(){return this->refNode;}
	void setRefNode(RecurNode* refNode0){this->refNode=refNode0;}
};




class CommManager{

private:

	vector<Condition> stackOfRankConditions;

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

	Condition getNegatedCondFromExpr(Expr *expr);

	void insertCondition(Expr *expr);	

	void simplyInsertCond(Condition cond);

	Condition popCondition();


	void insertExistingCondition(Condition cond);

	void insertRankVarAndOffset(string varName, int offset);

	int getOffSet4RankRelatedVar(string varName);

	bool isVarRelatedToRank(string varName);

	void cancelRelation(string varName);

	void insertVarName(string varName);

	bool isAVar(string str);

	//test whether a string contains a rank-related var name
	bool containsRankStr(string str);

	bool hasAssociatedWithCondition(string varName);

	Condition getTopCondition();

	const map<string,ParamRole*> getParamRoleMapping() const;
	ParamRole* getParamRoleWithName(string name) const;
};

//////////////////////////////////////////////////////////////////////////////
class CollectiveOPManager{
private:
	map<string,Condition> collectiveOPFinishInfoMap;

	map<string,vector<CommNode*>*> participatingCommNodesMap;

	void insertCollectiveOPAndCondPair(string opName,int rank, Condition cond, CommNode* node);

public:
	//if the collective op fires, then relevant nodes will be unblocked
	void insertCollectiveOP(MPIOperation* op);

};


//////////////////////////////////////////////////////////////////////////////
class MPISimulator{
private:

	CommNode *root;
	CommNode *curNode;

	CommManager *commManager;

	CollectiveOPManager collectiveOpMgr;
	vector<MPIOperation*> pendingOPs;

public:
	MPISimulator(CommManager *commManager);
	void insertNode(CommNode *node);
	void gotoParent();

	void simulate();

	void printTheRoles();

	void initTheRoles();

	void makeItTidy(); //check whether there exists any blocking role can be unblocked

	string printTheTree(){return this->root->printTheNode();}

	bool areAllRolesDone();
	bool isDeadLockDetected();

	Condition getTarCondFromExprAndExecCond(Expr *expr, Condition execCond);
	void analyzeVisitResult(VisitResult *vr);

	void insertOpToPendingList(MPIOperation *op);
};

#endif