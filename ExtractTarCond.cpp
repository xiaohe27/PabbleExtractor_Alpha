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
				Condition cond=this->getTopCond4NonRankVar(exprStr);

				if (unboundVarList.count(exprStr)){
					if(cond.isSameAsCond(Condition(Range(N,N))))
						cond.execStr="N";
					cond.isVolatile=true;
				}

				return cond;
			}

		}

		else if(VarCondMap.count(exprStr)){
			Condition cond2=VarCondMap.at(exprStr);

			if(cond2.isSameAsCond(Condition(Range(N,N))))
				cond2.execStr="N";

			return cond2;
		}

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

		Condition updatedCond=extractCondFromTargetExpr(withoutParen);

		updatedCond.execStr="("+updatedCond.execStr+")";

		return updatedCond;
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
		cout<<"The rhs is "<<rhsStr<<"\n";
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

			if(lcond.size()==1 && !lcond.isVolatile)
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
			if(rcond.size()==1 && !rcond.isVolatile)
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
				Condition lcond=this->extractCondFromTargetExpr(lhs);
				Condition plusCond=lcond.addANumber(rhsNum);
				string rStr=rhsNum==N?"N":convertIntToStr(rhsNum);
				plusCond.execStr=lcond.execStr + "+" + rStr;
				return plusCond;
			}

			if(lhsIsNum){
				Condition rcond=this->extractCondFromTargetExpr(rhs);
				Condition plusCond=rcond.addANumber(lhsNum);
				string lStr=lhsNum==N?"N":convertIntToStr(lhsNum);
				plusCond.execStr=rcond.execStr + "+" + lStr;
				return plusCond;
			}

			//			throw new MPI_TypeChecking_Error(errInfo3);
			Condition lcond=this->extractCondFromTargetExpr(lhs);
			Condition rcond=this->extractCondFromTargetExpr(rhs);

			bool volatileV= (lcond.isVolatile || rcond.isVolatile);

			if(lcond.size()==1){
				int leftNum=lcond.getRangeList().at(0).getStart();
				Condition plusCond=rcond.addANumber(leftNum);
				string signOP=leftNum <=0 ? "" : "+";
				string lNumStr= leftNum==0 ? "" : convertIntToStr(leftNum);

				if(leftNum==N)
					lNumStr="N";

				plusCond.execStr=rcond.execStr + signOP + lNumStr;
				return plusCond.setVolatile(volatileV);
			}

			if(rcond.size()==1){
				int rightNum=rcond.getRangeList().at(0).getStart();
				Condition plusCond=lcond.addANumber(rightNum);
				string signOP=rightNum <=0 ? "" : "+";
				string rNumStr= rightNum==0 ? "" : convertIntToStr(rightNum);

				if(rightNum==N)
					rNumStr="N";
				plusCond.execStr=lcond.execStr + signOP + rNumStr;
				return plusCond.setVolatile(volatileV);
			}

			throw new MPI_TypeChecking_Error(errInfo3);
		}

		// minus
		else if(op=="-"){
			if(rhsIsNum){
				Condition lcond=this->extractCondFromTargetExpr(lhs);
				Condition plusCond=lcond.addANumber(-rhsNum);
				plusCond.execStr=lcond.execStr + "-" + convertIntToStr(rhsNum);
				return plusCond;
			}

			Condition lcond=this->extractCondFromTargetExpr(lhs);
			Condition rcond=this->extractCondFromTargetExpr(rhs);
			bool volatileV= (lcond.isVolatile || rcond.isVolatile);


			if(rcond.size()==1){
				int rightNum=rcond.getRangeList().at(0).getStart();
				Condition plusCond=lcond.addANumber(rightNum);
				string signOP=rightNum <0 ? "+" : "";
				string rNumStr= rightNum==0 ? "" : convertIntToStr(-rightNum);
				plusCond.execStr=lcond.execStr + signOP + rNumStr;
				return plusCond.setVolatile(volatileV);
			}

			throw new MPI_TypeChecking_Error(errInfo4);
		}




		else if(op=="%"){
			Condition lhsCond=this->extractCondFromTargetExpr(lhs);
			Condition rhsCond=this->extractCondFromTargetExpr(rhs);
			rhsIsNum= rhsCond.size()==1;

			if(!rhsIsNum)
				throw new MPI_TypeChecking_Error("The op % needs to have a number in the rhs.");

			rhsNum=rhsCond.getRangeList().at(0).getStart();

			if(rhsNum!=N)
				throw new MPI_TypeChecking_Error("Cur system only support the %N...");

			Condition newCond(false);
			for (int i = 0; i < lhsCond.getRangeList().size(); i++)
			{
				Range ranI=lhsCond.getRangeList().at(i);
				newCond=newCond.OR(Condition(Range(ranI.getStart() % N, ranI.getEnd() % N)));
			}

			newCond.isVolatile=true;
			newCond.execStr=lhsCond.execStr+"%N";
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

			Condition resultCond=Condition(Range(quotient,quotient));

			if(lhsCond.isVolatile || rhsCond.isVolatile)
				resultCond.isVolatile=true;


			resultCond.execStr=lhsCond.execStr+"/"+convertIntToStr(divisor); //need to revise
			return resultCond;
		}

		throw new MPI_TypeChecking_Error(errInfo);

	}

	//"Cannot find a proper condition for the target."
	return Condition(false);
}