#ifndef Comm_H
#define Comm_H

#include <iostream>
#include "include/st_node.h"
#include <vector>
#include <stack>
#include <map>

using namespace std;

class CommNode;


class Property{
private:
	string conditionStr;
	int startIndex;
	int endIndex;

	void analyze();

public:

	Property(string condStr){this->conditionStr=condStr; analyze();}

	int startIndex(){return this->startIndex;}

	int endIndex(){return this->endIndex;}

	string getCondStr(){return this->conditionStr;}

	string getRoleName();

	//the param role name is in the between of beginning and the first underscore.
	string getParamRoleName(){
		string roleName=getRoleName();
		return roleName.substr(0,roleName.find_first_of('_'));
	}
};

class Role{
private:
	Property *prop;


	//the naming convention is 
	//ParamRoleName_startIndex..endIndex
	string roleName;

	CommNode *curVisitNode;

public:
	Role(Property *p){prop=p; this->roleName=prop->getRoleName();}

	Role(string condStr){prop= new Property(condStr); this->roleName=prop->getRoleName();}

	Property* getProp(){return this->prop;}

	~Role(){delete prop; delete curVisitNode;}

};

class ParamRole{
private:
	
	int size;
	string paramRoleName;
	string commGroup;
	st_tree localSessionTree;
	
	vector<Role*> actualRoles;

public:
	ParamRole(string name){ParamRole(name,"world");}
	ParamRole(string name, string groupName){this->paramRoleName=name; this->commGroup=groupName;}

	Role* getActualRoleWhichSatisfies(Property p);
	void insertActualRole(Role *r){actualRoles.push_back(r);}
};



class MPIOperation{
private:
	string opType;
	string dataType;
	Role *from;
	Role *to;
	string tag;

public:
	MPIOperation(string opType0, string dataType0,Role *from0, Role *to0, string tag0){
	this->opType=opType0;
	this->dataType=dataType0;
	this->from=from0;
	this->to=to0;
	this->tag=tag0;
	}

	~MPIOperation(){
	delete from;
	delete to;
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

public:
	//construct an intermediate node
	CommNode(int type){this->nodeType=type;}

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

	void insert(CommNode *child){child->parent=this; this->children.push_back(child);}

	string getCommInfoAsString();

};

class RoleManager{
private:
	//assume all the communications happen in the default WORLD communicator group.
	map<string,ParamRole> paramRoleNameMap;

public:
	void addRole(string cond){

		if(isThereAnyRoleSatisfies(cond)){
			cout<<"There already exists a role for the condition " << &cond<<endl;
			return;
		}

		else{
		Property *p=new Property(cond);
		Role role=Role(p);
		string paramRoleName=p->getParamRoleName();
		
		if(paramRoleNameMap.count(paramRoleName)>0){
		ParamRole pr=paramRoleNameMap.find(paramRoleName)->second;
		pr.insertActualRole(&role);
		}

		else{
			paramRoleNameMap[paramRoleName]=ParamRole(paramRoleName);
			paramRoleNameMap[paramRoleName].insertActualRole(&role);
		}

		}
	}


	Role* getRole(string condition){
		if(!isThereAnyRoleSatisfies(condition)){
			return nullptr;
		}

		Property p=Property(condition);
		string paramRoleName=p.getParamRoleName();
		

		if(paramRoleNameMap.count(paramRoleName)>0){
		ParamRole pr=paramRoleNameMap.find(paramRoleName)->second;
		return pr.getActualRoleWhichSatisfies(p);
		}

		else{
			return nullptr;
		}
	}
	
//	Role* getRole(string condition, string commGroup);
	bool isThereAnyRoleSatisfies(string condition){
		Property p=Property(condition);
		string paramRoleName=p.getParamRoleName();
		

		if(paramRoleNameMap.count(paramRoleName)>0){
		ParamRole pr=paramRoleNameMap.find(paramRoleName)->second;
		Role* theRole= pr.getActualRoleWhichSatisfies(p);

		if (theRole==nullptr)
		{
			return false;
		}

		else
		{return true;}

		}

		else{
			return false;
		}
	
	}
	
};


class CommManager{

private:
	stack<string> stackOfConditions;
	RoleManager roleManager;

public:
	
	void insertCondition(string cond){
		stackOfConditions.push(cond);
		this->roleManager.addRole(cond);
	}
	
	void addCommActions(MPIOperation op);

};


#endif