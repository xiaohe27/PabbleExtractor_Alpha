#ifndef Comm_H
#define Comm_H

#include <iostream>
#include "include/st_node.h"
#include <vector>
#include <stack>
#include <map>
#include <string>
#include <sstream>

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
//the init end index is N
#define InitEndIndex -1
#define InvalidIndex -3

using namespace std;
using namespace clang;

string convertIntToStr(int number);

string stmt2str(SourceManager *sm, LangOptions lopt,clang::Stmt *stmt);

bool evalIntCmpTruth(int arg1, int arg2, string op);




class CommNode;
class Condition;

class Range{
private:
	int startPos;
	int endPos;
	bool marked;
	bool shouldBeIgnored;

	int startOffset;
	int endOffset;

public:
	

	int getStart(){return startPos;}
	int getEnd(){return endPos;}

	Range(){shouldBeIgnored=true;
			marked=false;
			startPos=InitStartIndex;
			endPos=InitEndIndex;

			init();	
	}
	
	Range(int s,int e);

	void init(){
		startOffset=0;
		endOffset=0;
	}

	bool isIgnored(){return shouldBeIgnored;}

	bool isAllRange();

	bool isThisNumInside(int num);

	bool isSpecialRange(){return endPos < startPos;}

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

	string printRangeInfo();
};

class Condition{
private:
	vector<Range> rangeList;
	bool shouldBeIgnored;
	bool complete;

public:
	Condition(){this->shouldBeIgnored=false; this->complete=false;}

	Condition(bool val){
		rangeList.clear();

		if(val){
			
			this->complete=true;
			this->shouldBeIgnored=false;
			this->rangeList.push_back(Range(0,InitEndIndex));
		}

		else{
			this->shouldBeIgnored=true;
			this->complete=false;
		}
	}

	vector<Range> getRangeList(){return this->rangeList;}

	bool isIgnored(){
		if(this->isComplete())
			return false;

		return shouldBeIgnored || this->rangeList.size()==0;
	}

	bool isComplete(){return this->complete;}

	Condition(Range ran){
		this->shouldBeIgnored=false; this->complete=false;
		this->rangeList.clear(); this->rangeList.push_back(ran);
	}

	Condition(Range ran1, Range ran2){
	this->shouldBeIgnored=false; this->complete=false;
	rangeList.clear(); rangeList.push_back(ran1);rangeList.push_back(ran2);
	}

	static Condition negateCondition(Condition cond);

	static Condition createCondByOp(string op, int num);

	void normalize();

	bool isRangeConsecutive(){return this->rangeList.size()==1;}

//	vector<Range> getRangeList(){return rangeList;}

	Condition AND(Condition other);
	
	Condition OR(Condition other);

	Condition addANumber(int num);

	string printConditionInfo();
};




class Role{
private:
	Range range;


	//the naming convention is 
	//ParamRoleName[startIndex..endIndex]
	string roleName;

	CommNode *curVisitNode;

public:
	Role(Range ran){range=ran;}

	Range getRange(){return this->range;}

	bool hasRangeEqualTo(Range ran){return this->range.isEqualTo(ran);}

	string getRoleName(){
	string name="world[";
	name.append(convertIntToStr(this->range.getStart()));
	name.append("..");
	name.append(convertIntToStr(this->range.getEnd()));
	name.append("]");

	return name;
}


};

class ParamRole{
private:
	
	int size;
	//the paramRole name is the communicator group name
	string paramRoleName;
	
	st_tree localSessionTree;
	
	vector<Role*> actualRoles;

	void insertActualRole(Role *r){actualRoles.push_back(r);}

public:
	ParamRole(){}
	ParamRole(string name){this->paramRoleName=name;}
	

	bool hasARoleSatisfiesRange(Range ran){
		for (int i = 0; i < actualRoles.size(); i++)
		{
			Role* r=actualRoles[i];
			if(r->hasRangeEqualTo(ran))
				return true;
		}
	
		return false;
	}

	void addAllTheRangesInTheCondition(Condition cond){
		vector<Range> ranList=cond.getRangeList();
		for (int i = 0; i < ranList.size(); i++)
		{
			Range r=ranList[i];
			if(this->hasARoleSatisfiesRange(r))
				continue;

			else
				this->insertActualRole(new Role(r));
			
		}
	}
	
};


////////////////////////////////////////////////////////////////////////////
class MPIOperation{
private:
	int opType;
	string dataType;
	
	Condition src;

	Condition dest;
	string tag;
	string group;

public:
	MPIOperation(int opType0, string dataType0,Condition src0, Condition dest0, string tag0, string group0){
	this->opType=opType0;
	this->dataType=dataType0;
	this->src=src0;
	this->dest=dest0;
	this->tag=tag0;
	this->group=group0;
	}

	int getOPType(){return this->opType;}

};


class CommNode{
private:
	//use the session node type here. but the CommTree is not the session tree
	//the CommNode is the node of the CommTree which reflects the charateristics of MPI program.
	int nodeType;

	string nodeName;

	vector<CommNode*> children;

	CommNode *parent;

	MPIOperation *op;

	int depth;

	void init(int type,MPIOperation *op0);

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
	delete op;
	delete sibling;
	}

	int getNodeType(){return this->nodeType;}

	bool isLeaf();

	Condition getCond(){return this->condition;}

	void setCond(Condition cond){this->condition=cond;}

	void setInfo(string info0){this->info=info0;}

	CommNode* getParent(){return this->parent;}

	void setNodeType(int nodeType);

	void insert(CommNode *child);

	string getCommInfoAsString();

	CommNode* goDeeper();

	CommNode* getSibling();

	string printTheNode();

};




class CommManager{

private:
	vector<Condition> stackOfRankConditions;
//	RoleManager roleManager;
	CommNode root;
	

	CompilerInstance *ci;

	int numOfProc;

	map<string,int> rankVarOffsetMapping;

	vector<string> varNames;

	CommNode *curNode;


public:
	
	CommManager(CompilerInstance *ci0, int numOfProc0);

	Condition extractCondFromExpr(Expr *expr);

	void insertCondition(Expr *expr);

	void insertNode(CommNode *node);

	Condition popCondition();

	void gotoParent();

	void insertExistingCondition(Condition cond);
	
	void addCommActions(MPIOperation op);

	void insertRankVarAndOffset(string varName, int offset);

	bool isVarRelatedToRank(string varName);

	void cancelRelation(string varName);

	void insertVarName(string varName);

	bool isAVar(string str);

	Condition getTopCondition();

	string printTheTree(){return this->root.printTheNode();}
};


#endif