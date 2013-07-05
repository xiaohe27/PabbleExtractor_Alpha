#include "MPITypeCheckingConsumer.h"


using namespace clang;
using namespace std;
using namespace llvm;



MPITypeCheckingConsumer::MPITypeCheckingConsumer(CompilerInstance *ci) {
	this->ci=ci;
}



bool MPITypeCheckingConsumer::HandleTopLevelDecl( clang::DeclGroupRef d)
{
	using clang::Stmt;
	using clang::CFG;

	
	clang::DeclGroupRef::iterator it;
	for( it = d.begin(); it != d.end(); it++){
	//myDeclVisitor::VisitDecl(*it);
	}


	for( it = d.begin(); it != d.end(); it++)
	{
//		this->TraverseDecl(*it);
		

		clang::FunctionDecl *fd = llvm::dyn_cast<clang::FunctionDecl>(*it);
		if(!fd)
		{
			continue;
		}

		if(fd->getName().compare("main")==0){
			cout << "Found the main!"<<endl;
			this->TraverseDecl(fd);

		}
		

	}
	return true;
}


/*
bool MPITypeCheckingConsumer::TraverseFunctionDecl(FunctionDecl *fd){
	//		const string stat=fd->hasBody()?" has ":"has NO ";
	//	cout <<"function " << fd->getName().data() << stat<< "function body" <<endl;

	cout << "Find function "<<fd->getName().data()<<endl;

	if(fd->hasBody())
		cout << "The body of the function "<<fd->getName().data()<<" is \n" <<
		decl2str(&ci->getSourceManager(),ci->getLangOpts(),fd) <<endl;



	Stmt *s0=fd->getBody();
	//cout << "The statement is " << s0->getStmtClassName() << endl;

	this->TraverseStmt(s0);


	return true;
}

*/















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





