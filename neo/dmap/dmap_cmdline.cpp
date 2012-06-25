#include "../idlib/precompiled.h"
#include "../tools/compilers/dmap/dmap.h"
#include "dmap_cmdline.h"

void Dmap( const idCmdArgs &args );

static void help();

DmapCmdLine::DmapCmdLine(int argc, char **argv)
	: argsOk(true)
{
	bool haveFilename = false;
	
	dmapArgs.AppendArg("dmap");
	for(int i = 1; i < argc; i++) {
		if(!strcmp(argv[i], "--lightCarve")) {
			dmapArgs.AppendArg("lightCarve");
		} else if(!strcmp(argv[i], "--no-carve")) {
			dmapArgs.AppendArg("noCarve");
		} else if(!strcmp(argv[i], "--no-clip-sides")) {
			dmapArgs.AppendArg("noClipSides");
		} else if(!strcmp(argv[i], "--no-cm")) {
			dmapArgs.AppendArg("noCM");
		} else if(!strcmp(argv[i], "--no-curves")) {
			dmapArgs.AppendArg("noCurves");
		} else if(!strcmp(argv[i], "--no-flood")) {
			dmapArgs.AppendArg("noFlood");
		} else if(!strcmp(argv[i], "--no-opt")) {
			dmapArgs.AppendArg("noOpt");
		} else if(!strcmp(argv[i], "--no-t-junc")) {
			dmapArgs.AppendArg("noTJunc");
		} else if(!strcmp(argv[i], "--verbose") || !strcmp(argv[i], "-v")) {
			dmapArgs.AppendArg("v");
		} else if(!strcmp(argv[i], "--verbose-entities")) {
			dmapArgs.AppendArg("verboseEntities");
		} else if(!strcmp(argv[i], "--shadow-opt")) {
			if(i+1 < argc) {
				dmapArgs.AppendArg("shadowOpt");
				dmapArgs.AppendArg(argv[i+1]);
			} else {
				argsOk = false;
			}
		} else {
			if(!haveFilename) {
				dmapArgs.AppendArg(argv[i]);
				haveFilename = true;
			} else {
				argsOk = false;
			}
		}
	}

	argsOk = argsOk && haveFilename;
}

void DmapCmdLine::run()
{
	if(!argsOk) return help();
	Dmap(dmapArgs);
}

void help()
{
	printf("Usage:\tdmap [options] filename\n\n");
	
	printf("Options: --light-carve: Split polygons along light volume edges\n");
	printf("         --no-carve: Doesn't cut any surfaces as if they had the noFragment global keyword\n");
	printf("         --no-clip-sides: Doesn't clip overlapping solid brushes\n");
	printf("         --no-cm: Do not create collission map\n");
	printf("         --no-flood: Skips the flooding of the map allowing it to have leaks\n");
	printf("         --no-opt: Doesn't merge/remove redundant polygons\n");
	printf("         --no-t-junc: Doesn't create T-Junctions. Forces --no-opt\n");
	printf("         -v, --verbose: Shows additional infromation while compiling\n");
	printf("         --verbose-entities: Shows additional information on entities like removal of degenerate polygons\n");
	printf("         --shadow-opt <n>: Sets the static shadow computation optimization type:\n");
	printf("             0: No optimization\n");
	printf("             1: SO_MERGE_SURFACES (default)\n");
	printf("             2: SO_CLIP_OCCLUDED\n");
	printf("             3: SO_CLIP_OCCLUDERS\n");
	printf("             4: SO_CLIP_SILS\n");
	printf("             5: SO_SIL_OPTIMIZE\n\n");
	
	printf("No AAS support in this executable.\n");
	printf("Extracted from the Doom 3 sourcecode, licensed under the GNU GPL v3.\n");
	printf("See <https://github.com/TTimo/doom3.gpl/blob/master/COPYING.txt>\n");
	printf("Information for this help text was taken from modwiki.net.\n");
	printf("See <http://modwiki.net/wiki/Dmap_(console_command)>.\n");
}