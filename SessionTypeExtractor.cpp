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
using namespace clang;
using namespace llvm;

int numOfProcesses;
string filePath;


int main(int argc, char *argv[])
{
	if(argc==2){
		filePath=argv[1];
		InitEndIndex=100;
		cout<<"The path of src file is "<<argv[1]<<endl;
	}
	else if(argc==3){	
		cout<<"There are two args"<<endl;

		filePath=argv[1];
		cout<<"The path of src file is "<<argv[1]<<endl;

		numOfProcesses=atoi(argv[2]);

		InitEndIndex=numOfProcesses;

		cout<<"There are "<<numOfProcesses<<" processes!"<<endl;
	}

	else{
		cerr <<"The argument number should be one (only the mpi src code is provided)";
		cerr <<"\tor two (if both the mpi src code and number of processes are provided.)"<<endl;
		throw exception();
	}

	
	ofstream outputFile("Protocol.txt");
	outputFile.clear();
	outputFile.close();

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



	ci.getPreprocessorOpts().UsePredefines = true;


	IntrusiveRefCntPtr<clang::HeaderSearchOptions> hso( new clang::HeaderSearchOptions());
	HeaderSearch headerSearch(	hso, 
		ci.getFileManager(),
		ci.getDiagnostics(),
		ci.getLangOpts(),
		pti);



	/******************************************************************************************
	Platform specific code start
	****************************************************************************************/
	hso->AddPath(StringRef("S:/MinGW/MPI/include"),
		clang::frontend::Quoted,
		false,
		false);



	hso->AddPath(StringRef("S:/MinGW/include"),
		clang::frontend::Angled,
		false,
		false);


	hso->AddPath(StringRef("S:/CLANG/SW/build4VS11/lib/clang/3.4/include"),
		clang::frontend::Angled,
		false,
		false);



	/******************************************************************************************
	Platform specific code end
	****************************************************************************************/

	InitializePreprocessor(ci.getPreprocessor(), 
		ci.getPreprocessorOpts(),
		*hso,
		ci.getFrontendOpts());



	MPITypeCheckingConsumer *astConsumer;

	if(numOfProcesses!=0){
	astConsumer= new MPITypeCheckingConsumer(&ci,numOfProcesses);
	}
	
	else{
	astConsumer= new MPITypeCheckingConsumer(&ci);
	}

	ci.setASTConsumer(astConsumer);


	ci.createASTContext();


	ci.createSema(clang::TU_Complete, NULL);


	//read from the mpi src file
	const FileEntry *pFile = ci.getFileManager().getFile(argv[1]);
	ci.getSourceManager().createMainFileID(pFile);
	/////////////////////////////////////////



	try{

		ci.getDiagnosticClient().BeginSourceFile(ci.getLangOpts(),
			&ci.getPreprocessor());


		clang::ParseAST(ci.getSema());

		ci.getDiagnosticClient().EndSourceFile();

		ci.getASTContext().Idents.PrintStats();


		//print the tree
		astConsumer->printTheTree();


		outputFile.close();
	}


	catch(MPI_TypeChecking_Error* funcErr){
		funcErr->printErrInfo();

		writeToFile(funcErr->errInfo);
	}

	catch(...){cout<<"There exists compile time error or unknown runtime error, please fix the error(s) first.";}


	//checkIdTable(&ci);

	/**********************************************************************************
	*Delete the unused resources: start.
	***********************************************************************************/


	return 0;
}


