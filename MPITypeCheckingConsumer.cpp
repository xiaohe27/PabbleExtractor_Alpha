#include "MPITypeCheckingConsumer.h"

#define DEFAULT_NUM_OF_PROCESSES 100


using namespace clang;
using namespace std;
using namespace llvm;



MPITypeCheckingConsumer::MPITypeCheckingConsumer(CompilerInstance *ci, int numOfProc){
	this->ci=ci;
	this->visitStart=false;
	this->numOfProcs=numOfProc;
	this->commManager=new CommManager(ci, numOfProc);
}

MPITypeCheckingConsumer::MPITypeCheckingConsumer(CompilerInstance *ci) {
	MPITypeCheckingConsumer(ci, DEFAULT_NUM_OF_PROCESSES);
}

void MPITypeCheckingConsumer::HandleTranslationUnit(ASTContext &Ctx) {
	cout<<"all the parts have been parsed!"<<endl;
	
	int numOfErrs=ci->getDiagnosticClient().getNumErrors();

	if(numOfErrs>0){
		throw exception();
	}

	this->visitStart=true;

	this->VisitDecl(this->mainFunc);

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




string decl2str(SourceManager *sm, LangOptions lopt,clang::Decl *d) {
	clang::SourceLocation b(d->getLocStart()), _e(d->getLocEnd());
	clang::SourceLocation e(clang::Lexer::getLocForEndOfToken(_e, 0, *sm, lopt));
	return std::string(sm->getCharacterData(b),
		sm->getCharacterData(e)-sm->getCharacterData(b));

}


string stmt2str(SourceManager *sm, LangOptions lopt,clang::Stmt *stmt) {
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

	while(*endChar!=',' && *endChar!='('){
		offset++;
		endChar--;
	}
	

	string out=string(endChar+1,offset);

	return delSpaces(out);

}


string delSpaces(string &str){
   str.erase(std::remove(str.begin(), str.end(), ' '), str.end());
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




