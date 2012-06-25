#pragma once

class DmapCmdLine {
	idCmdArgs dmapArgs;
	bool argsOk;
public:
	DmapCmdLine(int argc, char **argv);
	void run();
};
