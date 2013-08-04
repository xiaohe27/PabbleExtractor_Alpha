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

bool strict=false;

void parseArgs(int argc, char *argv[]){
		if (argc < 3) {
		cout<<"Not enough args."<<endl;
	} else { 

		std::cout << argv[0];
		for (int i = 1; i < argc; i+=2) { 
			if (i + 1 != argc) {// Check that we haven't finished parsing already
				if (string(argv[i]) == "-f") {
					// We know the next argument *should* be the filename:
					filePath = argv[i + 1];
					cout<<"The path of src file is "<<filePath<<endl;
				} else if (string(argv[i]) == "-n") {
					numOfProcesses = atoi(argv[i + 1]);
					InitEndIndex=numOfProcesses;
					cout<<"There are "<<numOfProcesses<<" processes!"<<endl;
				} else if (string(argv[i]) == "-strict") {
					string ans = argv[i + 1];
					transform(ans.begin(), ans.end(), ans.begin(), ::tolower);
					if (ans=="true" || ans=="y" || ans=="yes")
						strict=true;

					else 
						strict=false;
				} else {
					std::cout << "\nNot enough or invalid arguments, please try again.\n";

					throw exception();
				}
				std::cout << argv[i] << " ";
			}
		}

	}
}


int main(int argc, char *argv[])
{
	clock_t initT, finalT;
	initT=clock();

	filePath="";

	parseArgs(argc,argv);

	if (filePath=="")
	throw MPI_TypeChecking_Error("No MPI src code provided!");


	ofstream outputFile("A:/MPI_SessionType_Extractor/SessionTypeExtractor4MPI/Debug/Protocol.txt");
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
	const FileEntry *pFile = ci.getFileManager().getFile(filePath);
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


	///////////////////////////////////////////////////////////////////////////
	finalT=clock()-initT;
	double timePassed=(double)finalT / ((double)CLOCKS_PER_SEC);
	string timeStr="\nThe program used "+convertDoubleToStr(timePassed)
		+" seconds to generate the protocol.\n";
	writeToFile(timeStr);

	cout<<timeStr<<endl;

	//checkIdTable(&ci);

	/**********************************************************************************
	*Delete the unused resources: start.
	***********************************************************************************/


	return 0;
}


