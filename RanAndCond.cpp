#include "Comm.h"

using namespace llvm;
using namespace clang;

/********************************************************************/
//Class Range impl start										****
/********************************************************************/

Range::Range(int s,int e){

	this->shouldBeIgnored=false;

	if(s<0 || s>=InitEndIndex)
		s=0;

	this->startPos=s; this->endPos=e;

}

Range::Range(){
	shouldBeIgnored=true;

	this->startPos=InitStartIndex;
	this->endPos=InitEndIndex;

}

int Range::size(){
	if (this->startPos <= this->endPos)
	{
		return this->endPos-this->startPos+1;
	}

	else{
		return this->endPos+1 + InitEndIndex-this->startPos;
	}
}

Range Range::createByStartIndex(int start){
	if(start>=InitEndIndex)
		return Range();

	return Range(start,InitEndIndex-1);
}

Range Range::createByEndIndex(int end){
	if (end<0)
		return Range();

	else if(end>=InitEndIndex)
		return Range(0,InitEndIndex-1);

	return Range(0,end);
}


Range Range::createByOp(string op, int num){
	if(op=="=="){
		if(num<0 || num>= InitEndIndex)
			return Range();

		else
			return Range(num,num);
	}

	if(op=="<"){
		if(num<=0)
			return Range();

		else if(num<= InitEndIndex)
			return Range(0,num-1);

		else
			return Range(0,InitEndIndex-1);
	}

	if(op=="<="){
		if(num<0)
			return Range();

		else if(num<InitEndIndex)
			return Range(0,num);

		else
			return Range(0,InitEndIndex-1);
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
	if (!this->isSpecialRange())
	{
		this->startPos+=num;
		this->endPos+=num;

		Condition cond=this->AND(Range(0,InitEndIndex-1));
		if (cond.isIgnored())
		{
			this->shouldBeIgnored=true;
		}

		else{
			*this=cond.getRangeList().at(0);
		}
	}

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
			int newEnd=minEnd(this->getEnd(),other.getEnd());

			return Condition(Range(newStart,newEnd));
	}

	//one is special and the other is not 
	if(this->isSpecialRange()){}
	else{
		//if this is not special but the other is
		Range tmp=Range(*this);
		this->startPos=other.startPos;
		this->endPos=other.endPos;
		this->shouldBeIgnored=other.shouldBeIgnored;

		other=tmp; //exchange the value of this and other range
	}

	if(this->isSuperSetOf(other))
		return Condition(other);

	else{
		if(other.isThisNumInside(this->getEnd())){
			if(other.getEnd() < this->getStart()){
				return Condition(Range(other.getStart(),this->getEnd()));
			}

			else{
				Range ran1=Range(other.getStart(),this->getEnd());
				Range ran2=Range(this->getStart(),other.getEnd());
				return Condition(ran1,ran2);
			}
		}

		else{
			return Condition(Range(this->getStart(), other.getEnd()));
		}

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
		if (areTheseTwoNumsAdjacent(other.getEnd(),this->getStart()))
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
			int newEnd=maxEnd(this->getEnd(),other.getEnd());

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
	if (this->shouldBeIgnored)
		return false;

	if(getStart()==0 && getEnd()==InitEndIndex-1)
		return true;

	if(getEnd()==getStart()-1)
		return true;

	return false;

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
		Range allRange=Range(0,InitEndIndex);
		return Condition(allRange);
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
	int exceed=this->getEnd()-InitEndIndex;
	string sign= exceed>0?"+":"";
	endPosStr="N"+sign+(exceed==0?"":convertIntToStr(exceed));
	return "["+convertIntToStr(this->getStart())+
		".."+((this->getEnd()>=InitEndIndex-1 && this->getEnd()!=this->getStart())?endPosStr:convertIntToStr(this->getEnd()))+"]";

}

/********************************************************************/
//Class Range impl end											****
/********************************************************************/


/********************************************************************/
//Class Condition impl start									****
/********************************************************************/

Condition::Condition(){
	this->shouldBeIgnored=false; this->complete=false;
	this->mixed=false;
	this->groupName=WORLD;
	this->nonRankVarName="";
}

Condition::Condition(bool val){
	this->groupName=WORLD;
	this->nonRankVarName="";
	rangeList.clear();
	this->mixed=false;

	if(val){

		this->complete=true;
		this->shouldBeIgnored=false;
		this->rangeList.push_back(Range(0,InitEndIndex-1));
	}

	else{
		this->shouldBeIgnored=true;
		this->complete=false;
	}
}

Condition::Condition(Range ran){
	this->nonRankVarName="";
	this->shouldBeIgnored=false; 
	this->complete=false;
	this->mixed=false;
	this->groupName=WORLD;
	this->rangeList.clear(); this->rangeList.push_back(ran);

}

Condition::Condition(Range ran1, Range ran2){
	this->nonRankVarName="";
	this->shouldBeIgnored=false; 
	this->complete=false;
	this->mixed=false;
	this->groupName=WORLD;
	rangeList.clear(); rangeList.push_back(ran1);rangeList.push_back(ran2);

}


bool Condition::isIgnored(){
	if(this->isComplete())
		return false;

	return shouldBeIgnored || this->rangeList.size()==0;
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
	if(!this->complete)
		this->normalize();

	return this->complete;
}

void Condition::normalize(){

	for (int i = 0; i < rangeList.size(); i++)
	{
		if(rangeList[i].isIgnored())
			continue;

		if(rangeList[i].isAllRange())
			rangeList[i]=Range(0,InitEndIndex-1);

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

	if(this->isRangeConsecutive() && this->rangeList[0].isAllRange())
	{this->complete=true;}

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
			Condition tmpCond=ranI.AND(other.rangeList[j]);

			for (int k = 0; k < tmpCond.rangeList.size(); k++)
				cond.rangeList.push_back(tmpCond.rangeList[k]);

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

Condition Condition::Diff(Condition other){
	return this->AND(Condition::negateCondition(other));
}


bool Condition::hasSameGroupComparedTo(Condition other){
	if (this->isComplete() || other.isComplete())
		return true;

	if(this->groupName==other.groupName)
		return true;

	return false;
}


Condition Condition::addANumber(int num){
	vector<Range> tmp;

	for (int i = 0; i < this->rangeList.size(); i++)
	{
		if(!this->rangeList[i].isSpecialRange())
			this->rangeList[i].addNumber(num);

		else{
			Range ran1(0,this->rangeList[i].getEnd());
			Range ran2(this->rangeList[i].getStart(),InitEndIndex-1);

			ran1.addNumber(num);
			ran2.addNumber(num);
			tmp.push_back(ran1);
			tmp.push_back(ran2);

			this->rangeList.erase(rangeList.begin()+i);
			i--;
		}
	}

	for (int i = 0; i < tmp.size(); i++)
	{
		this->rangeList.push_back(tmp[i]);
	}

	this->normalize();

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
