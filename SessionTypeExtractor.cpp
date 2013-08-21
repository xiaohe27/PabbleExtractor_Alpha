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

string filePath;
bool STRICT=true;
string APP_PATH="";

string getFileName(string path){

	char drive[_MAX_DRIVE];
	char dir[_MAX_DIR];
	char fname[_MAX_FNAME];
	char ext[_MAX_EXT];
	errno_t err;



	err = _splitpath_s( path.c_str(), drive, _MAX_DRIVE, dir, _MAX_DIR, fname,
		_MAX_FNAME, ext, _MAX_EXT );
	if (err != 0)
	{
		printf("Error splitting the path. Error code %d.\n", err);
		exit(1);
	}

	printf( "Path extracted with _splitpath_s:\n" );
	printf( "  Drive: %s\n", drive );
	printf( "  Dir: %s\n", dir );
	printf( "  Filename: %s\n", fname );
	printf( "  Ext: %s\n", ext );

	string name(fname);
	return name;
}

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
					N = atoi(argv[i + 1]);

					cout<<"There are "<<N<<" processes!"<<endl;
				} else if (string(argv[i]) == "-strict") {
					string ans = argv[i + 1];
					transform(ans.begin(), ans.end(), ans.begin(), ::tolower);
					if (ans=="true" || ans=="y" || ans=="yes")
						STRICT=true;

					else 
						STRICT=false;
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


	ofstream outputFile("Protocol.txt");
	outputFile.clear();
	outputFile.close();

	ofstream debugFile("Debug.txt");
	debugFile.clear();
	debugFile.close();

	///////////////////////////////////////////////////////////////////////////
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

	if(N!=0){
		astConsumer= new MPITypeCheckingConsumer(&ci,N);
	}

	else{
		astConsumer= new MPITypeCheckingConsumer(&ci);
	}

	ci.setASTConsumer(astConsumer);


	ci.createASTContext();


	ci.createSema(clang::TU_Complete, NULL);


	try{
		//read from the mpi src file
		const FileEntry *pFile = ci.getFileManager().getFile(filePath);
		ci.getSourceManager().createMainFileID(pFile);
		/////////////////////////////////////////

		MPI_FILE_NAME=getFileName(filePath);

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


