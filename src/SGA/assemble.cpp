//-----------------------------------------------
// Copyright 2009 Wellcome Trust Sanger Institute
// Written by Jared Simpson (js18@sanger.ac.uk)
// Released under the GPL
//-----------------------------------------------
//
// assemble - convert read overlaps into contigs
//
#include <iostream>
#include <fstream>
#include "Util.h"
#include "assemble.h"
#include "SGUtil.h"
#include "SGAlgorithms.h"
#include "SGPairedAlgorithms.h"
#include "SGDebugAlgorithms.h"
#include "Timer.h"


//
// Getopt
//
#define SUBPROGRAM "assemble"
static const char *ASSEMBLE_VERSION_MESSAGE =
SUBPROGRAM " Version " PACKAGE_VERSION "\n"
"Written by Jared Simpson.\n"
"\n"
"Copyright 2009 Wellcome Trust Sanger Institute\n";

static const char *ASSEMBLE_USAGE_MESSAGE =
"Usage: " PACKAGE_NAME " " SUBPROGRAM " [OPTION] ... READSFILE\n"
"Create contigs for the reads in READSFILE. Overlaps are read from PREFIX.ovr. PREFIX defaults to the basename of READSFILE\n"
"\n"
"  -v, --verbose                        display verbose output\n"
"      --help                           display this help and exit\n"
"      -o, --out=FILE                   write the contigs to FILE (default: contigs.fa)"
"      -m, --min-overlap=LEN            only use overlaps of at least LEN. This can be used to filter"
"                                       the overlap set so that the overlap step only needs to be run once."
"      -p, --prefix=FILE                use PREFIX instead of the basename of READSFILE\n"
"      -b, --bubble                     perform bubble removal\n"
"      -t, --trim                       trim terminal branches\n"
"      -c, --close                      perform transitive closure\n"
"\nReport bugs to " PACKAGE_BUGREPORT "\n\n";

namespace opt
{
	static unsigned int verbose;
	static std::string readsFile;
	static std::string prefix;
	static std::string outFile;
	static std::string debugFile;
	static unsigned int minOverlap;
	static bool bTransClose;
	static bool bTrim;
	static bool bBubble;
}

static const char* shortopts = "p:o:m:d:vbtc";

enum { OPT_HELP = 1, OPT_VERSION };

static const struct option longopts[] = {
	{ "verbose",     no_argument,       NULL, 'v' },
	{ "prefix",      required_argument, NULL, 'p' },
	{ "out",         required_argument, NULL, 'o' },
	{ "min-overlap", required_argument, NULL, 'm' },
	{ "debug-file",  required_argument, NULL, 'd' },
	{ "bubble",      no_argument,       NULL, 'b' },
	{ "trim",        no_argument,       NULL, 't' },
	{ "close",       no_argument,       NULL, 'c' },	
	{ "help",        no_argument,       NULL, OPT_HELP },
	{ "version",     no_argument,       NULL, OPT_VERSION },
	{ NULL, 0, NULL, 0 }
};

//
// Main
//
int assembleMain(int argc, char** argv)
{
	Timer* pTimer = new Timer("sga assemble");
	parseAssembleOptions(argc, argv);
	assemble();
	delete pTimer;

	return 0;
}

void assemble()
{
	Timer t("sga assemble");
	StringGraph* pGraph = loadStringGraph(opt::readsFile, opt::prefix + ".ovr", opt::prefix + ".ctn", opt::minOverlap, true);
	pGraph->printMemSize();

	//pGraph->validate();
	//pGraph->writeDot("before.dot");
	
	// Visitor functors
	SGTrimVisitor trimVisit;
	SGIslandVisitor islandVisit;
	SGTransRedVisitor trVisit;
	SGTCVisitor tcVisit;
	SGBubbleVisitor bubbleVisit;
	SGGraphStatsVisitor statsVisit;
	SGDebugEdgeClassificationVisitor edgeClassVisit;
	SGVertexPairingVisitor pairingVisit;
	SGPairedOverlapVisitor pairedOverlapVisit;
	SGPETrustVisitor trustVisit;
	SGGroupCloseVisitor groupCloseVisit;
	SGRealignVisitor realignVisitor;
	SGContainRemoveVisitor containVisit;

	if(!opt::debugFile.empty())
	{
		// Pre-assembly graph stats
		std::cout << "Initial graph stats\n";
		pGraph->visit(statsVisit);

		SGDebugGraphCompareVisitor* pDebugGraphVisit = new SGDebugGraphCompareVisitor(opt::debugFile);
		
		/*
		pGraph->visit(*pDebugGraphVisit);
		while(pGraph->visit(realignVisitor))
			pGraph->visit(*pDebugGraphVisit);
		SGOverlapWriterVisitor overlapWriter("final-overlaps.ovr");
		pGraph->visit(overlapWriter);
		*/
		//pDebugGraphVisit->m_showMissing = true;
		pGraph->visit(*pDebugGraphVisit);
		pGraph->visit(statsVisit);

		delete pDebugGraphVisit;
		//return;
	}
	/*
	if(!opt::positionsFile.empty())
	{
		WARN_ONCE("Using positions file");
		std::cout << "Loading debug positions from file " << opt::positionsFile << "\n";
		std::ifstream reader(opt::positionsFile.c_str());
		std::string line;
		std::string vertID;
		int position;
		int distance;

		while(getline(reader, line))
		{
			std::istringstream ss(line);
			ss >> vertID;
			ss >> position;
			ss >> distance;
			Vertex* pVertex = pGraph->getVertex(vertID);
			if(pVertex != NULL)
				pVertex->dbg_position = position;
		}
	}
	*/

	// Pre-assembly graph stats
	std::cout << "Initial graph stats\n";
	pGraph->visit(statsVisit);

	// Remove containments from the graph
	pGraph->visit(containVisit);
	
	std::cout << "Post-contain removal stats\n";
	pGraph->visit(statsVisit);
	
	/*
	std::cout << "Pairing reads\n";
	pGraph->visit(pairingVisit);

	std::cout << "Processing trust network\n";
	pGraph->visit(trustVisit);

	//std::cout << "Paired overlap distance\n";
	//pGraph->visit(pairedOverlapVisit);

	
	std::cout << "\nPerforming transitive reduction\n";
	pGraph->visit(edgeClassVisit);
	pGraph->visit(trVisit);
	pGraph->visit(edgeClassVisit);
	SGPEConflictRemover conflictVisit;
	pGraph->visit(conflictVisit);
	pGraph->visit(edgeClassVisit);
	pGraph->visit(statsVisit);
	//pGraph->visit(resolveVisit);
	*/

/*
	if(opt::bTrim)
	{
		std::cout << "Performing island/trim reduction\n";
		pGraph->visit(islandVisit);
		pGraph->visit(trimVisit);
		pGraph->visit(statsVisit);
	}
*/
	if(opt::bTransClose)
	{
		pGraph->visit(edgeClassVisit);
		std::cout << "\nPerforming transitive closure\n";
		int max_tc = 10;
		while(pGraph->visit(tcVisit) && --max_tc > 0);
		pGraph->visit(statsVisit);
		pGraph->visit(edgeClassVisit);
	}

	if(opt::bTrim)
	{
		std::cout << "\nPerforming island/trim reduction\n";
		pGraph->visit(islandVisit);
		pGraph->visit(trimVisit);
		pGraph->visit(trimVisit);
		pGraph->visit(trimVisit);
		pGraph->visit(statsVisit);
	}

	/*
	std::cout << "\nPairing vertices\n";
	pGraph->visit(pairingVisit);
	//pGraph->visit(pairedOverlapVisit);
	pGraph->visit(trustVisit);
	*/

	std::cout << "\nPerforming transitive reduction\n";
	pGraph->visit(trVisit);

	pGraph->visit(statsVisit);
	printf("starting simplify\n");
	pGraph->simplify();
	printf("done simplify\n");

	//pGraph->visit(trimVisit);
	//pGraph->simplify();
	
	if(opt::bBubble)
	{
		std::cout << "\nPerforming bubble removal\n";
		// Bubble removal
		while(pGraph->visit(bubbleVisit))
			pGraph->simplify();
		pGraph->visit(statsVisit);
	}

	pGraph->simplify();

	std::cout << "\nFinal graph stats\n";
	pGraph->visit(statsVisit);

#ifdef VALIDATE
	VALIDATION_WARNING("SGA/assemble")
	pGraph->validate();
#endif

	// Write the results
	pGraph->writeDot("final.dot");
	SGFastaVisitor av(opt::outFile);
	pGraph->visit(av);

	delete pGraph;
}

// 
// Handle command line arguments
//
void parseAssembleOptions(int argc, char** argv)
{
	// Set defaults
	opt::outFile = "contigs.fa";
	opt::minOverlap = 0;

	bool die = false;
	for (char c; (c = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1;) 
	{
		std::istringstream arg(optarg != NULL ? optarg : "");
		switch (c) 
		{
			case 'p': arg >> opt::prefix; break;
			case 'o': arg >> opt::outFile; break;
			case 'm': arg >> opt::minOverlap; break;
			case 'd': arg >> opt::debugFile; break;
			case '?': die = true; break;
			case 'v': opt::verbose++; break;
			case 'b': opt::bBubble = true; break;
            case 't': opt::bTrim = true; break;
			case 'c': opt::bTransClose = true; break;
			case OPT_HELP:
				std::cout << ASSEMBLE_USAGE_MESSAGE;
				exit(EXIT_SUCCESS);
			case OPT_VERSION:
				std::cout << ASSEMBLE_VERSION_MESSAGE;
				exit(EXIT_SUCCESS);
		}
	}

	if (argc - optind < 1) 
	{
		std::cerr << SUBPROGRAM ": missing arguments\n";
		die = true;
	} 
	else if (argc - optind > 1) 
	{
		std::cerr << SUBPROGRAM ": too many arguments\n";
		die = true;
	}

	if (die) 
	{
		std::cerr << "Try `" << SUBPROGRAM << " --help' for more information.\n";
		exit(EXIT_FAILURE);
	}

	// Parse the input filenames
	opt::readsFile = argv[optind++];
	
	if(opt::prefix.empty())
		opt::prefix = stripFilename(opt::readsFile);
}
