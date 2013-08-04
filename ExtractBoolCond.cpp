#include "Comm.h"

using namespace llvm;
using namespace clang;





Condition CommManager::extractCondFromBoolExpr(Expr *expr){
	string exprStr=stmt2str(&ci->getSourceManager(),ci->getLangOpts(),expr);
	cout<<"The expr "<<exprStr<<" is obtained by Comm.cpp"<<endl;

	cout<<"The stmt class type is "<<expr->getStmtClassName()<<endl;


	//**************************If the condition is a single item***********************************************
	//if it is a single number
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

	//if it is a single var
	if(this->isAVar(exprStr)){
		//a var can contain any value, so assume true.
		return Condition(true);
	}


	if(isa<ParenExpr>(expr)){
		ParenExpr *bracketExpr=cast<ParenExpr>(expr);
		Expr *withoutParen=bracketExpr->getSubExpr();
		cout<<"The expr without brackets is "<<stmt2str(&ci->getSourceManager(),ci->getLangOpts(),withoutParen)<<endl;

		return extractCondFromBoolExpr(withoutParen);
	}

	//The bool expr is a negation
	if(isa<UnaryOperator>(expr)){
		UnaryOperator *uOP=cast<UnaryOperator>(expr);
		string uopstr=UnaryOperator::getOpcodeStr(uOP->getOpcode());
		Expr *subExpr=uOP->getSubExpr();
		if (uopstr=="!")
		{
			Condition theCond=this->extractCondFromBoolExpr(subExpr);
			string nonRankVarName=theCond.getNonRankVarName();
			if (nonRankVarName!="")
			{
				if(this->tmpNonRankVarCondMap.count(nonRankVarName)){
				Condition tmp=this->tmpNonRankVarCondMap[nonRankVarName].top();
				this->tmpNonRankVarCondMap[nonRankVarName].pop();
				Condition newC=Condition::negateCondition(tmp);
				newC.setNonRankVarName(nonRankVarName);
				this->insertTmpNonRankVarCond(nonRankVarName,newC);
				}

				return Condition(true);
			}
			return Condition::negateCondition(theCond);
		}
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



		///////////////////////////////////////////////////////////////////////////////////////////////////


		if(op=="&&"){
			Condition lCond=extractCondFromBoolExpr(lhs);
			Condition rCond=extractCondFromBoolExpr(rhs);

			//if warning level is high, then forbid the mixture of rank
			//and non-rank condition...
			if (strict && !lCond.hasSameRankNature(rCond))
			{
				string lCondStr=stmt2str(&ci->getSourceManager(),ci->getLangOpts(),lhs);
				string rCondStr=stmt2str(&ci->getSourceManager(),ci->getLangOpts(),rhs);
				string errInfo="\nCondition "+lCondStr+" and Condition "+rCondStr;
				errInfo+=" are not compatible. They need to be both rank related or non-rank related.\n";
				throw new MPI_TypeChecking_Error(errInfo);
			}

			string nonRankVarName="";
			string lhsNonRankVarName=lCond.getNonRankVarName();
			string rhsNonRankVarName=rCond.getNonRankVarName();

			if (lhsNonRankVarName!="" && rhsNonRankVarName!="")
				nonRankVarName=lhsNonRankVarName+"_"+rhsNonRankVarName;

			if(lhsNonRankVarName==rhsNonRankVarName && lhsNonRankVarName!=""){
				nonRankVarName=lhsNonRankVarName;

				Condition tmpCond1=this->tmpNonRankVarCondMap[lhsNonRankVarName].top();
				this->tmpNonRankVarCondMap[lhsNonRankVarName].pop();
				Condition tmpCond2=this->tmpNonRankVarCondMap[lhsNonRankVarName].top();
				this->tmpNonRankVarCondMap[lhsNonRankVarName].pop();

				Condition newTmpCond=tmpCond1.AND(tmpCond2);
				newTmpCond.setNonRankVarName(lhsNonRankVarName);
				this->insertTmpNonRankVarCond(lhsNonRankVarName,newTmpCond);
			}

			if(!lCond.hasSameGroupComparedTo(rCond))
			{
				string errInfo="ERROR: mixture of conditions in Group "+
					lCond.getGroupName()+" AND Group "+rCond.getGroupName();
				throw new MPI_TypeChecking_Error(errInfo);
			}

			Condition andCond=lCond.AND(rCond);

			andCond.setNonRankVarName(nonRankVarName);
			cout <<"\n\n\n\n\n"<< andCond.printConditionInfo()<<"\n\n\n\n\n" <<endl;
			return andCond;
		}

		else if(op=="||"){
			Condition lCond=extractCondFromBoolExpr(lhs);
			Condition rCond=extractCondFromBoolExpr(rhs);

			//if warning level is high, then forbid the mixture of rank
			//and non-rank condition...
			if (strict && !lCond.hasSameRankNature(rCond))
			{
				string lCondStr=stmt2str(&ci->getSourceManager(),ci->getLangOpts(),lhs);
				string rCondStr=stmt2str(&ci->getSourceManager(),ci->getLangOpts(),rhs);
				string errInfo="\nCondition "+lCondStr+" and Condition "+rCondStr;
				errInfo+=" are not compatible. They need to be both rank related or non-rank related.\n";
				throw new MPI_TypeChecking_Error(errInfo);
			}

			string nonRankVarName="";
			string lhsNonRankVarName=lCond.getNonRankVarName();
			string rhsNonRankVarName=rCond.getNonRankVarName();

			if (lhsNonRankVarName!="" && rhsNonRankVarName!="")
				nonRankVarName=lhsNonRankVarName+"_"+rhsNonRankVarName;

			if(lhsNonRankVarName==rhsNonRankVarName && lhsNonRankVarName!=""){
				nonRankVarName=lhsNonRankVarName;

				Condition tmpCond1=this->tmpNonRankVarCondMap[lhsNonRankVarName].top();
				this->tmpNonRankVarCondMap[lhsNonRankVarName].pop();
				Condition tmpCond2=this->tmpNonRankVarCondMap[lhsNonRankVarName].top();
				this->tmpNonRankVarCondMap[lhsNonRankVarName].pop();

				Condition newTmpCond=tmpCond1.OR(tmpCond2);
				newTmpCond.setNonRankVarName(lhsNonRankVarName);		
				this->insertTmpNonRankVarCond(lhsNonRankVarName,newTmpCond);
			}

			if(!lCond.hasSameGroupComparedTo(rCond))
			{
				string errInfo="ERROR: mixture of conditions in Group "+
					lCond.getGroupName()+" AND Group "+rCond.getGroupName();
				throw new MPI_TypeChecking_Error(errInfo);
			}

			Condition orCond=lCond.OR(rCond);

			orCond.setNonRankVarName(nonRankVarName);
			cout <<"\n\n\n\n\n"<< orCond.printConditionInfo()<<"\n\n\n\n\n" <<endl;
			return orCond;
		}
		////it is a basic op
		else {

			//if the cur op is not a comparison op, then return true;
			if(!binOP->isComparisonOp())
				return Condition(true);

			//if either lhs or rhs are bin op, then return true;
			if(isa<BinaryOperator>(lhs) || isa<BinaryOperator>(rhs))
				return Condition(true);


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
					APSInt Result;
					string nonRankVarName="";

					if(leftIsVar){
						nonRankVarName=lVarName;

						if (rhs->EvaluateAsInt(Result, this->ci->getASTContext())) {
							int num=atoi(Result.toString(10).c_str());
							std::cout << "The non-rank cond is created by (" <<op<<","<< num <<")"<< std::endl;

							Condition cond=Condition::createCondByOp(op,num);
							cond.setNonRankVarName(lVarName);
							this->insertTmpNonRankVarCond(lVarName,cond);
						}
					}

					if(rightIsVar){
						nonRankVarName=rVarName;

						if (lhs->EvaluateAsInt(Result, this->ci->getASTContext())) {
							int num=atoi(Result.toString(10).c_str());
							std::cout << "The non-rank cond is created by (" <<op<<","<< num <<")"<< std::endl;

							Condition cond=Condition::createCondByOp(op,num);
							cond.setNonRankVarName(rVarName);
							this->insertTmpNonRankVarCond(rVarName,cond);
						}
					}

					//if one of the vars is non-rank var, then it might be true
					//so return the "all" range.
					cout<<"Either the lvar or rvar is a non-rank var. So All Range Condition!"<<endl;
					//also set the name of the non-rank var
					Condition nonRankCond=Condition(true);
					nonRankCond.setNonRankVarName(nonRankVarName);
					return nonRankCond;
			}

			else{
				APSInt Result;

				if(leftIsVar){
					string commGroupName=this->getCommGroup4RankVar(lVarName);

					if (rhs->EvaluateAsInt(Result, this->ci->getASTContext())) {
						int num=atoi(Result.toString(10).c_str());
						std::cout << "The cond is created by (" <<op<<","<< num <<")"<< std::endl;

						Condition cond=Condition::createCondByOp(op,num);

						if(commGroupName!=WORLD)
							cond.setCommGroup(commGroupName);

						return cond;
					}
				}

				if(rightIsVar){
					string commGroupName=this->getCommGroup4RankVar(rVarName);

					if (lhs->EvaluateAsInt(Result, this->ci->getASTContext())) {
						int num=atoi(Result.toString(10).c_str());
						std::cout << "The cond is created by (" <<op<<","<< num <<")"<< std::endl;

						Condition cond=Condition::createCondByOp(op,num);

						if(commGroupName!=WORLD)
							cond.setCommGroup(commGroupName);

						return cond;
					}
				}
			}			

		}
	}

	return Condition(false);
}