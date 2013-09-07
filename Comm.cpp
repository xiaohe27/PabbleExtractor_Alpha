#include "Comm.h"



int N=100;
string RANKVAR;
map<string,Condition> VarCondMap;
string MPI_FILE_NAME="MPIProtocol";

/////////////////////////////////////
int LargestKnownRank=0; 
bool IsGenericProtocol=true;
set<string> unboundVarList;

void updateLargestKnownRank(int num){
if(num > LargestKnownRank)
	LargestKnownRank=num;
}

int getLFP(){return LargestKnownRank + 2;}

bool IsProtocolStable(){
	return IsGenericProtocol && N >= getLFP();
}
/////////////////////////////////////

//some functions
int min(int a, int b){if(a<b) return a; else return b;}
int max(int a, int b){if(a<b) return b; else return a;}






int compute(string op, int operand1, int operand2){
	if(op=="+")
		return operand1+operand2;

	if(op=="-")
		return operand1-operand2;

	if(op=="*")
		return operand1*operand2;

	if(op=="/")
		return operand1/operand2;

	if(op=="%")
		return operand1%operand2;

	if(op=="<<")
		return operand1<<operand2;

	if(op==">>")
		return operand1>>operand2;
	
	return -1;
}

bool areTheseTwoNumsAdjacent(int a, int b){
	if(a==b || a==b+1 || a==b-1) return true;

	if(a==0 && b==N-1) return true;

	if(a==N-1 && b==0) return true;

	if(a%N==(b-1+N)%N ||
		a%N==(b+1)%N)
		return true;

	return false;
}

string convertIntToStr(int number)
{
	stringstream ss;//create a stringstream
	ss << number;//add number to the stream
	return ss.str();//return a string with the contents of the stream
}

string convertDoubleToStr(double number)
{
	stringstream ss;//create a stringstream
	ss << number;//add number to the stream
	return ss.str();//return a string with the contents of the stream
}

void writeToFile(string content){
	ofstream outputFile("Debug.txt",
		ios_base::out | ios_base::app);

	outputFile <<"\n"<< content <<"\n";

}

void writeProtocol(string protocol){
	ofstream outputFile("Protocol.txt",
		ios_base::out | ios_base::trunc);

	outputFile <<"\n"<< protocol <<"\n";

	outputFile.close();
}


/********************************************************************/
//Class CommManager impl start										****
/********************************************************************/


CommManager::CommManager(CompilerInstance *ci0, int numOfProc0){

	this->ci=ci0;
	this->numOfProc=numOfProc0;
	this->paramRoleNameMapping[WORLD]=new ParamRole();

}

void CommManager::insertVarName(string name){
	this->varNames.push_back(name);
}

bool CommManager::isAVar(string name){
	for (int i = 0; i < this->varNames.size(); i++)
	{
		if(varNames[i]==name)
			return true;
	}

	return false;
}

//test whether a string contains a rank-related var name
bool CommManager::containsRankStr(string str){

	for (auto &x:this->rankVarOffsetMapping)
	{
		string rankRelatedVarName=x.first;

		size_t found = str.find(rankRelatedVarName);
		if (found!=string::npos){
			return true;
		}
	}

	return false;
}








void CommManager::insertRankVarAndOffset(string varName, int offset){
	if(this->isVarRelatedToRank(varName)){
		this->rankVarOffsetMapping.find(varName)->second=offset;
	}

	else{
		this->rankVarOffsetMapping[varName]=offset;
	}

}

void CommManager::cancelRelation(string varName){
	this->rankVarOffsetMapping.erase(this->rankVarOffsetMapping.find(varName));
}


bool CommManager::isVarRelatedToRank(string varName){
	if(this->rankVarOffsetMapping.count(varName)>0)
		return true;

	else
		return false;

}



void CommManager::insertRankVarAndCommGroupMapping(string rankVar0, string commGroup){
	this->rankVarCommGroupMapping[rankVar0]=commGroup;
}

string CommManager::getCommGroup4RankVar(string rankVar0){
	return this->rankVarCommGroupMapping[rankVar0];
}

void CommManager::insertNonRankVarAndCondtion(string nonRankVar, Condition cond){
	if(this->nonRankVarAndStackOfCondMapping.count(nonRankVar)>0)
	{
		if (this->nonRankVarAndStackOfCondMapping[nonRankVar].size()>0)
		{
			Condition top=this->getTopCond4NonRankVar(nonRankVar);
			cond=cond.AND(top);
		}

		cond.setNonRankVarName(nonRankVar);
		this->nonRankVarAndStackOfCondMapping[nonRankVar].push(cond);
	}

	else{
		stack<Condition> newStack;
		cond.setNonRankVarName(nonRankVar);
		newStack.push(cond);
		this->nonRankVarAndStackOfCondMapping[nonRankVar]=newStack;
	}

}

void CommManager::insertTmpNonRankVarCond(string nonRankVar, Condition cond){
	if(this->tmpNonRankVarCondMap.count(nonRankVar)>0){
		this->tmpNonRankVarCondMap[nonRankVar].push(cond);
	}

	else{
		stack<Condition> newStack;
		newStack.push(cond);
		this->tmpNonRankVarCondMap[nonRankVar]=newStack;
	}

}


void CommManager::clearTmpNonRankVarCondStackMap(){
	this->tmpNonRankVarCondMap.clear();
}


map<string,stack<Condition>> CommManager::getTmpNonRankVarCondStackMap()
{
	return tmpNonRankVarCondMap;
}


Condition CommManager::getTopCond4NonRankVar(string nonRankVar){
	if(this->nonRankVarAndStackOfCondMapping.count(nonRankVar)>0){
		if(this->nonRankVarAndStackOfCondMapping[nonRankVar].size()>0)
			return this->nonRankVarAndStackOfCondMapping[nonRankVar].top();

		else
			return Condition(false);
	}

	else{return Condition(false);}
}

void CommManager::removeTopCond4NonRankVar(string nonRankVar){
	if(this->nonRankVarAndStackOfCondMapping.count(nonRankVar)>0){
		if(this->nonRankVarAndStackOfCondMapping[nonRankVar].size()>0){
			this->nonRankVarAndStackOfCondMapping[nonRankVar].pop();

			if(this->nonRankVarAndStackOfCondMapping[nonRankVar].size()==0)
				this->nonRankVarAndStackOfCondMapping.erase(nonRankVar);
		}

		else
			throw new MPI_TypeChecking_Error("try to pop from an empty stack of non-rank var condtion");
		//			this->nonRankVarAndStackOfCondMapping.erase(nonRankVar);
	}

	else{
		throw new MPI_TypeChecking_Error("try to pop from a non-existing non-rank var condtion stack");
	}

}




bool CommManager::hasAssociatedWithCondition(string varName){
	if(VarCondMap.count(varName))
		return true;

	if(this->isVarRelatedToRank(varName))
		return true;

	if(this->nonRankVarAndStackOfCondMapping.count(varName)>0){
		if(this->nonRankVarAndStackOfCondMapping[varName].size()>0)
			return true;
	}

	return false;
}


int CommManager::getOffSet4RankRelatedVar(string varName){
	if(this->rankVarOffsetMapping.count(varName)>0){
		return rankVarOffsetMapping[varName];
	}

	else{
		throw new MPI_TypeChecking_Error("The var "+varName+" is not related to rank but the method getOffSet4RankRelatedVar() is called");
	}
}


map<string,ParamRole*> CommManager::getParamRoleMapping() const{
	return this->paramRoleNameMapping;
}

ParamRole* CommManager::getParamRoleWithName(string name) const{

	if (this->paramRoleNameMapping.count(name)>0)
	{
		return this->paramRoleNameMapping.at(name);
	}

	else{
		return nullptr;
	}

}

/********************************************************************/
//Class CommManager impl end										****
/********************************************************************/

