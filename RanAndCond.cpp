#include "Comm.h"

using namespace llvm;
using namespace clang;

/********************************************************************/
//Class Range impl start										****
/********************************************************************/

Range::Range(int s,int e){
	this->startPos=s; this->endPos=e;
}

Range::Range(){
	this->startPos=InitEndIndex;
	this->endPos=InitStartIndex;
}

int Range::size(){
	if(this->startPos==InitEndIndex && this->endPos==InitStartIndex)
		return 0;

	if (this->startPos <= this->endPos)
	{
		return this->endPos-this->startPos+1;
	}

	else{
		return this->endPos+1 + N-this->startPos;
	}
}

int Range::getLargestNum(){
	if(this->isSpecialRange())
		return 0;

	return this->startPos;
}

Range Range::createByStartIndex(int start){
	return Range(start,InitEndIndex);
}

Range Range::createByEndIndex(int end){
	return Range(InitStartIndex,end);
}


Range Range::createByOp(string op, int num){
	if(op=="=="){
		return Range(num,num);
	}

	if(op=="<"){
		return Range(InitStartIndex,num-1);
	}

	if(op=="<="){
		return Range(InitStartIndex,num);
	}

	if(op==">"){	
		return createByStartIndex(num+1);
	}

	if(op==">="){	
		return createByStartIndex(num);
	}

	return Range();

}

void Range::addNumber(int num){

	this->startPos+=num;
	this->endPos+=num;

}

//ok. already considered the case of end < start
//considered the offset
Condition Range::AND(Range other){
	if(!this->hasIntersectionWith(other)){
		return Condition(false);
	}

	if( (!this->isSpecialRange() && !other.isSpecialRange())
		|| (this->isSpecialRange() && other.isSpecialRange()) ){
			int newStart=max(this->getStart(),other.getStart());
			int newEnd=min(this->getEnd(),other.getEnd());

			return Condition(Range(newStart,newEnd));
	}

	//one is special and the other is not 

	if(this->isSpecialRange()){
		return (Range(0,this->endPos).AND(other))
			.OR(Range(this->startPos,N-1).AND(other));
	}

	else{
		return (Range(0,other.endPos).AND(*this))
			.OR(Range(other.startPos,N-1).AND(*this));
	}

}


//ok. already considered the case of end < start
bool Range::hasIntersectionWith(Range other){
	if(this->isIgnored() || other.isIgnored())
		return false;

	if(other.isThisNumInside(this->getStart()) ||
		other.isThisNumInside(this->getEnd()) ||
		this->isThisNumInside(other.getStart())||
		this->isThisNumInside(other.getEnd()))
		return true;

	else
		return false;
}


//ok. already considered the case of end < start
Condition Range::OR(Range other){
	if(this->isAllRange() || other.isAllRange()){
		return Condition(true);
	}

	if(this->isIgnored() && other.isIgnored()){
		return Condition(false);
	}

	if(this->isIgnored()){
		return Condition(other);
	}

	if(other.isIgnored()){
		return Condition(*this);
	}

	if(this->isSuperSetOf(other))
		return Condition(*this);

	if(other.isSuperSetOf(*this))
		return Condition(other);

	//the two ranges have no intersection
	if(!this->hasIntersectionWith(other)){
		if(this->size()==1 && other.size()==1){
			int smallOne=min(this->getStart(),other.getStart());
			int largeOne=max(this->getStart(),other.getStart());
			if (areTheseTwoNumsAdjacent(smallOne,largeOne))
				return Condition(Range(smallOne,largeOne));
			else
				return Condition(*this,other);

		}

		else if (areTheseTwoNumsAdjacent(other.getEnd(),this->getStart()))
		{
			if(other.isSpecialRange()){
				if(this->getEnd()>=other.getStart())
					return Condition(true);
			}

			return Condition(Range(other.getStart(),this->getEnd()));
		}

		else if(areTheseTwoNumsAdjacent(this->getEnd(),other.getStart())){
			if(this->isSpecialRange()){
				if(other.getEnd()>=this->getStart())
					return Condition(true);
			}

			return Condition(Range(this->getStart(),other.getEnd()));
		}

		return Condition(*this,other);
	}

	//if the two ranges have intersection
	/*****************************************************************/
	//if both of them are special ranges (end < start)
	if(this->isSpecialRange() && other.isSpecialRange()){
		int newS=min(this->getStart(), other.getStart());
		int newE=max(this->getEnd(), other.getEnd());

		if(newE >= newS)
			return Condition(true);

		else
			return Condition(Range(newS,newE));
	}

	if(!this->isSpecialRange() &&
		!other.isSpecialRange()){

			int newStart=min(this->getStart(),other.getStart());
			int newEnd=max(this->getEnd(),other.getEnd());

			Range ran= Range(newStart,newEnd);

			return Condition(ran);
	}

	//one is special, the other is not special
	if(this->isSpecialRange()){
		if(other.isThisNumInside(this->getEnd())){
			int newS=this->getStart();
			int newE=other.getEnd();

			if(newE >= newS-1)
				return Condition(true);

			else
				return Condition(Range(newS,newE));
		}

		else{
			if(other.isThisNumInside(this->getStart())){
				int newS=other.getStart();
				int newE=this->getEnd();

				return Condition(Range(newS,newE));
			}

			else{
				return Condition(*this, other);
			}
		}

	}

	else{return other.OR(Range(*this));}
}

//ok. already considered the case of end < start
bool Range::isSuperSetOf(Range ran){
	if(this->isAllRange())
		return true;

	if(ran.isIgnored())
		return true;

	if((this->isSpecialRange() && ran.isSpecialRange())
		|| (!this->isSpecialRange() && !ran.isSpecialRange()))
	{
		if(this->getStart() > ran.getStart())
			return false;

		if(this->getEnd() < ran.getEnd())
			return false;

		return true;
	}

	if(this->isSpecialRange()){
		if(ran.getEnd() <= this->getEnd() || ran.getStart() >= this->getStart())
			return true;

		return false;
	}

	if(ran.isSpecialRange()){

		return false;
	}

	return false;
}

//ok. already considered the case of end < start
bool Range::isEqualTo(Range ran){
	if(this->isIgnored() && ran.isIgnored())
		return true;

	if(this->isAllRange() && ran.isAllRange())
		return true;

	if(this->getStart() == ran.getStart() && 
		this->getEnd() == ran.getEnd())
		return true;

	return false;
}

//ok. already considered the case of end < start
bool Range::isAllRange(){
	return this->size()==N;
}

//ok. already considered the case of end < start
bool Range::isThisNumInside(int num){

	if(this->isSpecialRange()){
		if(num >= getStart() || num <= getEnd())
			return true;
	}

	if(num>=getStart() && num<=getEnd())
		return true;

	return false;
}

//ok. already considered the case of end < start
Condition Range::negateOfRange(Range ran){
	if(ran.isIgnored()){
		return Condition(true);
	}

	if(ran.isAllRange()){
		return Condition(false);
	}

	if(ran.getStart()<=0){
		return Condition(Range::createByStartIndex(ran.getEnd()+1));
	}

	else{
		if(ran.isSpecialRange())
			return Condition(Range(ran.getEnd()+1, ran.getStart()-1));

		else
			return Condition(Range(0,ran.getStart()-1),Range::createByStartIndex(ran.getEnd()+1));
	}
}

string Range::printRangeInfo(){
	if(this->isIgnored()){
		return "[EMPTY]";
	}


	string endPosStr="";
	int exceed=this->getEnd()-N;
	string sign= exceed>0?"+":"";
	endPosStr="N"+sign+(exceed==0?"":convertIntToStr(exceed));

	if (this->getEnd()>=N-1 )
	{
		if(this->getEnd()==this->getStart())
			return "["+endPosStr+".."+endPosStr+"]";

		else
			return "["+convertIntToStr(this->getStart())+".."+endPosStr+"]";
	}

	else
		return "["+convertIntToStr(this->getStart())+
		".."+convertIntToStr(this->getEnd())+"]";

}

/********************************************************************/
//Class Range impl end											****
/********************************************************************/


/********************************************************************/
//Class Condition impl start									****
/********************************************************************/

Condition::Condition(){
	this->mixed=false;
	this->isTrivialCond=false;
	this->isVolatile=false;
	this->groupName=WORLD;
	this->nonRankVarName="";
	this->offset=0;
}

Condition::Condition(bool val){
	this->groupName=WORLD;
	this->nonRankVarName="";
	rangeList.clear();
	this->mixed=false;
	this->isTrivialCond=false;
	this->isVolatile=false;
	this->offset=0;

	if(val){
		this->rangeList.push_back(Range(0,N-1));
	}
}

Condition::Condition(Range ran){
	this->nonRankVarName="";
	this->mixed=false;
	this->isTrivialCond=false;
	this->isVolatile=false;
	this->groupName=WORLD;
	this->offset=0;
	this->rangeList.clear(); this->rangeList.push_back(ran);

}

Condition::Condition(Range ran1, Range ran2){
	this->nonRankVarName="";
	this->mixed=false;
	this->isTrivialCond=false;
	this->isVolatile=false;
	this->groupName=WORLD;
	this->offset=0;
	rangeList.clear(); 
	rangeList.push_back(ran1);
	rangeList.push_back(ran2);
}


bool Condition::isIgnored(){
	return this->size()==0;
}

bool Condition::isRangeInside(Range ran){
	for (int i = 0; i < this->getRangeList().size(); i++)
	{
		if (getRangeList()[i].isEqualTo(ran))
		{
			return true;
		}
	}

	return false;
}

int Condition::getLargestNum(){
	if(this->isVolatile)
		return 0;

	int curL=INT_MIN;
	for (int i = 0; i < this->rangeList.size(); i++)
	{
		Range ran=this->rangeList.at(i);
		int largestInRan=ran.getLargestNum();
		
		if(largestInRan > curL)
			curL=largestInRan;
	}

	return curL;
}


int Condition::getDistBetweenTwoCond(Condition cond1,Condition cond2){
	if (cond1.isIgnored() || cond2.isIgnored())
		return -1;

	Range ran1=cond1.getRangeList().at(0);
	Range ran2=cond2.getRangeList().at(0);
	return ran2.getStart()-ran1.getStart();
}

bool Condition::areTheseTwoCondAdjacent(Condition cond1, Condition cond2){
	if (cond1.AND(cond2).isIgnored())
	{
		Condition orCond=cond1.OR(cond2);
		if (orCond.rangeList.size()==1)
		{
			return true;
		}
	}

	return false;
}

int Condition::size(){
	int sum=0;

	for (int i = 0; i < this->getRangeList().size(); i++)
	{
		sum+=this->getRangeList()[i].size();
	}

	return sum;
}

bool Condition::isSameAsCond(Condition other){
	this->normalize();
	other.normalize();

	int thisSize=this->getRangeList().size();
	int otherSize=other.getRangeList().size();

	if (thisSize!=otherSize)
	{
		return false;
	}

	for (int i = 0; i < thisSize; i++)
	{
		if (!other.isRangeInside(this->getRangeList()[i]) ||
			!this->isRangeInside(other.getRangeList()[i]))
		{
			return false;
		}
	}

	return true;
}

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

bool Condition::isComplete()
{
	if(this->rangeList.size()==1)
	{
		Range ran0=getRangeList().at(0);
		int start=ran0.getStart();
		int end=ran0.getEnd();
		
		if(start==0 && end==N-1)
			return true;

		if(start==end+1)
			return true;

		return false;
	}


	return this->size()==N;
}

//need to revise
void Condition::normalize(){
	if(this->isComplete())
	{
		this->rangeList.clear();
		this->rangeList.push_back(Range(0,N-1));
		return;
	}

	for (int i = 0; i < rangeList.size(); i++)
	{
		if(rangeList[i].isIgnored())
			continue;

		for (int j = i+1; j < rangeList.size(); j++)
		{
			if (rangeList[j].isIgnored())
				continue;


			Condition cond=rangeList[i].OR(rangeList[j]);
			if(cond.rangeList.size()==1){
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
	bool isChangeable=false;
	if(this->isVolatile || other.isVolatile)
		isChangeable=true;

	if(this->isIgnored() || other.isIgnored())
		return Condition(false).setVolatile(isChangeable);

	if(this->isComplete()){
		Condition cond= other.setVolatile(isChangeable);
		other.execStr=this->execStr;
		return cond;
	}

//	if(other.isComplete()){return (*this).setVolatile(isChangeable);}

	Condition cond(*this); 
	cond.rangeList.clear();

	for (int i = 0; i < this->rangeList.size(); i++)
	{
		Range ranI=this->rangeList[i];

		for (int j = 0; j < other.rangeList.size(); j++)
		{
			Condition tmpCond=ranI.AND(other.rangeList[j]);

			for (int k = 0; k < tmpCond.rangeList.size(); k++)
				cond.rangeList.push_back(tmpCond.rangeList[k]);

		}
	}

	cond.normalize();

	cond.offset=this->offset;
	return cond.setVolatile(isChangeable);

}

Condition Condition::OR(Condition other){
	bool isChangeable=false;
	if(this->isVolatile || other.isVolatile)
		isChangeable=true;

	if(this->isIgnored() && other.isIgnored())
		return other.setVolatile(isChangeable);

	if(this->isIgnored())
		return other.setVolatile(isChangeable);

	if(other.isIgnored())
		return (*this).setVolatile(isChangeable);

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

	cond.offset=this->offset;
	return cond.setVolatile(isChangeable);
}


Condition Condition::negateCondition(Condition cond){

	if(cond.isIgnored()){
		//negate of nothing becomes a complete condition
		return Condition(true).setVolatile(cond.isVolatile);
	}

	if(cond.isComplete()){
		return Condition(false).setVolatile(cond.isVolatile);	
	}

	//base case: only one range
	if(cond.rangeList.size()==1){
		Range ran=cond.rangeList.back();
		return Range::negateOfRange(ran).setVolatile(cond.isVolatile);
	}

	else{
		Range curRan=cond.rangeList.back();
		cond.rangeList.pop_back();
		return Range::negateOfRange(curRan).AND(Condition::negateCondition(cond));
	}
}

Condition Condition::Diff(Condition other){
	return this->AND(Condition::negateCondition(Condition(other)));
}


bool Condition::hasSameGroupComparedTo(Condition other){
	if (this->isComplete() || other.isComplete())
		return true;

	if(this->groupName==other.groupName)
		return true;

	return false;
}


Condition Condition::addANumber(int num){
	this->offset+=num;

	for (int i = 0; i < this->rangeList.size(); i++)
	{
		this->rangeList.at(i).addNumber(num);
	}

	return *this;
}


bool Condition::isRankInside(int rankNum){
	for (int i = 0; i < this->rangeList.size(); i++)
	{
		if(this->rangeList[i].isThisNumInside(rankNum))
			return true;
	}

	return false;
}

Range Condition::getTheRangeContainingTheNum(int num){
	for (int i = 0; i < this->rangeList.size(); i++)
	{
		if(this->rangeList[i].isThisNumInside(num))
			return rangeList[i];
	}

	return Range();
}


bool Condition::hasSameRankNature(Condition other){
	if (this->isRelatedToRank())
	{
		return other.isRelatedToRank();
	}

	else{
		return !other.isRelatedToRank();
	}
}


string Condition::printConditionInfo(){
	if(this->isIgnored())
		return "{Empty Condition}";


	/////////////////////////////////////////////////

	if(this->rangeList.size()==0)
		return "{Empty Condition}";

	string output="{";
	for (int i = 0; i < this->rangeList.size(); i++)
	{
		Range ran=this->rangeList[i];
		output+=ran.printRangeInfo();

		/*control the len of the line
		if(i%4==0 && i!=0){
		output+="\n ";
		}
		*/

		if(i!=this->rangeList.size()-1)
			output+=", ";
	}

	output+="}";
	return output;
}

/********************************************************************/
//Class Condition impl end											****
/********************************************************************/
