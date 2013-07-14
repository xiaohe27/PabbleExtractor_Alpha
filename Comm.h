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


class CommNode;
class Condition;

class Range{
private:
	int startPos;
	int endPos;
	bool marked;


public:
	bool shouldBeIgnored;

	int getStart(){return startPos;}
	int getEnd(){return endPos;}

	Range(){shouldBeIgnored=true;}
	Range(int s,int e);
	Range createByStartIndex(int start);
	Range createByEndIndex(int end);
	Range AND(Range other);
	bool hasIntersectionWith(Range other);
	
	Condition OR(Range other);

	bool isEqualTo(Range ran);

	
};

class Condition{
private:
	vector<Range> rangeList;


public:
	Condition(){}
	Condition(Range ran){rangeList.clear(); rangeList.push_back(ran);}

	Condition(Range ran1, Range ran2){
	rangeList.clear(); rangeList.push_back(ran1);rangeList.push_back(ran2);
	}

	void normalize();

	bool isRangeConsecutive(){return this->rangeList.size()==1;}

	vector<Range> getRangeList(){return rangeList;}

	Condition AND(Condition other);
	
	Condition OR(Condition other);
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
	string opType;
	string dataType;
	
	Condition src;

	Condition dest;
	string tag;

public:
	MPIOperation(string opType0, string dataType0,Condition src0, Condition dest0, string tag0){
	this->opType=opType0;
	this->dataType=dataType0;
	this->src=src0;
	this->dest=dest0;
	this->tag=tag0;
	}


};


class CommNode{
private:
	//use the session node type here. but the CommTree is not the session tree
	//the CommNode is the node of the CommTree which reflects the charateristics of MPI program.
	int nodeType;

	vector<CommNode*> children;

	CommNode *parent;

	MPIOperation *op;

	Condition condition;

	string info;

public:
	//construct an intermediate node
	CommNode(int type){this->nodeType=type;}

	CommNode(Condition cond){this->nodeType=ST_NODE_ROOT; this->condition=cond;}

	//construct a leaf
	CommNode(MPIOperation *op0){this->op=op0;}

	~CommNode(){
	delete parent;
	delete op;
	}

	bool isLeaf(){
		if (children.size()==0)
		{
			return true;
		}

		return false;
	}

	void setInfo(string info0){this->info=info0;}

	void insert(CommNode *child){child->parent=this; this->children.push_back(child);}

	string getCommInfoAsString();

};




class CommManager{

private:
	vector<string> stackOfNormalConditions;
	vector<Condition> stackOfRankConditions;
//	RoleManager roleManager;
	CommNode root;
	CommNode *curNode;

	CompilerInstance *ci;

	Condition extractCondFromExpr(Expr *expr);


public:
	CommManager(CompilerInstance *ci0):root(ST_NODE_ROOT),curNode(ST_NODE_ROOT){
		curNode=&root;
		this->ci=ci0;
	}

	void insertCondition(Expr *expr, bool isRelatedToRank){

		if(!isRelatedToRank){
			string normCond=stmt2str(&ci->getSourceManager(),ci->getLangOpts(),expr);
			stackOfNormalConditions.push_back(normCond);

			CommNode *node=new CommNode(ST_NODE_ROOT);
			node->setInfo(normCond);
			curNode->insert(node);
		}

		/////////////////////////////////////////////////////////////////
		else{
		
			Condition cond=this->extractCondFromExpr(expr);

			if(!stackOfRankConditions.empty()){
				Condition top=stackOfRankConditions.back();
				cond=top.AND(cond);
			}
			stackOfRankConditions.push_back(cond);

			CommNode *node=new CommNode(cond);
			curNode->insert(node);
		}
	}

	string retrieveTopNormalCondition(){return stackOfNormalConditions.back();}
	Condition retrieveTopRankCondition(){return stackOfRankConditions.back();}

	
	void addCommActions(MPIOperation op);

};


#endif