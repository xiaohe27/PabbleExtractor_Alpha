#include "Comm.h"

using namespace llvm;
using namespace clang;
//some functions
int min(int a, int b){if(a<b) return a; else return b;}
int max(int a, int b){if(a<b) return b; else return a;}

int minEnd(int a, int b){
	if(a==InitEndIndex)
		return b;

	if(b==InitEndIndex)
		return a;

	return min(a,b);
}


int maxEnd(int a, int b){
	if(a==InitEndIndex)
		return a;

	if(b==InitEndIndex)
		return b;

	return max(a,b);

}

string convertIntToStr(int number)
{
	stringstream ss;//create a stringstream
	ss << number;//add number to the stream
	return ss.str();//return a string with the contents of the stream
}

string stmt2str(SourceManager *sm, LangOptions lopt,clang::Stmt *stmt) {
	clang::SourceLocation b(stmt->getLocStart()), _e(stmt->getLocEnd());
	clang::SourceLocation e(Lexer::getLocForEndOfToken(_e, 0, *sm, lopt));
	return std::string(sm->getCharacterData(b),
		sm->getCharacterData(e)-sm->getCharacterData(b));

}




/********************************************************************/
//Class Range impl start										****
/********************************************************************/

Range::Range(int s,int e){
	/*
	if(e<0)	{ //the end index should not be less than 0
		this->shouldBeIgnored=true;
		startPos=InvalidIndex;
		endPos=InvalidIndex;

		return;
	}
	*/

	this->shouldBeIgnored=false;
	this->marked=false;

	if(s<0)
	{
		s=0;
	}

	startPos=s; endPos=e;

}


 Range Range::createByStartIndex(int start){

	return Range(start,InitEndIndex);
}

 Range Range::createByEndIndex(int end){

	return Range(0,end);
}


 Range Range::createByOp(string op, int num){
	if(op=="==")
		return Range(num,num);

	if(op=="<"){
		if(num<=0)
			return Range();

		return Range(0,num-1);
	}

	if(op=="<="){
		if(num<0)
			return Range();

		return Range(0,num);
	}

	if(op==">"){	
		return createByStartIndex(num+1);
	}

	if(op==">="){	
		return createByStartIndex(num);
	}

	return Range();

}

Range Range::AND(Range other){
	if(this->isIgnored() || other.isIgnored()){
		return Range();
	}

	if(other.endPos!=InitEndIndex && other.endPos < this->startPos)
		return Range();

	if(this->endPos!=InitEndIndex && this->endPos < other.startPos)
		return Range();


	int newStart=max(this->startPos,other.startPos);
	int newEnd=minEnd(this->endPos,other.endPos);

	return Range(newStart,newEnd);
}

bool Range::hasIntersectionWith(Range other){

	return !this->AND(other).isIgnored();
}

Condition Range::OR(Range other){
	if(this->isIgnored() && other.isIgnored()){
		return Condition(false);
	}

	if(this->isIgnored()){
		return Condition(other);
	}

	if(other.isIgnored()){
		return Condition(*this);
	}

	//the two ranges have no intersection
	if(!this->hasIntersectionWith(other)){
		return Condition(*this,other);
	}

	int newStart=min(this->startPos,other.startPos);
	int newEnd=maxEnd(this->endPos,other.endPos);

	Range ran= Range(newStart,newEnd);

	return Condition(ran);
}

bool Range::isEqualTo(Range ran){
	if(this->isIgnored() && ran.isIgnored())
		return true;

	if(this->startPos == ran.startPos && 
		this->endPos == ran.endPos)
		return true;

	return false;
}

Condition Range::negateOfRange(Range ran){
	if(ran.isIgnored()){
		Range allRange=Range(0,InitEndIndex);
		return Condition(allRange);
	}

	if(ran.isAllRange()){
		return Condition(false);
	}

	if(ran.startPos<=0){
		return Condition(Range::createByStartIndex(ran.endPos+1));
	}

	else{
		return Condition(Range(0,ran.startPos-1),Range::createByStartIndex(ran.endPos+1));
	}
}

string Range::printRangeInfo(){
	if(this->isIgnored()){
		return "[EMPTY]";
	}

	return "["+convertIntToStr(this->startPos)+".."+((this->endPos==InitEndIndex)?"N-1":convertIntToStr(this->endPos))+"]";

}

/********************************************************************/
//Class Range impl end											****
/********************************************************************/



/********************************************************************/
//Class Condition impl start									****
/********************************************************************/

 Condition Condition::createCondByOp(string op, int num){
	if(op!="!="){
		Range ran=Range::createByOp(op,num);
		return Condition(ran);
	}

	else{
		if(num<0)
			return Condition(true);

		if(num==0)
		{
			return Condition(Range::createByStartIndex(1));
		}

		Range ran1=Range(0,num-1);
		Range ran2=Range::createByStartIndex(num+1);

		return Condition(ran1,ran2);
	}

}


void Condition::normalize(){

	for (int i = 0; i < rangeList.size(); i++)
	{
		if(rangeList[i].isIgnored())
			continue;

		for (int j = i+1; j < rangeList.size(); j++)
		{
			if (rangeList[j].isIgnored())
				continue;
			
			if(rangeList[i].hasIntersectionWith(rangeList[j])){
				Condition cond=rangeList[i].OR(rangeList[j]);
				Range combinedRange=cond.rangeList.back();
				rangeList[i]=combinedRange;
				//insert a dummy node to the pos of the deleted node.
				rangeList[j]=Range();

				//after the combination, the cur range may have intersection with the previous node (before node j)
				//so repeat the analysis of node i.
				i--;
				break;
			}
		}
	}

	vector<Range> newRangeList;
	for (int i = 0; i < this->rangeList.size(); i++)
	{
		if(!rangeList[i].isIgnored())
			newRangeList.push_back(rangeList[i]);
	}

	this->rangeList=newRangeList;
}




Condition Condition::AND(Condition other){
	if(this->isIgnored() || other.isIgnored())
		return Condition(false);

	if(this->isComplete()){return other;}

	if(other.isComplete()){return *this;}

	Condition cond; 

	for (int i = 0; i < this->rangeList.size(); i++)
	{
		Range ranI=this->rangeList[i];

		for (int j = 0; j < other.rangeList.size(); j++)
		{
			cond.rangeList.push_back(ranI.AND(other.rangeList[j]));
		}
	}

	cond.normalize();

	return cond;

}

Condition Condition::OR(Condition other){
	if(this->isIgnored() && other.isIgnored())
	return other;

	if(this->isIgnored())
		return other;

	if(other.isIgnored())
		return *this;

	/////////////////////////////////////
	Condition cond;
	for (int i = 0; i < this->rangeList.size(); i++)
	{
		cond.rangeList.push_back(this->rangeList[i]);
	}

	for (int i = 0; i < other.rangeList.size(); i++)
	{
		cond.rangeList.push_back(other.rangeList[i]);
	}

	cond.normalize();

	return cond;
}


Condition Condition::negateCondition(Condition cond){
	if(cond.isIgnored()){
		//negate of nothing becomes a complete condition
		return Condition(true);
	}

	if(cond.isComplete()){
		return Condition(false);	
	}

	//base case: only one range
	if(cond.rangeList.size()==1){
		Range ran=cond.rangeList.back();
		return Range::negateOfRange(ran);
	}

	else{
		Range curRan=cond.rangeList.back();
		cond.rangeList.pop_back();
		return Range::negateOfRange(curRan).AND(Condition::negateCondition(cond));
	}
}

string Condition::printConditionInfo(){
	if(this->isIgnored())
		return "{Empty Condition}";

	if(this->isComplete())
		return "{[0..N-1]}";

	/////////////////////////////////////////////////

	if(this->rangeList.size()==0)
		return "{Empty Condition}";

	string output="{";
	for (int i = 0; i < this->rangeList.size(); i++)
	{
		Range ran=this->rangeList[i];
		output+=ran.printRangeInfo();

		//control the len of the line
		if(i%4==0 && i!=0){
			output+="\n ";
		}

		if(i!=this->rangeList.size()-1)
			output+=", ";
	}

	output+="}";
	return output;
}

/********************************************************************/
//Class Condition impl end											****
/********************************************************************/


/********************************************************************/
//Class CommManager impl start										****
/********************************************************************/

CommManager::CommManager(CompilerInstance *ci0, int numOfProc0):root(ST_NODE_ROOT){
		curNode=&root;
		this->ci=ci0;
		this->numOfProc=numOfProc0;
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

Condition CommManager::extractCondFromExpr(Expr *expr){
	cout<<"The expr "<<stmt2str(&ci->getSourceManager(),ci->getLangOpts(),expr)<<" is obtained by Comm.cpp"<<endl;
	
	cout<<"The stmt class type is "<<expr->getStmtClassName()<<endl;


	bool boolResult;
	bool canBeEval=expr->EvaluateAsBooleanCondition(boolResult,this->ci->getASTContext());
	
	if(canBeEval){
		cout <<"The expr can be evaluated!"<<endl;

		cout<<"The boolResult is "<<boolResult<<endl;

			if(boolResult){
				
				return Condition(true);

			}
			
			else{return Condition(false);}			
		
	}

	else{cout <<"The expr can NOT be evaluated!"<<endl;}

	if(isa<ParenExpr>(expr)){
		ParenExpr *bracketExpr=cast<ParenExpr>(expr);
		Expr *withoutParen=bracketExpr->getSubExpr();
		cout<<"The expr without brackets is "<<stmt2str(&ci->getSourceManager(),ci->getLangOpts(),withoutParen)<<endl;

		return extractCondFromExpr(withoutParen);
	}
	/////////////////////////////////////////////////////////////
	//the expr is a bin op.
	////////////////////////////////////////////////////////////
	if(isa<BinaryOperator>(expr)){
		BinaryOperator *binOP=cast<BinaryOperator>(expr);

		Expr *lhs=binOP->getLHS();
		Expr *rhs=binOP->getRHS();

		string lhsStr=stmt2str(&ci->getSourceManager(),ci->getLangOpts(),lhs);
		string rhsStr=stmt2str(&ci->getSourceManager(),ci->getLangOpts(),rhs);
		string op=binOP->getOpcodeStr();

		cout<<"The bin op is "<<stmt2str(&ci->getSourceManager(),ci->getLangOpts(),binOP)<<"\n";
		cout<<"The lhs is "<<lhsStr<<"\n";
		cout<<"The rhs is "<<rhs<<"\n";
		cout<<"The operator is : "<<op<<endl;

		


		if(op=="&&"){
			Condition andCond=extractCondFromExpr(lhs).AND(extractCondFromExpr(rhs));

			cout <<"\n\n\n\n\n"<< andCond.printConditionInfo()<<"\n\n\n\n\n" <<endl;
			return andCond;
		}

		else if(op=="||"){
			Condition orCond=extractCondFromExpr(lhs).OR(extractCondFromExpr(rhs));

			cout <<"\n\n\n\n\n"<< orCond.printConditionInfo()<<"\n\n\n\n\n" <<endl;
			return orCond;
		}
		////it is a basic op
		else {
			
			bool leftIsVar=false;
			bool rightIsVar=false;
			string lVarName=lhsStr;
			string rVarName=rhsStr;

			
//check if the lhs or rhs are var decl.
////////////////////////////////////////////////////////////////////////////////
			if (DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(lhs)) {
				// It's a reference to a declaration...
				cout<<"//It's a reference to a declaration..."<<endl;

				if (VarDecl *VD = dyn_cast<VarDecl>(DRE->getDecl())) {
				// It's a reference to a variable (a local, function parameter, global, or static data member).
				cout<<"// It's a reference to a variable (a local, function parameter, global, or static data member)."<<endl;

				lVarName=VD->getQualifiedNameAsString();
				std::cout << "LHS is var: " << lVarName << std::endl;
				leftIsVar=true;
				}
			}

			//check if the lhs is a var ref
			else{
				if(this->isAVar(lVarName)){leftIsVar=true;}
			}


			if (DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(rhs)) {
				
				// It's a reference to a declaration...
				cout<<"//It's a reference to a declaration..."<<endl;
				if (VarDecl *VD = dyn_cast<VarDecl>(DRE->getDecl())) {
				// It's a reference to a variable (a local, function parameter, global, or static data member).
				cout<<"// It's a reference to a variable (a local, function parameter, global, or static data member)."<<endl;
				
				rVarName=VD->getQualifiedNameAsString();
				std::cout << "RHS is var: " << rVarName << std::endl;
				rightIsVar=true;
				}
			}

			else{
				if(this->isAVar(rVarName)){rightIsVar=true;}
			}

////////////////////////////////////////////////////////////////////////////////



			if(leftIsVar && rightIsVar){
				cout<<"Both lhs and rhs are var, so, all Range Condition!"<<endl;

				return Condition(true);
			}

			else if(!this->isVarRelatedToRank(lVarName)
				&&  !this->isVarRelatedToRank(rVarName)){
				//if one of the vars is non-rank var, then it might be true
				//so return the "all" range.
				cout<<"Either the lvar or rvar is a non-rank var. So All Range Condition!"<<endl;
				return Condition(true);
			}

			else{
				APSInt Result;

				if(leftIsVar){
					if (rhs->EvaluateAsInt(Result, this->ci->getASTContext())) {
						int num=atoi(Result.toString(10).c_str());
						std::cout << "The cond is created by (" <<op<<","<< num <<")"<< std::endl;

						return Condition::createCondByOp(op,num);
					}
				}

				if(rightIsVar){
					if (lhs->EvaluateAsInt(Result, this->ci->getASTContext())) {
						int num=atoi(Result.toString(10).c_str());
						std::cout << "The cond is created by (" <<op<<","<< num <<")"<< std::endl;

						return Condition::createCondByOp(op,num);
					}
				}
			}

			
			
		}
	}

	return Condition(false);
}



void CommManager::insertCondition(Expr *expr){
		
			Condition cond=this->extractCondFromExpr(expr);

			if(!stackOfRankConditions.empty()){
				Condition top=stackOfRankConditions.back();
				cond=top.AND(cond);
			}
			stackOfRankConditions.push_back(cond);

			cout<<"\n\n\n\n\nThe inner condition is \n"
				<<stackOfRankConditions.back().printConditionInfo()
				<<"\n\n\n\n\n"<<endl;

			CommNode *node=new CommNode(cond);
			curNode->insert(node);
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

Condition CommManager::getTopCondition(){
		if(this->stackOfRankConditions.size()==0){
			return Condition(true);
		}

		else{
			return this->stackOfRankConditions.back();
		}
}

void CommManager::insertExistingCondition(Condition cond){
		this->stackOfRankConditions.push_back(cond);
}

Condition CommManager::popCondition(){
		Condition tmp=stackOfRankConditions.back();	
		this->stackOfRankConditions.pop_back();
		return tmp;
}

void CommManager::gotoParent(){
this->curNode= this->curNode->getParent();
}

/********************************************************************/
//Class CommManager impl end										****
/********************************************************************/


/********************************************************************/
//Class CommNode impl start										****
/********************************************************************/

CommNode::CommNode(int type){
	
	this->nodeType=type;

	if(type==ST_NODE_CHOICE || type==ST_NODE_ROOT){
		//everybody has a chance to see the options
		this->condition=Condition(true);
	}

}

void CommNode::setNodeType(int type){
	this->nodeType=type;
}

bool CommNode::isLeaf(){
		if (children.size()==0)
		{
			return true;
		}

		return false;
}

void CommNode::insert(CommNode *child){
		child->parent=this; 
		
		if(this->children.size()!=0){
			this->children.back()->sibling=child;
		}

		this->children.push_back(child);
	
	}

CommNode* CommNode::goDeeper(){
		if(this->children.size()==0)
		{return getSibling();}

		else{
			return children[0];
		}
	}

	CommNode* CommNode::getSibling(){
		return this->sibling;
	}


/********************************************************************/
//Class CommNode impl end										****
/********************************************************************/