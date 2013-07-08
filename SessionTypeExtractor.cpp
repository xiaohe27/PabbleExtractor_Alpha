// SessionTypeExtractor.cpp : Defines the entry point for the console application.
//

/********************************************************
* 
*****************************************************************************/


/******************************************************************************
*
*****************************************************************************/
#include "MPITypeCheckingConsumer.h"
using namespace std;



int main()
{
	using clang::CompilerInstance;
	using clang::TargetOptions;
	using clang::TargetInfo;
	using clang::FileEntry;
	using clang::Token;
	using clang::ASTContext;
	using clang::ASTConsumer;
	using clang::Parser;
	using clang::DiagnosticOptions;
	using clang::TextDiagnosticPrinter;
	using clang::IdentifierTable;

	CompilerInstance ci;
	DiagnosticOptions diagnosticOptions;
	TextDiagnosticPrinter *pTextDiagnosticPrinter =
		new TextDiagnosticPrinter(
		llvm::outs(),
		&diagnosticOptions,
		true);

	ci.createDiagnostics(pTextDiagnosticPrinter,false);

	llvm::IntrusiveRefCntPtr<TargetOptions> pto( new TargetOptions());
	pto->Triple = llvm::sys::getDefaultTargetTriple();
	TargetInfo *pti = TargetInfo::CreateTargetInfo(ci.getDiagnostics(), pto.getPtr());
	ci.setTarget(pti);

	ci.createFileManager();
	ci.createSourceManager(ci.getFileManager());
	ci.createPreprocessor();


	/*********************************************************************
	vector<string> inclPair=ci.getPreprocessorOpts().Includes;
	cout<<"the default includes are : "<<endl;
	for (vector<string>::iterator it = inclPair.begin(); it !=inclPair.end() ; it++)
	{
		cout << *it << endl;
	}
	/**********************************************************************/



	ci.getPreprocessorOpts().UsePredefines = true;


	llvm::IntrusiveRefCntPtr<clang::HeaderSearchOptions> hso( new clang::HeaderSearchOptions());
	HeaderSearch headerSearch(	hso, 
		ci.getFileManager(),
		ci.getDiagnostics(),
		ci.getLangOpts(),
		pti);



	/**
	Platform specific code start
	**/
	hso->AddPath(StringRef("A:/MPI_SessionType_Extractor/SessionTypeExtractor4MPI/MPI/mpich2"),
		clang::frontend::Quoted,
		false,
		false);



	hso->AddPath(StringRef("S:/MinGW/include"),
		clang::frontend::Angled,
		false,
		false);


	hso->AddPath(StringRef("S:/CLANG/SW/build4VS11/lib/clang/3.4/include/"),
		clang::frontend::Angled,
		false,
		false);

	
	
	/**
	Platform specific code end
	**/

	InitializePreprocessor(ci.getPreprocessor(), 
		ci.getPreprocessorOpts(),
		*hso,
		ci.getFrontendOpts());


//	Builtin::Context builtInContext;
//	builtInContext.InitializeBuiltins(ci.getPreprocessor().getIdentifierTable(),ci.getLangOpts()) ;

	/**********************************************************************
	inclPair=ci.getPreprocessorOpts().Includes;
	cout<<"the default includes are : "<<endl;
	for (vector<string>::iterator it = inclPair.begin(); it !=inclPair.end() ; it++)
	{
		cout << *it << endl;
	}
	/**********************************************************************/




	MPITypeCheckingConsumer *astConsumer = new MPITypeCheckingConsumer(&ci);
	ci.setASTConsumer(astConsumer);


	ci.createASTContext();



	ci.createSema(clang::TU_Complete, NULL);



	//read from the mpi src file

	//	onst FileEntry *fileIn = fileMgr.getFile(argv[1]);

	/////////////////////////////////////////


	const FileEntry *pFile = ci.getFileManager().getFile("test.c");
	ci.getSourceManager().createMainFileID(pFile);






	try{

		ci.getDiagnosticClient().BeginSourceFile(ci.getLangOpts(),
			&ci.getPreprocessor());


		clang::ParseAST(ci.getSema());

		ci.getDiagnosticClient().EndSourceFile();

		ci.getASTContext().Idents.PrintStats();


	}


	catch(FunctionRecursionError* funcErr){
		funcErr->printErrInfo();
	}

	catch(...){cout<<"default error occurs";}


	//checkIdTable(&ci);

	/*Delete the unused resources: start.
	***********************************************************************************/


	return 0;
}


