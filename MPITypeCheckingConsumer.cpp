#include "MPITypeCheckingConsumer.h"


using namespace clang;
using namespace std;
using namespace llvm;



MPITypeCheckingConsumer::MPITypeCheckingConsumer(CompilerInstance *ci) {
	this->ci=ci;
	this->visitStart=false;
}

void MPITypeCheckingConsumer::HandleTranslationUnit(ASTContext &Ctx) {
	cout<<"all the parts have been parsed!"<<endl;
	this->visitStart=true;

	this->VisitDecl(this->mainFunc);

}


bool MPITypeCheckingConsumer::HandleTopLevelDecl( clang::DeclGroupRef d)
{
	using clang::Stmt;
	using clang::CFG;


	clang::DeclGroupRef::iterator it;


	for( it = d.begin(); it != d.end(); it++)
	{
		this->TraverseDecl(*it);

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



void MPITypeCheckingConsumer::analyzeDecl(FunctionDecl *funcDecl, CallExpr *op){

	if (this->checkWhetherTheDeclHasBeenVisitedBefore(funcDecl))
	{
		string errInfo=	"The decl "+decl2str(&ci->getSourceManager(),ci->getLangOpts(),funcDecl)
			+ " has already started in the past and has NOT finished yet! A possible deadlock is detected. Program analysis stops here.";

		throw new FunctionRecursionError(errInfo);

	}


		//analyze the body of the function
		/***********************************************************************/

		if(funcDecl->hasBody()){
			this->funcsList.push_back(funcDecl->getQualifiedNameAsString());

			cout<<"add "<<funcDecl->getQualifiedNameAsString()<<" to list "<<endl;
		}


		this->VisitFunctionDecl(funcDecl);

}






string decl2str(SourceManager *sm, LangOptions lopt,clang::Decl *d) {
	clang::SourceLocation b(d->getLocStart()), _e(d->getLocEnd());
	clang::SourceLocation e(clang::Lexer::getLocForEndOfToken(_e, 0, *sm, lopt));
	return std::string(sm->getCharacterData(b),
		sm->getCharacterData(e)-sm->getCharacterData(b));

}

string stmt2str(SourceManager *sm, LangOptions lopt,clang::Stmt *stmt) {
	clang::SourceLocation b(stmt->getLocStart()), _e(stmt->getLocEnd());
	clang::SourceLocation e(clang::Lexer::getLocForEndOfToken(_e, 0, *sm, lopt));
	return std::string(sm->getCharacterData(b),
		sm->getCharacterData(e)-sm->getCharacterData(b));

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





