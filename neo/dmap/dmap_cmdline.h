#pragma once

class DmapCmdLine {
	idCmdArgs dmapArgs;
	bool argsOk;
	char *exename;
public:
	DmapCmdLine(int argc, char **argv);
	void run();
};
