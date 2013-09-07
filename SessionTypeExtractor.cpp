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


const string LIB_FILE_PATH="LIB_SEARCH_PATH.txt";
bool STRICT=true;
vector<string> srcFilesPaths;
vector<string> searchPaths;

//windows specific. 
string getFileName(string path, bool withExtension){

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

	printf( "  Path extracted with _splitpath_s:\n" );
	printf( "  Drive: %s\n", drive );
	printf( "  Dir: %s\n", dir );
	printf( "  Filename: %s\n", fname );
	printf( "  Ext: %s\n", ext );

	string name(fname);

	if (withExtension)
		name+=ext;

	return name;

	/*
	Linux equivaluent.
	/////////////////////////////////////////
	char *resolvedPath;
	char buf[PATH_MAX + 1];
	char *ret=realpath(path.c_str(), buf);

	if (ret) {
	char *baseName=basename(buf);

	string name(baseName);

	if(!withExtension){
	unsigned found = name.find_last_of(".");
	if (found!=string::npos)	
	return name.substr(0,found);

	else return name;
	}
	else
	return name;

	} else {
	cerr << "Not resolvable path!" <<endl;
	exit(EXIT_FAILURE);
	}
	*/

}

void parseArgs(int argc, char *argv[]){
	if (argc % 2 != 1) {
		cout<<"Not enough args."<<endl;
	} else { 

		std::cout << argv[0];
		for (int i = 1; i < argc; i+=2) { 
			if (i + 1 != argc) {// Check that we haven't finished parsing already
				if (string(argv[i]) == "-n") {
					N = atoi(argv[i + 1]);

				} else if (string(argv[i]) == "-lib"){
					string singleLibPath;
					stringstream stream(argv[i+1]);
					while( getline(stream, singleLibPath, ';') ){
						searchPaths.push_back(singleLibPath);
					}

				} else if (string(argv[i]) == "-strict") {
					string ans = argv[i + 1];
					transform(ans.begin(), ans.end(), ans.begin(), ::tolower);
					if (ans=="true" || ans=="y" || ans=="yes")
						STRICT=true;

					else 
						STRICT=false;
				} else if(string(argv[i]) == "-src"){
					string srcFileListStr=argv[i + 1];

					cout <<"\n"<< srcFileListStr <<endl;

					string singlePath;
					stringstream stream(srcFileListStr);
					while( getline(stream, singlePath, ';') ){
						srcFilesPaths.push_back(singlePath);
					}
				} 

				else {
					std::cout << "\nNot enough or invalid arguments, please try again.\n";

					throw exception();
				}

			}
		}

	}


}

void readConfig(){
	ifstream libFile(LIB_FILE_PATH, fstream::binary);
	string singleLibPath;
	while( getline(libFile, singleLibPath, ';') ){
		searchPaths.push_back(singleLibPath);
	}
}

int main(int argc, char *argv[])
{
	clock_t initT, finalT;
	initT=clock();

	readConfig();//read args from config file

	parseArgs(argc,argv);//the command line args can overwrite the config args

	try{
		if (srcFilesPaths.size()==0)
			throw MPI_TypeChecking_Error("No MPI src code provided!");


		ofstream outputFile("Protocol.txt");
		outputFile.clear();
		outputFile.close();

		ofstream debugFile("Debug.txt");
		debugFile.clear();
		debugFile.close();


		///////////////////////////////////////////////////////////////////////////
		CompilerInstance ci;
		DiagnosticOptions *diagnosticOptions=new DiagnosticOptions();
		TextDiagnosticPrinter *pTextDiagnosticPrinter =
			new TextDiagnosticPrinter(
			llvm::outs(),
			diagnosticOptions,
			true);


		string clangArgs1= "-w";
		// Arguments to pass to the clang frontend
		vector<const char *> args4Clang;
		args4Clang.push_back(clangArgs1.c_str());

		// The compiler invocation needs a DiagnosticsEngine so it can report problems

		llvm::IntrusiveRefCntPtr<clang::DiagnosticIDs> DiagID(new clang::DiagnosticIDs());
		clang::DiagnosticsEngine Diags(DiagID,diagnosticOptions);

		llvm::OwningPtr<clang::CompilerInvocation> CI(new clang::CompilerInvocation);
		clang::CompilerInvocation::CreateFromArgs(*CI, &args4Clang[0], 
			&args4Clang[0] + args4Clang.size(), Diags);

		///////////////////////////////////////////////////////////////////////////
		ci.setInvocation(CI.take());


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
		if (searchPaths.size()==0)
			throw new MPI_TypeChecking_Error("No lib search path is provided!!!");

		for (int i = 0; i < searchPaths.size(); i++)
		{
			hso->AddPath(searchPaths.at(i),
				clang::frontend::Angled,
				false,
				false);
		}

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



		string filePath=srcFilesPaths.at(0);
		MPI_FILE_NAME=getFileName(filePath,false);

		string updatedMainFilePath=filePath+"_ExplictIncluding";
		//create a bakup modifiable file on which we perform the analysis
		ifstream inputFileStream (filePath, fstream::binary);
		ofstream outputFileStream (updatedMainFilePath, fstream::trunc|fstream::binary);

		if(!inputFileStream)
			throw new MPI_TypeChecking_Error("Cannot find MPI source file!");

		outputFileStream<<"\n/*explict including the import list*/\n";
		//the first is main file and the others are following.
		for (int i = 1; i < srcFilesPaths.size(); i++)
		{
			outputFileStream << "#include \""<< srcFilesPaths.at(i) << "\"\n";
		}
		outputFileStream <<"\n/*End of importing the other src files*/\n\n";

		outputFileStream << inputFileStream.rdbuf ();
		outputFileStream.close();

		//read from the mpi src file
		const FileEntry *pFile = ci.getFileManager().getFile(updatedMainFilePath);
		ci.getSourceManager().createMainFileID(pFile);
		/////////////////////////////////////////

		ci.getDiagnosticClient().BeginSourceFile(ci.getLangOpts(),
			&ci.getPreprocessor());


		clang::ParseAST(ci.getSema());

		ci.getDiagnosticClient().EndSourceFile();

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


	/**********************************************************************************
	*Delete the unused resources: start.
	***********************************************************************************/

	return 0;
}


