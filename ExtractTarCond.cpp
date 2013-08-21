#include "Comm.h"

using namespace llvm;
using namespace clang;


Condition CommManager::extractCondFromTargetExpr(Expr *expr){
	string errInfo="The current system does not support the operators other than + or - when constructing the target of MPI operation";
	string errInfo2="the current system does not allow to use unknown non-rank vars to denote target processes";
	string errInfo3="To represent the rank of a process using plus op, current system only support number plus number or rank-related var plus number";
	string errInfo4="To represent the rank of a process using minus op, current system only support rank-related var minus number";


	string exprStr=stmt2str(&ci->getSourceManager(),ci->getLangOpts(),expr);
	cout<<"The target expr "<<exprStr<<" is obtained by the extract method"<<endl;

	cout<<"The stmt class type is "<<expr->getStmtClassName()<<endl;


	//**************************If the condition is a single item***********************************************
	//if it is a single number
	APSInt IntResult;
	bool canBeEval=expr->EvaluateAsInt(IntResult,ci->getASTContext());

	if(canBeEval){
		cout <<"The expr can be evaluated as int!"<<endl;

		int val=atoi(IntResult.toString(10).c_str());

		cout<<"The val here is "<<val<<endl;

		return Condition(Range(val,val));
	}

	else{cout <<"The expr can NOT be evaluated as int!"<<endl;}

	//if it is a single var
	if(this->isAVar(exprStr)){

		if(this->nonRankVarAndStackOfCondMapping.count(exprStr)>0){
			if(this->nonRankVarAndStackOfCondMapping[exprStr].size()>0){
				//we have identified the range of the non-rank var 
				return this->getTopCond4NonRankVar(exprStr);
			}

		}

		else if(VarCondMap.count(exprStr))
			return VarCondMap.at(exprStr);

		//the current system does not allow to use unknown non-rank vars to denote target processes.
		throw new MPI_TypeChecking_Error(errInfo2);
	}

	if(isa<UnaryOperator>(expr))
	{
		UnaryOperator *uOP=cast<UnaryOperator>(expr);
		string uopstr=UnaryOperator::getOpcodeStr(uOP->getOpcode());
		Expr *subExpr=uOP->getSubExpr();

		if(uopstr=="+")
			return this->extractCondFromTargetExpr(subExpr);

		else{
			throw new MPI_TypeChecking_Error("The only supported unary op is '+', but "+uopstr+" is found.");
		}
	}


	if(isa<ParenExpr>(expr)){
		ParenExpr *bracketExpr=cast<ParenExpr>(expr);
		Expr *withoutParen=bracketExpr->getSubExpr();
		//		cout<<"The expr without brackets is "<<stmt2str(&ci->getSourceManager(),ci->getLangOpts(),withoutParen)<<endl;

		return extractCondFromTargetExpr(withoutParen);
	}


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


		APSInt INT_Result;
		bool lhsIsNum=false;
		bool rhsIsNum=false;
		int lhsNum=-1;
		int rhsNum=-1;

		if (lhs->EvaluateAsInt(INT_Result, this->ci->getASTContext())) {
			lhsIsNum=true;
			lhsNum=atoi(INT_Result.toString(10).c_str());
		} 

		else if(this->nonRankVarAndStackOfCondMapping.count(lhsStr)>0){}

		else if(VarCondMap.count(lhsStr)){
			Condition lcond=VarCondMap.at(lhsStr);
			if(lcond.size()==1)
			{
				lhsIsNum=true;
				lhsNum=lcond.getRangeList().at(0).getStart();
			}
		}

		if (rhs->EvaluateAsInt(INT_Result, this->ci->getASTContext())) {
			rhsIsNum=true;
			rhsNum=atoi(INT_Result.toString(10).c_str());
		}	

		else if(this->nonRankVarAndStackOfCondMapping.count(lhsStr)>0){}

		else if(VarCondMap.count(rhsStr)){
			Condition rcond=VarCondMap.at(rhsStr);
			if(rcond.size()==1)
			{
				rhsIsNum=true;
				rhsNum=rcond.getRangeList().at(0).getStart();
			}
		}

		/////////////////////////////////////////////////////
		if (lhsIsNum && rhsIsNum)
		{
			int rankNum=compute(op,lhsNum,rhsNum);
			return Condition(Range(rankNum,rankNum));
		}

		/*********************************************************************************************/


		if(op=="+"){
			if(rhsIsNum){
				return this->extractCondFromTargetExpr(lhs).addANumber(rhsNum);
			}

			if(lhsIsNum){
				return this->extractCondFromTargetExpr(rhs).addANumber(lhsNum);
			}

			throw new MPI_TypeChecking_Error(errInfo3);
		}

		// minus
		else if(op=="-"){
			if(rhsIsNum){
				return this->extractCondFromTargetExpr(lhs).addANumber(-rhsNum);
			}

			throw new MPI_TypeChecking_Error(errInfo4);
		}




		else if(op=="%"){
			Condition lhsCond=this->extractCondFromTargetExpr(lhs);
			if(!rhsIsNum)
				throw new MPI_TypeChecking_Error("The op % needs to have a number in the rhs.");

			if(rhsNum!=N)
				throw new MPI_TypeChecking_Error("Cur system only support the %N...");

			Condition newCond(false);
			for (int i = 0; i < lhsCond.getRangeList().size(); i++)
			{
				Range ranI=lhsCond.getRangeList().at(i);
				newCond=newCond.OR(Condition(Range(ranI.getStart() % N, ranI.getEnd() % N)));
			}

			return newCond;
		}

		else if(op=="/"){
			Condition lhsCond=this->extractCondFromTargetExpr(lhs);
			Condition rhsCond=this->extractCondFromTargetExpr(rhs);
			if(lhsCond.size()!=1 || rhsCond.size()!=1)
				throw new MPI_TypeChecking_Error("The op / can only be performed on two numbers.");
				
			int dividend=lhsCond.getRangeList().at(0).getStart();
			int divisor=rhsCond.getRangeList().at(0).getStart();
			int quotient=dividend/divisor;
			return Condition(Range(quotient,quotient));

		}

		throw new MPI_TypeChecking_Error(errInfo);

	}

	//"Cannot find a proper condition for the target."
	return Condition(false);
}