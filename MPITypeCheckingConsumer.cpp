#include "MPITypeCheckingConsumer.h"



using namespace clang;
using namespace std;
using namespace llvm;

int labelIndex=0;

MPITypeCheckingConsumer::MPITypeCheckingConsumer(CompilerInstance *ci, int numOfProc){
	init(ci,numOfProc);
}

MPITypeCheckingConsumer::MPITypeCheckingConsumer(CompilerInstance *ci) {
	init(ci, N);
}

void MPITypeCheckingConsumer::init(CompilerInstance *ci, int numOfProc){
	this->ci=ci;
	this->visitStart=false;
	this->numOfProcs=numOfProc;
	this->setOfWorldCommGroup.insert(WORLD);
	this->commManager=new CommManager(ci, numOfProc);
	this->mpiTree=new MPITree(new MPINode(new CommNode(ST_NODE_ROOT,Condition(true))));

	this->mpiSimulator=new MPISimulator(this->commManager,this->mpiTree);

	this->protocolGen=new ProtocolGenerator(this->mpiTree,this->commManager->getParamRoleMapping());
}


bool MPITypeCheckingConsumer::HandleTopLevelDecl(DeclGroupRef d)
{
	DeclGroupRef::iterator it;


	for( it = d.begin(); it != d.end(); it++)
	{
		this->TraverseDecl(*it);

		if(isa<VarDecl>(*it)){
			VarDecl *var=cast<VarDecl>(*it);
			string varName=var->getDeclName().getAsString();
			cout<<"Find the var "<<varName<<endl;

			this->commManager->insertVarName(varName);
		}

		if(isa<FunctionDecl>(*it)){
			FunctionDecl *func=cast<FunctionDecl>(*it);
			if(func->isMain()){
				this->mainFunc=func;
			}
		}

	}

	return true;
}


void MPITypeCheckingConsumer::HandleTranslationUnit(ASTContext &Ctx) {
	cout<<"all the parts have been parsed!"<<endl;

	int numOfErrs=ci->getDiagnosticClient().getNumErrors();

	if(numOfErrs>0){
		throw exception();
	}


	this->visitStart=true;

	this->VisitFunctionDecl(this->mainFunc);

	//after the main function has been visited, 
	//the comm tree and roles will have been constructed,
	//and the traversal of the comm tree can start
	this->mpiSimulator->simulate();

	this->protocolGen->generateTheProtocols();

}




bool MPITypeCheckingConsumer::checkWhetherTheDeclHasBeenVisitedBefore(FunctionDecl *decl){
	list<string>::iterator findIt=find(funcsList.begin(),funcsList.end(),decl->getQualifiedNameAsString());

	if(findIt!=funcsList.end())
		return true;

	else
		return false;
}



void MPITypeCheckingConsumer::analyzeDecl(FunctionDecl *funcDecl){

	if (this->checkWhetherTheDeclHasBeenVisitedBefore(funcDecl))
	{
		string errInfo=	"The decl "+decl2str(&ci->getSourceManager(),ci->getLangOpts(),funcDecl)
			+ " has already started in the past and has NOT finished yet! A possible deadlock is detected. Program analysis stops here.";

		throw new MPI_TypeChecking_Error(errInfo);

	}


	//analyze the body of the function
	/***********************************************************************/

	if(funcDecl->hasBody()){
		this->funcsList.push_back(funcDecl->getQualifiedNameAsString());

		cout<<"add "<<funcDecl->getQualifiedNameAsString()<<" to list "<<endl;
	}


	this->VisitFunctionDecl(funcDecl);

}

void MPITypeCheckingConsumer::removeFuncFromList(){
	if(!this->funcsList.empty()){
		string popped=this->funcsList.back();
		cout<<"The decl with name "<<popped<<" is going to be removed."<<endl;
		this->funcsList.pop_back();
	}
}


string MPITypeCheckingConsumer::getVarInIncExpr(Expr *inc){
	if (isa<UnaryOperator>(inc)){
		UnaryOperator *uOP=cast<UnaryOperator>(inc);

		return expr2str(&ci->getSourceManager(),ci->getLangOpts(),uOP->getSubExpr());
	}

	if (isa<BinaryOperator>(inc))
	{
		BinaryOperator *binOP=cast<BinaryOperator>(inc);
		Expr *lhs=binOP->getLHS();

		return expr2str(&ci->getSourceManager(),ci->getLangOpts(),lhs);
	}

	return "";
}

bool MPITypeCheckingConsumer::isChangingByOneUnit(Expr *inc){
	if (isa<UnaryOperator>(inc)){
		UnaryOperator *uOP=cast<UnaryOperator>(inc);

		return uOP->isIncrementDecrementOp();
	}

	if (isa<BinaryOperator>(inc))
	{
		BinaryOperator *binOP=cast<BinaryOperator>(inc);
		Expr *lhs=binOP->getLHS();
		Expr *rhs=binOP->getRHS();
		string op=binOP->getOpcodeStr();

		if(binOP->isCompoundAssignmentOp()){
			if(op!="+=" && op!="-=")
				return false;

			APSInt result;
			if(rhs->EvaluateAsInt(result,ci->getASTContext())){
				int num=atoi(result.toString(10).c_str());
				if(num==1)
					return true;
			}
		}
	}

	return false;
}


void MPITypeCheckingConsumer::handleLoop(){
	CommNode *theLoopNode=this->mpiSimulator->getCurNode();
	CommNode *curP=theLoopNode->getParent();
	if (curP->isMaster())
	{
		theLoopNode->setInfo("LOOP_"+convertIntToStr(labelIndex++));
		theLoopNode->setMaster();
		MPINode *theLoopMPINode;
		if (theLoopNode->getNodeType()==ST_NODE_FOREACH)		
			theLoopMPINode=new MPIForEachNode((ForEachNode*)theLoopNode);
		else
			theLoopMPINode=new MPINode(theLoopNode);

		this->mpiTree->insertNode(theLoopMPINode);

		this->mpiSimulator->insertPosAndMPINodeTuple(theLoopNode->getPosIndex(),theLoopMPINode);
	}
}





string decl2str(SourceManager *sm, LangOptions lopt,Decl *d) {
	clang::SourceLocation b(d->getLocStart()), _e(d->getLocEnd());
	clang::SourceLocation e(clang::Lexer::getLocForEndOfToken(_e, 0, *sm, lopt));
	return std::string(sm->getCharacterData(b),
		sm->getCharacterData(e)-sm->getCharacterData(b));

}

string decl2str(SourceManager *sm, LangOptions lopt,const Decl *d) {
	clang::SourceLocation b(d->getLocStart()), _e(d->getLocEnd());
	clang::SourceLocation e(clang::Lexer::getLocForEndOfToken(_e, 0, *sm, lopt));
	return std::string(sm->getCharacterData(b),
		sm->getCharacterData(e)-sm->getCharacterData(b));

}


string stmt2str(SourceManager *sm, LangOptions lopt,Stmt *stmt) {
	SourceLocation b(stmt->getLocStart()), _e(stmt->getLocEnd());
	SourceLocation e(Lexer::getLocForEndOfToken(_e, 0, *sm, lopt));
	return string(sm->getCharacterData(b),
		sm->getCharacterData(e)-sm->getCharacterData(b));

}

string stmt2str(SourceManager *sm, LangOptions lopt,const Stmt *stmt) {
	SourceLocation b(stmt->getLocStart()), _e(stmt->getLocEnd());
	SourceLocation e(Lexer::getLocForEndOfToken(_e, 0, *sm, lopt));
	return string(sm->getCharacterData(b),
		sm->getCharacterData(e)-sm->getCharacterData(b));

}

string expr2str(SourceManager *sm, LangOptions lopt,clang::Expr *expr){


	SourceLocation b(expr->getLocStart()), _e(expr->getLocEnd());

	SourceLocation e(Lexer::getLocForEndOfToken(_e, 0, *sm, lopt));

	const char* endChar= sm->getCharacterData(e);
	int offset=0;
	endChar--;

	while(*endChar!=',' && *endChar!='(' && *endChar!=';'
		&& *endChar!='='){
		offset++;
		endChar--;
	}


	string out=string(endChar+1,offset);

	return delSpaces(out);

}


string delSpaces(string &str){
	str.erase(std::remove(str.begin(), str.end(), ' '), str.end());
	str.erase(std::remove(str.begin(), str.end(), '\t'), str.end());
	str.erase(std::remove(str.begin(), str.end(), '\n'), str.end());
	return str;
}


void checkIdTable(clang::CompilerInstance *ci){

	clang::IdentifierTable *idTable =&(ci->getASTContext().Idents);
	clang::IdentifierTable::iterator it;
	for (it=idTable->begin(); it!=idTable->end(); ++it){
		clang::StringRef str=it->getKey();

		cout << "the id is: "<<str.str() << endl;

		clang::IdentifierInfo *idInfo;
		idInfo = &idTable->get(str);

		//cout << "the len is "<< idInfo->getLength()<<" and "<< (idInfo->getTokenID()) <<endl;

	}
}




