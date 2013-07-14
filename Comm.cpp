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
	if(e<0)	{ //the end index should not be less than 0
		this->shouldBeIgnored=true;
		startPos=InvalidIndex;
		endPos=InvalidIndex;

		return;
	}

	if(s<0)
	{
		s=0;
	}

	startPos=s; endPos=e;

}

Range Range::createByStartIndex(int start){
	Range tmp= Range(start,0);
	tmp.endPos=InitEndIndex;

	return tmp;
}

Range Range::createByEndIndex(int end){

	return Range(0,end);
}

Range Range::AND(Range other){
	if(this->shouldBeIgnored || other.shouldBeIgnored){
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

	return !this->AND(other).shouldBeIgnored;
}

Condition Range::OR(Range other){
	if(!this->hasIntersectionWith(other)){
		return Condition(*this,other);
	}

	int newStart=min(this->startPos,other.startPos);
	int newEnd=maxEnd(this->endPos,other.endPos);

	Range ran= Range(newStart,newEnd);

	return Condition(ran);
}

bool Range::isEqualTo(Range ran){
	if(this->shouldBeIgnored && ran.shouldBeIgnored)
		return true;

	if(this->startPos == ran.startPos && 
		this->endPos == ran.endPos)
		return true;

	return false;
}

/********************************************************************/
//Class Range impl end											****
/********************************************************************/



/********************************************************************/
//Class Condition impl start									****
/********************************************************************/

void Condition::normalize(){

	for (int i = 0; i < rangeList.size(); i++)
	{
		if(rangeList[i].shouldBeIgnored)
			continue;

		for (int j = i+1; j < rangeList.size(); j++)
		{
			if (rangeList[j].shouldBeIgnored)
				continue;
			
			if(rangeList[i].hasIntersectionWith(rangeList[j])){
				Condition cond=rangeList[i].OR(rangeList[j]);
				Range combinedRange=*cond.rangeList.begin();
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
		if(!rangeList[i].shouldBeIgnored)
			newRangeList.push_back(rangeList[i]);
	}

	this->rangeList=newRangeList;
}




Condition Condition::AND(Condition other){
	//TO DO
	return Condition();

}

Condition Condition::OR(Condition other){
	//TO DO
	return Condition();

}

/********************************************************************/
//Class Condition impl end											****
/********************************************************************/


/********************************************************************/
//Class CommManager impl start										****
/********************************************************************/

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


	if(isa<BinaryOperator>(expr)){
		BinaryOperator *binOP=cast<BinaryOperator>(expr);

		Expr *lhs=binOP->getLHS();
		Expr *rhs=binOP->getRHS();

		string op=binOP->getOpcodeStr();

		cout<<"The bin op is "<<stmt2str(&ci->getSourceManager(),ci->getLangOpts(),binOP)<<"\n";
		cout<<"The lhs is "<<stmt2str(&ci->getSourceManager(),ci->getLangOpts(),lhs)<<"\n";
		cout<<"The rhs is "<<stmt2str(&ci->getSourceManager(),ci->getLangOpts(),rhs)<<"\n";
		cout<<"The operator is : "<<op<<endl;

		


		if(op=="&&")
			return extractCondFromExpr(lhs).AND(extractCondFromExpr(rhs));

		else if(op=="||")
			return extractCondFromExpr(lhs).OR(extractCondFromExpr(rhs));

		else if(op=="=="){
			
		}
	}

	return Condition();
}



void CommManager::insertCondition(Expr *expr){
		
			Condition cond=this->extractCondFromExpr(expr);

			if(!stackOfRankConditions.empty()){
				Condition top=stackOfRankConditions.back();
				cond=top.AND(cond);
			}
			stackOfRankConditions.push_back(cond);

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
/********************************************************************/
//Class CommManager impl end										****
/********************************************************************/