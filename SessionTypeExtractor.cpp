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

	ci.createDiagnostics(0,true);

	llvm::IntrusiveRefCntPtr<TargetOptions> pto( new TargetOptions());
	pto->Triple = llvm::sys::getDefaultTargetTriple();
	TargetInfo *pti = TargetInfo::CreateTargetInfo(ci.getDiagnostics(), pto.getPtr());
	ci.setTarget(pti);

	ci.createFileManager();
	ci.createSourceManager(ci.getFileManager());
	ci.createPreprocessor();
	ci.getPreprocessorOpts().UsePredefines = false;
	ci.createASTContext();

	MPITypeCheckingConsumer *astConsumer = new MPITypeCheckingConsumer(&ci);
	ci.setASTConsumer(astConsumer);

	ci.createSema(clang::TU_Complete, NULL);



	//read from the mpi src file

//	onst FileEntry *fileIn = fileMgr.getFile(argv[1]);

	/////////////////////////////////////////


	const FileEntry *pFile = ci.getFileManager().getFile("test.c");
	ci.getSourceManager().createMainFileID(pFile);
	clang::ParseAST(ci.getSema());
	ci.getASTContext().Idents.PrintStats();

	//checkIdTable(&ci);

	return 0;
}


