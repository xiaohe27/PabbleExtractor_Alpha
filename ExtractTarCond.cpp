#include "Comm.h"

using namespace llvm;
using namespace clang;




Condition CommManager::extractCondFromTargetExpr(Expr *expr, Condition execCond){
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

		int rankNum=atoi(IntResult.toString(10).c_str());

		cout<<"The rank here is "<<rankNum<<endl;

		return Condition(Range(rankNum,rankNum));
	}

	else{cout <<"The expr can NOT be evaluated as int!"<<endl;}

	//if it is a single var
	if(this->isAVar(exprStr)){
		if(this->isVarRelatedToRank(exprStr))
			return execCond.addANumber(this->getOffSet4RankRelatedVar(exprStr));

		else if(this->nonRankVarAndStackOfCondMapping.count(exprStr)>0){
			if(this->nonRankVarAndStackOfCondMapping[exprStr].size()>0){
				//we have identified the range of the non-rank var 
				return this->getTopCond4NonRankVar(exprStr);
			}

		}

		//the current system does not allow to use unknown non-rank vars to denote target processes.
		throw new MPI_TypeChecking_Error(errInfo2);
	}


	if(isa<ParenExpr>(expr)){
		ParenExpr *bracketExpr=cast<ParenExpr>(expr);
		Expr *withoutParen=bracketExpr->getSubExpr();
		cout<<"The expr without brackets is "<<stmt2str(&ci->getSourceManager(),ci->getLangOpts(),withoutParen)<<endl;

		return extractCondFromTargetExpr(withoutParen,execCond);
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

		/*********************************************************************************************/
		if(op=="+" || op=="-"){
			APSInt INT_Result;
			bool lhsIsNum=false;
			bool rhsIsNum=false;
			int lhsNum=-1;
			int rhsNum=-1;

			if (lhs->EvaluateAsInt(INT_Result, this->ci->getASTContext())) {
				lhsIsNum=true;
				lhsNum=atoi(INT_Result.toString(10).c_str());
			}

			if (rhs->EvaluateAsInt(INT_Result, this->ci->getASTContext())) {
				rhsIsNum=true;
				rhsNum=atoi(INT_Result.toString(10).c_str());
			}

			/////////////////////////////////////////////////////
			if (lhsIsNum && rhsIsNum)
			{
				int rankNum=compute(op,lhsNum,rhsNum);
				return Condition(Range(rankNum,rankNum));
			}


			if(op=="+"){
				if(this->hasAssociatedWithCondition(lhsStr) && rhsIsNum){
					return this->extractCondFromTargetExpr(lhs,execCond).addANumber(rhsNum);
				}



				if(this->hasAssociatedWithCondition(rhsStr) && lhsIsNum){
					return this->extractCondFromTargetExpr(rhs,execCond).addANumber(lhsNum);
				}


				throw new MPI_TypeChecking_Error(errInfo3);
			}

			// minus
			else{
				if(this->hasAssociatedWithCondition(lhsStr) && rhsIsNum){
					return this->extractCondFromTargetExpr(lhs,execCond).addANumber(-rhsNum);
				}

				throw new MPI_TypeChecking_Error(errInfo4);
			}

		}



		throw new MPI_TypeChecking_Error(errInfo);

	}

	throw new MPI_TypeChecking_Error("Cannot find a proper condition for the target.");
}